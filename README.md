# OS - 103: Smart Parking Management System
## OS Concurrency & Multithreading Framework
**Course:** Operating Systems (`CCSE0303A`)  
**Institution:** NIET Greater Noida (An Autonomous Institute)  
**Program:** B.Tech CSE (AI) | **Group:** Group 8 (G8)  
**Faculty Mentor:** Mr. Manish Chaudhary  

---

### Team Members & Contributions
1. **Chandani Yadav (Leader)** - ERP: `2501331520072` (Architecture & Synchronization Design, Lock-ordering fix)
2. **Anubhav Saxena** - ERP: `2501331520040` (FCFS & Priority Schedulers, Priority Aging tuning)
3. **Anushka Pandey** - ERP: `2501331520042` (Slot Bitmap Allocation, Semaphore Logic, Exit Fee & Dashboard)
4. **Ashmit Saxena** - ERP: `2501331520056` (POSIX Threads, Per-level Mutex Locking, Race/Stress Tests)
5. **Ayan Ahmad** - ERP: `2501331520063` (Banker's Algorithm Arbiter, Shared-Memory IPC, Benchmarking)

---

## 🏛️ Project Architecture & OS Concepts Implemented

```
[Vehicle Thread Generator] (pthread_create, stochastic arrivals)
         │
         ▼
[Scheduler Queue] ───────► (FCFS vs Priority Scheduling with Aging)
         │
         ▼
[Entry Gate Controller] ─► (Counting Semaphore: 100 slots)
         │
         ▼
[Resource Manager] ──────► (Per-Level Mutexes + 128-bit Bitmap Allocation)
         │
         ▼
[Lane Traffic Arbiter] ──► (Dijkstra's Banker's Algorithm: Segments A1, A2, A3)
         │
         ▼
[Parking Bay & Exit] ────► (Mutex-protected Fee Counter + sem_post release)
         │
         ▼
[Shared-Memory Telemetry]► (POSIX shm_open / mmap -> Live Web Dashboard)
```

### Module Breakdown
- **Module 1: Vehicle Thread Generator & Priority Scheduler** (`src/vehicle_generator.c`)
  - Generates concurrent vehicle threads with random arrival intervals.
  - Implements **Priority Scheduling with Aging** to eliminate starvation for normal vehicles when emergency/VIP vehicles arrive.
  - Demonstrated **21% to 45% reduction in waiting time** compared to FCFS.
- **Module 2: Entry Gate Synchronization & Bay Allocation** (`src/entry_manager.c`)
  - Guarded by a counting semaphore initialized to 100 slots (`sem_wait` / `sem_post`).
  - Implements **Per-Level Mutexes** (4 levels x 25 slots) with strict ascending lock acquisition order to prevent lock-ordering deadlocks.
  - 128-bit bitmap allocation checked and updated atomically under the per-level lock (0 double allocations across 500 threads).
- **Module 3: Lane Traffic Arbiter with Deadlock Avoidance** (`src/bankers_arbiter.c`)
  - Models single-lane aisle as 3 resource segments (`seg A1`, `seg A2`, `seg A3`).
  - Runs Dijkstra's **Banker's Algorithm** before granting aisle entry.
  - Safe requests proceed immediately (`SAFE`); unsafe requests wait on condition variables (`DELAYED`).
  - Prevents Coffman's circular wait condition: **0 deadlocks detected**.
- **Module 4: Shared-Memory IPC & Telemetry Engine** (`src/shm_ipc.c`)
  - Uses POSIX shared memory (`shm_open`, `ftruncate`, `mmap`) to broadcast real-time facility metrics without blocking simulation threads.
- **Module 5: Exit Fee Collection & Performance Evaluation** (`src/exit_fee.c`)
  - Mutex-protected duration fee accumulator.
  - Benchmarks lock contention: **Per-level locking cuts contention by ~62% at 500 threads**.

---

## 🚀 How to Run in VS Code

### 1. Open Project in VS Code
Open your terminal or command prompt and run:
```bash
code "C:\Users\anush\.gemini\antigravity\scratch\smart_parking_system"
```

### 2. Run the Live Interactive Prototype (Figure 1 Dashboard)
In VS Code terminal (or press `Ctrl + F5`):
```bash
npm start
```
or
```bash
node server.js
```
Then open your browser at:
👉 **`http://localhost:3000`**

The dashboard replicates **Figure 1** from the Capstone Report:
- **Real-time KPI Counters**: Total Slots (100), Occupied, Available, Active Threads, Revenue.
- **Slot Map (Level 1)**: 100-slot grid with Live status (Green = Free, Red = Occupied, Blue = EV bays #07 and #08).
- **Scheduling Performance Chart**: Bar chart comparing FCFS vs Priority + Aging at 50, 100, 200, 500 threads.
- **Banker's Algorithm Safety Log**: Terminal log streaming `SAFE` vs `DELAYED` decisions with `Deadlocks detected: 0`.
- **Lock Contention Table**: Global Mutex vs Per-Level Locks comparison.
- **Interactive Controls**:
  - `▶ Start Live Simulation` / `⏸ Pause Simulation`
  - `⚡ Single Arrival Step`
  - `🚨 Inject Emergency Vehicle`
  - `📊 Run Benchmark`

### 3. Compile and Run Native C Code (Linux / WSL / POSIX)
If running on Linux, macOS, or Windows WSL:
```bash
make all
./bin/smart_parking 50
./bin/test_concurrency
```

---

## 📊 Benchmark Validation (from Capstone Report)

### Table 3: Average Waiting Time (FCFS vs Priority + Aging)
| Threads | FCFS (ms) | Priority + Aging (ms) | Improvement |
| :---: | :---: | :---: | :---: |
| 50 | 120 | 95 | 21% |
| 100 | 210 | 150 | 29% |
| 200 | 410 | 260 | 37% |
| 500 | 980 | 540 | **45%** |

### Table 4: Lock Contention & Throughput
| Threads | Global Mutex (ms) | Per-Level Locks (ms) | Throughput (veh/s) |
| :---: | :---: | :---: | :---: |
| 50 | 18 | 11 | 41 |
| 100 | 47 | 26 | 38 |
| 200 | 132 | 61 | 34 |
| 500 | 390 | 148 (**~62% cut**) | 29 |
