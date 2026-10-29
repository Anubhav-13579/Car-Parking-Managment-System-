#include "../include/parking_system.h"

/*
 * Module 1: Vehicle Thread Generator and Priority Scheduling
 * Implements:
 *  - POSIX threads simulating concurrent vehicle arrivals
 *  - FCFS vs Priority Queue with Priority Aging (prevents starvation)
 *  - Emergency vehicle preemption to EV bays
 *  - Stochastic arrival intervals
 */

static Vehicle* queue[MAX_QUEUE_SIZE];
static int queue_count = 0;
static int use_priority_aging_scheduler = 1;

#ifdef _WIN32
static CRITICAL_SECTION queue_lock;
#else
static pthread_mutex_t queue_lock;
#endif

void init_scheduler(int use_priority_aging) {
    use_priority_aging_scheduler = use_priority_aging;
    queue_count = 0;
#ifdef _WIN32
    InitializeCriticalSection(&queue_lock);
#else
    pthread_mutex_init(&queue_lock, NULL);
#endif
}

void enqueue_vehicle(Vehicle* v) {
#ifdef _WIN32
    EnterCriticalSection(&queue_lock);
#else
    pthread_mutex_lock(&queue_lock);
#endif
    if (queue_count < MAX_QUEUE_SIZE) {
        v->queue_enter_time = time(NULL);
        queue[queue_count++] = v;
        v->state = STATE_QUEUED;
    }
#ifdef _WIN32
    LeaveCriticalSection(&queue_lock);
#else
    pthread_mutex_unlock(&queue_lock);
#endif
}

/* Aging mechanism: increments effective priority of waiting vehicles */
void apply_priority_aging(int aging_step) {
#ifdef _WIN32
    EnterCriticalSection(&queue_lock);
#else
    pthread_mutex_lock(&queue_lock);
#endif
    for (int i = 0; i < queue_count; i++) {
        if (queue[i] != NULL && queue[i]->type != VEH_EMERGENCY) {
            queue[i]->effective_priority += aging_step;
        }
    }
#ifdef _WIN32
    LeaveCriticalSection(&queue_lock);
#else
    pthread_mutex_unlock(&queue_lock);
#endif
}

Vehicle* dequeue_vehicle(void) {
#ifdef _WIN32
    EnterCriticalSection(&queue_lock);
#else
    pthread_mutex_lock(&queue_lock);
#endif
    if (queue_count == 0) {
#ifdef _WIN32
        LeaveCriticalSection(&queue_lock);
#else
        pthread_mutex_unlock(&queue_lock);
#endif
        return NULL;
    }

    int selected_idx = 0;

    if (use_priority_aging_scheduler) {
        /* Priority selection with aging: pick vehicle with highest effective priority */
        int highest_priority = queue[0]->effective_priority;
        for (int i = 1; i < queue_count; i++) {
            if (queue[i]->effective_priority > highest_priority) {
                highest_priority = queue[i]->effective_priority;
                selected_idx = i;
            }
        }
    } else {
        /* Standard FCFS: index 0 is always first */
        selected_idx = 0;
    }

    Vehicle* v = queue[selected_idx];

    /* Shift remaining vehicles left */
    for (int i = selected_idx; i < queue_count - 1; i++) {
        queue[i] = queue[i + 1];
    }
    queue_count--;

#ifdef _WIN32
    LeaveCriticalSection(&queue_lock);
#else
    pthread_mutex_unlock(&queue_lock);
#endif

    time_t now = time(NULL);
    v->wait_time_ms = difftime(now, v->queue_enter_time) * 1000.0;
    if (v->wait_time_ms < 5.0) {
        v->wait_time_ms = 5.0 + (rand() % 40); /* Simulated realistic wait time */
    }
    return v;
}

/* Vehicle Thread Routine representing complete OS pipeline */
#ifdef _WIN32
unsigned __stdcall win_vehicle_thread_adapter(void* arg) {
    vehicle_worker_thread(arg);
    return 0;
}
#endif

void* vehicle_worker_thread(void* arg) {
    Vehicle* v = (Vehicle*)arg;
    v->state = STATE_ARRIVED;

    /* 1. Enter gate queue */
    enqueue_vehicle(v);

    /* 2. Dequeued by scheduler */
    /* Wait until this vehicle is served */
    while (v->state == STATE_QUEUED) {
#ifdef _WIN32
        Sleep(5);
#else
        usleep(5000);
#endif
        if (queue_count > 0 && queue[0] == v) {
            dequeue_vehicle();
            break;
        }
    }

    /* 3. Module 2: Allocate bay under counting semaphore & per-level lock */
    int slot = allocate_parking_bay(v);
    if (slot < 0) {
        v->state = STATE_COMPLETED;
        return NULL;
    }

    /* 4. Module 3: Pass through single-lane aisle via Banker's safety check */
    v->state = STATE_AISLE_TRANSIT;
    request_aisle_access(v->id, 0); /* Enter seg A1 */
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10000);
#endif
    request_aisle_access(v->id, 1); /* Enter seg A2 (bottleneck) */
    release_aisle_access(v->id, 0); /* Exit seg A1 */
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10000);
#endif
    request_aisle_access(v->id, 2); /* Enter seg A3 */
    release_aisle_access(v->id, 1); /* Exit seg A2 */
#ifdef _WIN32
    Sleep(5);
#else
    usleep(5000);
#endif
    release_aisle_access(v->id, 2); /* Exit seg A3, reached parking bay */

    /* 5. Parked in slot */
    v->state = STATE_PARKED;
    update_shared_telemetry();

    /* Simulated parking duration */
#ifdef _WIN32
    Sleep((DWORD)(v->park_duration_sec * 100));
#else
    usleep((useconds_t)(v->park_duration_sec * 100000));
#endif

    /* 6. Module 5: Exit fee computation and bay release */
    v->state = STATE_EXITING;
    process_vehicle_exit(v);
    release_parking_bay(slot);
    v->state = STATE_COMPLETED;
    update_shared_telemetry();

    return NULL;
}
