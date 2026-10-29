#include "../include/parking_system.h"

/*
 * OS - 103: Smart Parking Management System (OS Concurrency & Multithreading Framework)
 * NIET B.Tech CSE (AI) Capstone Project - Group 8
 * Team: Chandani Yadav (Leader), Anubhav Saxena, Anushka Pandey, Ashmit Saxena, Ayan Ahmad
 */

void print_banner(void) {
    printf("========================================================================\n");
    printf("  OS - 103: Smart Parking Management System (Concurrency Framework)     \n");
    printf("  NIET B.Tech CSE (AI) - Capstone Project Group 8                      \n");
    printf("========================================================================\n");
    printf(" [Module 1] POSIX Vehicle Threads, FCFS / Priority Aging Scheduler     \n");
    printf(" [Module 2] Counting Semaphore (100 Slots) & Per-Level Bitmap Mutexes  \n");
    printf(" [Module 3] Dijkstra Banker's Algorithm (Aisle Deadlock Avoidance)     \n");
    printf(" [Module 4] Shared-Memory IPC Telemetry Engine                         \n");
    printf(" [Module 5] Exit Fee Mutex Counter & Concurrency Benchmarking Suite    \n");
    printf("========================================================================\n\n");
}

int main(int argc, char* argv[]) {
    print_banner();

    int num_threads = 50;
    if (argc > 1) {
        num_threads = atoi(argv[1]);
        if (num_threads <= 0) num_threads = 50;
    }

    printf("[Init] Initializing OS subsystems...\n");
    init_resource_manager();
    init_bankers_arbiter();
    init_scheduler(1); /* 1: Priority with Aging, 0: FCFS */
    init_exit_collector();
    init_shared_telemetry();

    printf("[Init] Subsystems online. Total slots: %d (Level 1-4, 25 slots each)\n", TOTAL_SLOTS);
    printf("[Init] Starting simulation with %d vehicle threads...\n\n", num_threads);

    Vehicle* vehicles = (Vehicle*)malloc(num_threads * sizeof(Vehicle));
#ifdef _WIN32
    HANDLE* threads = (HANDLE*)malloc(num_threads * sizeof(HANDLE));
#else
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
#endif

    srand((unsigned int)time(NULL));

    for (int i = 0; i < num_threads; i++) {
        vehicles[i].id = i + 1;
        int r = rand() % 100;
        if (r < 10) {
            vehicles[i].type = VEH_EMERGENCY;
            vehicles[i].base_priority = 10;
        } else if (r < 25) {
            vehicles[i].type = VEH_VIP;
            vehicles[i].base_priority = 5;
        } else {
            vehicles[i].type = VEH_NORMAL;
            vehicles[i].base_priority = 1;
        }
        vehicles[i].effective_priority = vehicles[i].base_priority;
        vehicles[i].allocated_slot = -1;
        vehicles[i].allocated_level = -1;
        vehicles[i].park_duration_sec = 0.5 + ((rand() % 10) / 10.0);
        vehicles[i].state = STATE_ARRIVED;

#ifdef _WIN32
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, win_vehicle_thread_adapter, &vehicles[i], 0, NULL);
#else
        pthread_create(&threads[i], NULL, vehicle_worker_thread, &vehicles[i]);
#endif

        if (i % 5 == 0) {
            apply_priority_aging(1); /* Priority aging step */
        }
    }

    /* Wait for threads */
    printf("[System] Processing concurrent vehicle requests...\n");
#ifdef _WIN32
    WaitForMultipleObjects(num_threads > 64 ? 64 : num_threads, threads, TRUE, INFINITE);
#else
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
#endif

    update_shared_telemetry();
    SharedParkingTelemetry* telemetry = get_telemetry_snapshot();

    printf("\n[Telemetry] Real-time Facility Status:\n");
    printf(" - Total Slots:            %d\n", telemetry->total_slots);
    printf(" - Occupied Slots:         %d\n", telemetry->occupied_slots);
    printf(" - Available Slots:        %d\n", telemetry->available_slots);
    printf(" - Deadlocks Detected:     %d (Guaranteed 0 by Banker's Algorithm)\n", telemetry->deadlocks_detected);
    printf(" - Total Revenue:          Rs. %.2f\n", get_total_revenue());

    /* Run validation benchmarks matching Report Tables 3 & 4 */
    run_concurrency_benchmarks(50);
    run_concurrency_benchmarks(100);
    run_concurrency_benchmarks(200);
    run_concurrency_benchmarks(500);

    cleanup_shared_telemetry();
    free(vehicles);
    free(threads);

    printf("[System] Simulation completed successfully.\n");
    return 0;
}
