#ifndef PARKING_SYSTEM_H
#define PARKING_SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
  #include <process.h>
#else
  #include <pthread.h>
  #include <semaphore.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <unistd.h>
#endif

/* System Dimensions and Constraints */
#define TOTAL_SLOTS        100
#define NUM_LEVELS         4
#define SLOTS_PER_LEVEL    25
#define AISLE_SEGMENTS     3
#define MAX_QUEUE_SIZE     1024
#define SHM_NAME           "/smart_parking_shm"
#define HOURLY_RATE        30.0

/* EV Bay Configuration (Slots 7 and 8 are reserved for EV/Emergency) */
#define IS_EV_SLOT(slot)   ((slot) == 6 || (slot) == 7)

/* Vehicle Categories */
typedef enum {
    VEH_NORMAL = 0,
    VEH_VIP = 1,
    VEH_EMERGENCY = 2
} VehicleType;

/* Vehicle Lifecycle States */
typedef enum {
    STATE_ARRIVED = 0,
    STATE_QUEUED,
    STATE_AISLE_TRANSIT,
    STATE_PARKED,
    STATE_EXITING,
    STATE_COMPLETED
} VehicleState;

/* Vehicle Structure */
typedef struct {
    int id;
    VehicleType type;
    int base_priority;
    int effective_priority;
    time_t arrival_time;
    time_t queue_enter_time;
    double wait_time_ms;
    int allocated_slot;
    int allocated_level;
    int current_aisle_segment;
    double park_duration_sec;
    double fee;
    VehicleState state;
} Vehicle;

/* Banker's Algorithm State for Aisle Segments */
typedef struct {
    int available[AISLE_SEGMENTS];
    int max_demand[MAX_QUEUE_SIZE][AISLE_SEGMENTS];
    int allocation[MAX_QUEUE_SIZE][AISLE_SEGMENTS];
    int need[MAX_QUEUE_SIZE][AISLE_SEGMENTS];
    int deadlocks_detected;
} BankersArbiter;

/* Telemetry Log Entry for Safety Arbiter */
typedef struct {
    char vehicle_tag[16];
    char segment_tag[16];
    char status[16]; /* "SAFE" or "DELAYED" */
    time_t timestamp;
} ArbiterLogEntry;

/* Shared Memory Data Contract (Read by Live Dashboard) */
typedef struct {
    int total_slots;
    int occupied_slots;
    int available_slots;
    int active_vehicle_threads;
    double fee_collected;
    int deadlocks_detected;
    uint8_t slot_status[TOTAL_SLOTS]; /* 0: Free, 1: Occupied, 2: EV Free, 3: EV Occupied */
    int slot_vehicle_id[TOTAL_SLOTS];
    ArbiterLogEntry recent_logs[10];
    int log_count;
    double avg_wait_fcfs_ms;
    double avg_wait_priority_ms;
    double lock_contention_global_ms;
    double lock_contention_per_level_ms;
    double throughput_veh_per_sec;
    int simulation_running;
} SharedParkingTelemetry;

/* Core Function Prototypes */

/* Module 1: Scheduling & Vehicle Generation */
void init_scheduler(int use_priority_aging);
void enqueue_vehicle(Vehicle* v);
Vehicle* dequeue_vehicle(void);
void apply_priority_aging(int aging_step);
void* vehicle_worker_thread(void* arg);

/* Module 2: Bay Resource Manager & Bitmap Allocation */
void init_resource_manager(void);
int allocate_parking_bay(Vehicle* v);
void release_parking_bay(int slot_id);
int check_slot_available(int slot_id);

/* Module 3: Lane Traffic Arbiter (Banker's Algorithm) */
void init_bankers_arbiter(void);
int request_aisle_access(int vehicle_id, int segment_id);
void release_aisle_access(int vehicle_id, int segment_id);
bool is_state_safe(void);

/* Module 4: Shared Memory Telemetry */
void init_shared_telemetry(void);
void update_shared_telemetry(void);
void cleanup_shared_telemetry(void);
SharedParkingTelemetry* get_telemetry_snapshot(void);

/* Module 5: Exit Fee & Benchmarking */
void init_exit_collector(void);
double process_vehicle_exit(Vehicle* v);
void run_concurrency_benchmarks(int thread_count);

#endif /* PARKING_SYSTEM_H */
