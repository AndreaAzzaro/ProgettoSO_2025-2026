# Project: Oasi del Golfo - Cafeteria Simulation (SO 2025-2026)

## Overview
University Operating Systems project simulating a cafeteria with concurrent processes
(students, food operators, cashiers) communicating via POSIX System V IPC (shared memory,
semaphores, message queues). Written in C with strict `-Wall -Wextra -Werror`.

## Architecture
- **Master** (`responsabile_mensa`): orchestrates simulation, manages timers, barriers, refills
- **Operators** (`operatore`): serve food at stations (primi, secondi, caffe)
- **Cashiers** (`operatore_cassa`): process payments, handle communication disorder
- **Users** (`utente`): FSM-based lifecycle (ticket -> food -> meeting -> cashier -> table -> eat -> exit)
- **Utilities**: `add_users` (hot-plug users), `communication_disorder` (failure injection)

## Build
```bash
make all          # compile + reset statistics
make clean        # remove bin/ and obj/
make clean_ipc    # emergency IPC cleanup (ipcrm -a)
```
Compiler: `gcc -Wall -Wextra -Wvla -Werror -pthread -g -D_GNU_SOURCE`

## Key Conventions
- Language: C (Italian comments, English struct/function naming where possible)
- IPC: System V (shmget/semget/msgget), NOT POSIX shm_open/sem_open
- Semaphores: `SEM_UNDO` for mutexes, NO_UNDO for station posts (to avoid ghost posts)
- Barriers: Ping-pong pattern (READY counter + GATE semaphore)
- Messages: `SimulationMessage` struct with `long message_type` + `char message_text[256]`
- Payloads: Use `memcpy` into `message_text` (never cast directly - alignment safety)
- Time: `simulate_time_passage(units, nanoseconds_per_tick)` with EINTR-safe nanosleep
- Signal safety: Use `volatile sig_atomic_t` flags, check after blocking syscalls
- Mutex ordering: MUTEX_SHARED_DATA -> MUTEX_SIMULATION_STATS (never reverse)
- Group semaphores: 3 per group (PRE_CASHIER, TABLE_GATE, EXIT), indexed by `group_id * 3 + offset`

## File Layout
```
src/ipc/          - IPC wrappers (sem.c, shm.c, queue.c)
src/utils/        - Utilities (time, random, error, parsing)
src/common/       - MainSharedMemory struct, cleanup
src/config/       - config.conf and menu.conf parsers
src/statistics/   - KPI collection, CSV export, terminal reports
src/programs/     - One directory per executable (each has .c and .h)
include/          - Public headers mirroring src/ structure
config/           - Configuration files (config.conf, menu.conf)
```

## Critical Rules
1. **Never** use VLAs (enforced by `-Wvla`)
2. **Always** check return values of IPC operations
3. **Always** retry on `EINTR` for blocking IPC calls (semop, msgsnd, msgrcv, nanosleep)
4. **Never** call non-async-signal-safe functions inside signal handlers (printf, malloc, mutex ops)
5. **Always** use `memcpy` for payload packing/unpacking in message queues
6. **Always** protect shared data with the appropriate mutex semaphore
7. **Never** hold MUTEX_SIMULATION_STATS before MUTEX_SHARED_DATA (deadlock risk)
8. **Always** clean up ALL IPC resources in `cleanup_ipc_resources`
9. Service times in config are in SECONDS; `nanoseconds_per_tick` is per MINUTE - divide by 60

## Bug Status (see debug.md for full details)

### All bugs fixed (audit #1 + audit #2, 2026-02-02)
BUG-1 through BUG-27: All fixed (BUG-13 skipped, BUG-17 BY DESIGN). Build: CLEAN (zero warnings).

Key fixes from audit #2:
- **BUG-15+16**: Deferred SIGCHLD processing (flag + `reap_dead_children()` in main loop under mutex). Dead users now subtracted from `current_total_users`.
- **BUG-18**: Coffee/dessert station now refilled mid-day in `handle_refill_cycle()`.
- **BUG-19**: Refill loops use actual menu item counts instead of `MAX_DISHES_PER_CATEGORY`.
- **BUG-20**: `process_add_users_requests` has ~10s timeout with forced reset.
- **BUG-21**: Parent-side `setpgid()` added after fork to prevent POSIX race.
- **BUG-22+23**: Bounds validation on `dish_index` in both utente and operatore.
- **BUG-24**: Config parser uses `strtod()` instead of `atol()` for decimal prices.
- **BUG-25**: POSIX timers deleted at simulation end via `timer_delete()`.
- **BUG-26**: `add_users_flag` write removed from signal handler; protected by mutex in `add_users.c`.
- **BUG-27**: Lookup tables short-circuit on match via loop condition.

## Key Design Notes (post-fix)
- `connect_to_simulation()` now lives in `common.c` - used by add_users and communication_disorder
- `CashierStation` struct no longer has income fields - all income tracking via `statistics.income_statistics`
- Emergency termination uses flag-based pattern: handler sets `termination_requested`, main loop does cleanup
- Group abandonment compensates barriers with `reserve_sem_no_undo` to avoid SEM_UNDO double-compensation
- Payment happens BEFORE coffee station (pre-paid cafeteria model) -- this is BY DESIGN
- SIGCHLD processing: deferred pattern (handler sets `sigchld_received` flag, `reap_dead_children()` runs in main loop under mutex)
- `current_total_users` is decremented by `reap_dead_children()` when users die, keeping barrier counts accurate
- Config parser uses `strtod()` for all values; integer fields cast via `(long)`, price fields use `double` directly
- No `break`/`continue` outside `switch` statements (coding style constraint)
