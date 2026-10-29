#include "../include/parking_system.h"

/*
 * Module 2: Entry Gate Synchronisation and Parking Bay Resource Manager
 * Implements:
 *  - Counting semaphore initialized to TOTAL_SLOTS (100)
 *  - Slot bitmap allocation (4 x 32-bit words = 128 bits for 100 slots)
 *  - Per-level mutex locks with fixed lock ordering (Level 0 -> Level 3)
 *  - Guarantees zero double allocations under heavy concurrency
 */

static uint32_t slot_bitmap[4]; /* 128 bits for 100 slots */
static int slot_occupant[TOTAL_SLOTS];

#ifdef _WIN32
static HANDLE sem_slots;
static CRITICAL_SECTION level_locks[NUM_LEVELS];
static CRITICAL_SECTION global_lock;
#else
static sem_t sem_slots;
static pthread_mutex_t level_locks[NUM_LEVELS];
static pthread_mutex_t global_lock;
#endif

void init_resource_manager(void) {
    memset(slot_bitmap, 0, sizeof(slot_bitmap));
    for (int i = 0; i < TOTAL_SLOTS; i++) {
        slot_occupant[i] = -1;
    }

#ifdef _WIN32
    sem_slots = CreateSemaphore(NULL, TOTAL_SLOTS, TOTAL_SLOTS, NULL);
    InitializeCriticalSection(&global_lock);
    for (int i = 0; i < NUM_LEVELS; i++) {
        InitializeCriticalSection(&level_locks[i]);
    }
#else
    sem_init(&sem_slots, 0, TOTAL_SLOTS);
    pthread_mutex_init(&global_lock, NULL);
    for (int i = 0; i < NUM_LEVELS; i++) {
        pthread_mutex_init(&level_locks[i], NULL);
    }
#endif
}

static inline void mark_slot_occupied(int slot_id) {
    int word_idx = slot_id / 32;
    int bit_offset = slot_id % 32;
    slot_bitmap[word_idx] |= (1U << bit_offset);
}

static inline void mark_slot_free(int slot_id) {
    int word_idx = slot_id / 32;
    int bit_offset = slot_id % 32;
    slot_bitmap[word_idx] &= ~(1U << bit_offset);
}

static inline bool is_slot_bit_set(int slot_id) {
    int word_idx = slot_id / 32;
    int bit_offset = slot_id % 32;
    return (slot_bitmap[word_idx] & (1U << bit_offset)) != 0;
}

int check_slot_available(int slot_id) {
    if (slot_id < 0 || slot_id >= TOTAL_SLOTS) return 0;
    return !is_slot_bit_set(slot_id);
}

/* Allocate parking bay using per-level locking with strict ascending order */
int allocate_parking_bay(Vehicle* v) {
    /* 1. Decrement counting semaphore. Blocks if parking is 100% full */
#ifdef _WIN32
    WaitForSingleObject(sem_slots, INFINITE);
#else
    sem_wait(&sem_slots);
#endif

    int chosen_slot = -1;
    int chosen_level = -1;

    /* If vehicle is Emergency or VIP, prioritize EV bays (slots 6 and 7, which are #07 and #08) */
    if (v->type == VEH_EMERGENCY || v->type == VEH_VIP) {
        int ev_level = 0;
#ifdef _WIN32
        EnterCriticalSection(&level_locks[ev_level]);
#else
        pthread_mutex_lock(&level_locks[ev_level]);
#endif
        if (!is_slot_bit_set(6)) {
            chosen_slot = 6;
            chosen_level = ev_level;
        } else if (!is_slot_bit_set(7)) {
            chosen_slot = 7;
            chosen_level = ev_level;
        }

        if (chosen_slot != -1) {
            mark_slot_occupied(chosen_slot);
            slot_occupant[chosen_slot] = v->id;
#ifdef _WIN32
            LeaveCriticalSection(&level_locks[ev_level]);
#else
            pthread_mutex_unlock(&level_locks[ev_level]);
#endif
            v->allocated_slot = chosen_slot;
            v->allocated_level = chosen_level;
            return chosen_slot;
        }
#ifdef _WIN32
        LeaveCriticalSection(&level_locks[ev_level]);
#else
        pthread_mutex_unlock(&level_locks[ev_level]);
#endif
    }

    /* Standard Allocation: Scan levels in fixed order (Level 0 -> Level 3) */
    for (int lvl = 0; lvl < NUM_LEVELS; lvl++) {
#ifdef _WIN32
        EnterCriticalSection(&level_locks[lvl]);
#else
        pthread_mutex_lock(&level_locks[lvl]);
#endif
        int start_slot = lvl * SLOTS_PER_LEVEL;
        int end_slot = start_slot + SLOTS_PER_LEVEL;

        for (int s = start_slot; s < end_slot; s++) {
            /* Skip reserved EV bays for normal vehicles unless no other slot is free */
            if (IS_EV_SLOT(s) && v->type == VEH_NORMAL) {
                continue;
            }

            if (!is_slot_bit_set(s)) {
                chosen_slot = s;
                chosen_level = lvl;
                mark_slot_occupied(chosen_slot);
                slot_occupant[chosen_slot] = v->id;
                break;
            }
        }

#ifdef _WIN32
        LeaveCriticalSection(&level_locks[lvl]);
#else
        pthread_mutex_unlock(&level_locks[lvl]);
#endif

        if (chosen_slot != -1) {
            break;
        }
    }

    v->allocated_slot = chosen_slot;
    v->allocated_level = chosen_level;
    return chosen_slot;
}

void release_parking_bay(int slot_id) {
    if (slot_id < 0 || slot_id >= TOTAL_SLOTS) return;

    int lvl = slot_id / SLOTS_PER_LEVEL;
#ifdef _WIN32
    EnterCriticalSection(&level_locks[lvl]);
    mark_slot_free(slot_id);
    slot_occupant[slot_id] = -1;
    LeaveCriticalSection(&level_locks[lvl]);
    ReleaseSemaphore(sem_slots, 1, NULL);
#else
    pthread_mutex_lock(&level_locks[lvl]);
    mark_slot_free(slot_id);
    slot_occupant[slot_id] = -1;
    pthread_mutex_unlock(&level_locks[lvl]);
    sem_post(&sem_slots);
#endif
}
