#include "../include/parking_system.h"

/*
 * Module 3: Lane Traffic Arbiter with Deadlock Avoidance
 * Implements:
 *  - Dijkstra's Banker's Algorithm on single-lane aisle segments
 *  - Resource vectors: Available[AISLE_SEGMENTS], Allocation, Max, Need
 *  - Safety check before granting aisle segment transit
 *  - Prevents Coffman's circular wait condition
 *  - Generates telemetry logs (SAFE / DELAYED) for the live dashboard
 */

static BankersArbiter arbiter;
static ArbiterLogEntry recent_logs[10];
static int log_head = 0;
static int total_log_records = 0;

#ifdef _WIN32
static CRITICAL_SECTION arbiter_lock;
static CONDITION_VARIABLE aisle_cv;
#else
static pthread_mutex_t arbiter_lock;
static pthread_cond_t aisle_cv;
#endif

void init_bankers_arbiter(void) {
    /* Capacity for segments: seg A1=2, seg A2=1 (narrow bottleneck), seg A3=2 */
    arbiter.available[0] = 2; /* seg A1 */
    arbiter.available[1] = 1; /* seg A2 (single-vehicle choke point) */
    arbiter.available[2] = 2; /* seg A3 */
    arbiter.deadlocks_detected = 0;

    memset(arbiter.max_demand, 0, sizeof(arbiter.max_demand));
    memset(arbiter.allocation, 0, sizeof(arbiter.allocation));
    memset(arbiter.need, 0, sizeof(arbiter.need));

#ifdef _WIN32
    InitializeCriticalSection(&arbiter_lock);
    InitializeConditionVariable(&aisle_cv);
#else
    pthread_mutex_init(&arbiter_lock, NULL);
    pthread_cond_init(&aisle_cv, NULL);
#endif
}

static void record_arbiter_log(int vehicle_id, int segment_id, const char* status) {
    snprintf(recent_logs[log_head].vehicle_tag, 16, "V%03d", vehicle_id);
    snprintf(recent_logs[log_head].segment_tag, 16, "seg A%d", segment_id + 1);
    snprintf(recent_logs[log_head].status, 16, "%s", status);
    recent_logs[log_head].timestamp = time(NULL);

    log_head = (log_head + 1) % 10;
    if (total_log_records < 10) total_log_records++;
}

/* Banker's Safety Algorithm Check */
bool is_state_safe(void) {
    int work[AISLE_SEGMENTS];
    for (int i = 0; i < AISLE_SEGMENTS; i++) {
        work[i] = arbiter.available[i];
    }

    bool finish[MAX_QUEUE_SIZE] = { false };
    int completed_threads = 0;
    int active_in_aisle = 0;

    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        bool has_alloc = false;
        for (int j = 0; j < AISLE_SEGMENTS; j++) {
            if (arbiter.allocation[i][j] > 0 || arbiter.need[i][j] > 0) {
                has_alloc = true;
                break;
            }
        }
        if (!has_alloc) {
            finish[i] = true; /* inactive threads are finished */
        } else {
            active_in_aisle++;
        }
    }

    bool progress = true;
    while (progress) {
        progress = false;
        for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
            if (!finish[i]) {
                bool can_satisfy = true;
                for (int j = 0; j < AISLE_SEGMENTS; j++) {
                    if (arbiter.need[i][j] > work[j]) {
                        can_satisfy = false;
                        break;
                    }
                }

                if (can_satisfy) {
                    for (int j = 0; j < AISLE_SEGMENTS; j++) {
                        work[j] += arbiter.allocation[i][j];
                    }
                    finish[i] = true;
                    completed_threads++;
                    progress = true;
                }
            }
        }
    }

    /* System is safe if all active threads can complete */
    return (completed_threads == active_in_aisle);
}

int request_aisle_access(int vehicle_id, int segment_id) {
    if (segment_id < 0 || segment_id >= AISLE_SEGMENTS) return 0;
    int thread_idx = vehicle_id % MAX_QUEUE_SIZE;

#ifdef _WIN32
    EnterCriticalSection(&arbiter_lock);
#else
    pthread_mutex_lock(&arbiter_lock);
#endif

    /* Declare maximum demand for this segment if not already set */
    arbiter.max_demand[thread_idx][segment_id] = 1;
    arbiter.need[thread_idx][segment_id] = 1;

    while (1) {
        /* Check if physical resource is available and does not cause deadlock */
        if (arbiter.available[segment_id] >= 1) {
            /* Tentative allocation */
            arbiter.available[segment_id] -= 1;
            arbiter.allocation[thread_idx][segment_id] += 1;
            arbiter.need[thread_idx][segment_id] -= 1;

            if (is_state_safe()) {
                /* Request is safe: grant access */
                record_arbiter_log(vehicle_id, segment_id, "SAFE");
#ifdef _WIN32
                LeaveCriticalSection(&arbiter_lock);
#else
                pthread_mutex_unlock(&arbiter_lock);
#endif
                return 1;
            }

            /* Rollback if unsafe */
            arbiter.available[segment_id] += 1;
            arbiter.allocation[thread_idx][segment_id] -= 1;
            arbiter.need[thread_idx][segment_id] += 1;
        }

        /* Unsafe state or segment full -> Delay vehicle thread */
        record_arbiter_log(vehicle_id, segment_id, "DELAYED");
#ifdef _WIN32
        SleepConditionVariableCS(&aisle_cv, &arbiter_lock, INFINITE);
#else
        pthread_cond_wait(&aisle_cv, &arbiter_lock);
#endif
    }
}

void release_aisle_access(int vehicle_id, int segment_id) {
    if (segment_id < 0 || segment_id >= AISLE_SEGMENTS) return;
    int thread_idx = vehicle_id % MAX_QUEUE_SIZE;

#ifdef _WIN32
    EnterCriticalSection(&arbiter_lock);
    arbiter.available[segment_id] += 1;
    arbiter.allocation[thread_idx][segment_id] -= 1;
    WakeAllConditionVariable(&aisle_cv);
    LeaveCriticalSection(&arbiter_lock);
#else
    pthread_mutex_lock(&arbiter_lock);
    arbiter.available[segment_id] += 1;
    arbiter.allocation[thread_idx][segment_id] -= 1;
    pthread_cond_broadcast(&aisle_cv);
    pthread_mutex_unlock(&arbiter_lock);
#endif
}

void get_recent_arbiter_logs(ArbiterLogEntry out_logs[10], int* out_count) {
#ifdef _WIN32
    EnterCriticalSection(&arbiter_lock);
#else
    pthread_mutex_lock(&arbiter_lock);
#endif
    int count = total_log_records;
    for (int i = 0; i < count; i++) {
        int idx = (log_head - 1 - i + 10) % 10;
        out_logs[i] = recent_logs[idx];
    }
    *out_count = count;
#ifdef _WIN32
    LeaveCriticalSection(&arbiter_lock);
#else
    pthread_mutex_unlock(&arbiter_lock);
#endif
}
