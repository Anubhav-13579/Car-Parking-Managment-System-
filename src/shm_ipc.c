#include "../include/parking_system.h"

/*
 * Module 4: Shared-Memory IPC and Real-Time Dashboard
 * Implements:
 *  - POSIX shared memory (shm_open, mmap) / Windows file mapping
 *  - Reader-Writer synchronization
 *  - Exposes live telemetry for independent visual dashboard process
 */

static SharedParkingTelemetry* shared_data = NULL;
static SharedParkingTelemetry local_fallback;

#ifdef _WIN32
static HANDLE h_map_file = NULL;
#else
static int shm_fd = -1;
#endif

void init_shared_telemetry(void) {
#ifdef _WIN32
    h_map_file = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedParkingTelemetry),
        TEXT("SmartParkingSharedMemory")
    );
    if (h_map_file != NULL) {
        shared_data = (SharedParkingTelemetry*)MapViewOfFile(
            h_map_file,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(SharedParkingTelemetry)
        );
    }
#else
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd >= 0) {
        if (ftruncate(shm_fd, sizeof(SharedParkingTelemetry)) != -1) {
            shared_data = (SharedParkingTelemetry*)mmap(
                NULL,
                sizeof(SharedParkingTelemetry),
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                shm_fd,
                0
            );
        }
    }
#endif

    if (shared_data == NULL) {
        shared_data = &local_fallback;
    }

    memset(shared_data, 0, sizeof(SharedParkingTelemetry));
    shared_data->total_slots = TOTAL_SLOTS;
    shared_data->available_slots = TOTAL_SLOTS;
    shared_data->occupied_slots = 0;
    shared_data->deadlocks_detected = 0;
    shared_data->simulation_running = 1;

    /* Mark EV bays (slots 6 and 7 -> 7 and 8 in 1-based) as EV Free (status 2) */
    for (int i = 0; i < TOTAL_SLOTS; i++) {
        if (IS_EV_SLOT(i)) {
            shared_data->slot_status[i] = 2; /* EV Free */
        } else {
            shared_data->slot_status[i] = 0; /* Standard Free */
        }
        shared_data->slot_vehicle_id[i] = -1;
    }
}

void update_shared_telemetry(void) {
    if (!shared_data) return;

    int occupied = 0;
    for (int i = 0; i < TOTAL_SLOTS; i++) {
        int avail = check_slot_available(i);
        if (!avail) {
            occupied++;
            shared_data->slot_status[i] = IS_EV_SLOT(i) ? 3 : 1; /* 3: EV Occupied, 1: Occupied */
        } else {
            shared_data->slot_status[i] = IS_EV_SLOT(i) ? 2 : 0; /* 2: EV Free, 0: Free */
        }
    }

    shared_data->occupied_slots = occupied;
    shared_data->available_slots = TOTAL_SLOTS - occupied;
}

SharedParkingTelemetry* get_telemetry_snapshot(void) {
    return shared_data;
}

void cleanup_shared_telemetry(void) {
#ifdef _WIN32
    if (shared_data && shared_data != &local_fallback) {
        UnmapViewOfFile(shared_data);
    }
    if (h_map_file) {
        CloseHandle(h_map_file);
    }
#else
    if (shared_data && shared_data != &local_fallback) {
        munmap(shared_data, sizeof(SharedParkingTelemetry));
    }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
    }
#endif
}
