# Bug Report - Oasi del Golfo Simulation

Build: **CLEAN** (zero warnings with `-Wall -Wextra -Werror`)

**Status: ALL BUGS FIXED** (audit #1 + audit #2, 2026-02-02)

---

## CRITICAL -- IPC Leaks / Deadlock Risk / Crash Risk

### BUG-1: IPC Resource Leak in `cleanup_ipc_resources` -- FIXED
**File:** `src/common/common.c`

Three IPC resources were **never cleaned up**:
- `register_station.semaphore_set_id` -- cashier semaphore set
- `group_sync_semaphore_id` -- group synchronization semaphore pool
- `control_queue_id` -- control message queue for add_users

**Resolution:** Added all three cleanup calls in `cleanup_ipc_resources()`.

---

### BUG-2: Mid-day group abandonment blocks remaining members -- FIXED
**File:** `src/programs/utente/utente.c` (`fase_ritiro_formale`)

When a user abandoned via `fase_ritiro_formale`, they decremented `active_members` but
the group semaphores (PRE_CASHIER, EXIT) were already initialized to the original count.
Remaining members got stuck at the barrier until end-of-day SIGUSR2 rescued them.

**Resolution:** Added `reserve_sem_no_undo` compensation for both PRE_CASHIER and EXIT
barriers in `fase_ritiro_formale` (only for groups with size > 1). Uses NO_UNDO to avoid
SEM_UNDO double-compensation when the process exits.

**Note:** The original bug report suggested `release_sem_no_undo` (V/increment), but the
correct fix uses `reserve_sem_no_undo` (P/decrement) since the barrier counts down to zero.

---

### BUG-3: Non-async-signal-safe functions in `handle_emergency_termination` -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

The SIGINT/SIGTERM handler called `printf`, `wait()`, `alarm()`, `cleanup_ipc_resources()`.

**Resolution:** Handler now only sets a `termination_requested` flag (async-signal-safe).
The main loop in `run_simulation_loop` checks this flag after `pause()` returns, then
performs cleanup (printf, broadcast SIGTERM, etc.) in normal execution context.

---

### BUG-4: SIGCHLD handler data race on user_registry / group_statuses -- FIXED
**File:** `src/programs/responsabile_mensa/setup_population.c`

`handle_sigchld` iterates and modifies `user_registry[]` and `group_statuses[]` without
mutex. `setup_population.c` also modified `user_registry` under `MUTEX_SHARED_DATA`
without blocking SIGCHLD first.

**Resolution:** Wrapped `user_registry` modification in `launch_simulation_users()` with
`block_sigchld`/`unblock_sigchld`, matching the pattern already used in `add_users.c`.

---

### BUG-5: `timer_t` sentinel value 0 is not portable -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

`timer_create()` can return 0 as a valid timer ID.

**Resolution:** Added `daily_timer_active` and `refill_timer_active` boolean flags.
Timer existence is now checked via the flag instead of `timer_id != 0`.

---

## MEDIUM -- Incorrect Behavior

### BUG-6: `register_station.daily_income` never reset between days -- FIXED
### BUG-7: Redundant double income tracking -- FIXED
**Files:** `include/common.h`, `src/programs/operatore_cassa/operatore_cassa.c`

Income was tracked in both `register_station.daily_income/total_income` (under
MUTEX_SHARED_DATA) and `statistics.income_statistics` (under MUTEX_SIMULATION_STATS).
The `register_station` fields were **never read** by the reporting system.

**Resolution:** Removed `daily_income` and `total_income` fields from `CashierStation`
struct entirely. Removed the redundant `MUTEX_SHARED_DATA` acquisition in the cashier
payment path. All income tracking now uses `statistics.income_statistics` only.

---

### BUG-8: Cashier proceeds on non-EINTR `wait_for_zero` failure -- FIXED
**File:** `src/programs/operatore_cassa/operatore_cassa.c`

```c
// BEFORE (incorrect):
if (wait_res == 0 || (wait_res == -1 && errno != EINTR)) {
// AFTER (correct):
if (wait_res == 0) {
```

**Resolution:** Now only proceeds on actual success (`wait_res == 0`). On any failure,
the outer loop re-checks `local_daily_cycle_is_active`.

---

### BUG-9: Orphaned messages in queues at end of day -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

Stale messages from interrupted users could persist across days.

**Resolution:** Added `flush_station_queue()` helper that drains all messages with
`IPC_NOWAIT`. Called at the start of each day in `reset_daily_statistics` for all four
station queues (primi, secondi, caffe, cassa).

---

### BUG-10: Evening barrier gate opened twice -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

`open_barrier_gate(BARRIER_EVENING_GATE)` was called inside the reporting block AND
unconditionally after it.

**Resolution:** Restructured to if/else: the normal path opens the gate inside the
reporting block, and the early-exit path opens it in the `else` branch. Gate now opens
exactly once per path.

---

## LOW -- Design Issues

### BUG-11: `communication_disorder.c` uses real-time `sleep()` -- FIXED
**File:** `src/programs/communication_disorder/communication_disorder.c`

`sleep(duration_seconds)` used real time, making disruption duration wildly inconsistent
with the simulation's time scale.

**Resolution:** Replaced `sleep()` with `simulate_time_passage(duration, nanoseconds_per_tick)`.
Added `#include "utils.h"`.

---

### BUG-12: Duplicate `connect_to_simulation` function -- FIXED
**Files:** `src/common/common.c`, `include/common.h`

Identical logic was defined independently in `add_users.c` and `communication_disorder.c`.

**Resolution:** Extracted to `common.c` with declaration in `common.h`. Removed duplicates
from both files and their headers. The `srand()` call specific to `add_users` was moved
to its `main()` function.

---

### BUG-13: TOCTOU race on portion availability -- SKIPPED (working as-is)
**File:** `src/programs/utente/utente.c:214-238`

User checks portion availability under mutex, releases mutex, then sends order. The
operator re-verifies under mutex and returns `ORDER_STATUS_OUT_OF_STOCK` if unavailable.
Not a correctness bug -- just a wasted message queue round-trip in the race case.

**Decision:** Accepted as an optimistic pre-check. No code change needed.

---

### BUG-14: `setup_population.c` does not block SIGCHLD -- FIXED (see BUG-4)

**Resolution:** Same fix as BUG-4 -- wrapped with `block_sigchld`/`unblock_sigchld`.

---

## NEW BUGS -- Audit #2 (2026-02-02)

### Statistical Evidence (3-day simulation, 40 users, NNANOSECS=100000)

Day 1 output was analyzed for logic anomalies:
```
Serviti: 40 | Rinunciatari: 0
Primi: 40 | Secondi: 40 | Caffè/Dolci: 17   ← only 17/40 get coffee
Waste: Primi: 400 | Secondi: 400             ← massive waste
Caffè waste: 583 porzioni                    ← 600 initial - 17 served = 583 exactly
Incassi: 305.50 EUR                          ← 33*6.5 + 7*13 = 305.50 (ALL charged for coffee)
```

---

## CRITICAL -- Deadlock / Data Corruption

### BUG-15: Dead users not subtracted from `current_total_users` → barrier deadlock -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

The morning and evening barrier counts are computed as:
```c
int evening_count = number_of_workers + seats_cash_desk + current_total_users;
```
When a user dies (crash, SIGKILL), `handle_sigchld` decremented `group_statuses[g_idx].active_members`
but did NOT decrement `current_total_users`. The next day's barrier was initialized expecting N users,
but only N-K live users decremented it. `wait_for_zero(BARRIER_MORNING_READY)` would hang forever.

**Resolution:** Added `current_total_users--` in the new `reap_dead_children()` function that runs
in the main loop under `MUTEX_SHARED_DATA` (see BUG-16 fix). Dead users are now subtracted from
the total count before barrier setup.

---

### BUG-16: `handle_sigchld` modifies `active_members` without synchronization -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

The SIGCHLD handler read and decremented `group_statuses[g_idx].active_members` without any mutex.
User processes (separate processes) modify the same field under `MUTEX_SHARED_DATA` in `fase_ritiro_formale`.

**Resolution:** Implemented deferred SIGCHLD processing pattern:
1. `handle_sigchld` now only sets `sigchld_received = 1` (async-signal-safe).
2. New `reap_dead_children()` function does `waitpid(WNOHANG)` + metadata updates under `MUTEX_SHARED_DATA`.
3. `reap_dead_children()` is called at strategic points in the main loop: before evening barrier setup,
   inside the daily `pause()` loop, before/during evening barrier wait, and at simulation end.

---

## MEDIUM -- Incorrect Behavior / Logic Errors

### BUG-17: ~~Users charged for coffee BEFORE visiting coffee station~~ -- BY DESIGN
**File:** `src/programs/utente/utente.c:149-175`

Payment before coffee distribution is intentional (pre-paid cafeteria model).
Only 17/40 users receive coffee on Day 1 because end-of-day timer interrupts the pipeline.
Income includes coffee for all 40 users by design.

---

### BUG-18: Coffee/dessert station never refilled mid-day -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

`handle_refill_cycle()` only refilled `first_course_station` and `second_course_station`.
The `coffee_dessert_station` was refilled at start of day but NEVER replenished mid-day.

**Resolution:** Added coffee/dessert refill section in `handle_refill_cycle()` using the same
`STATION_SEM_REFILL_GATE` gate protocol. Total items = `number_of_dessert_courses + number_of_beverage_courses`.

---

### BUG-19: Refill fills all `MAX_DISHES_PER_CATEGORY` (20) slots, but menu only has 2 items -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

`perform_initial_daily_refill` and `handle_refill_cycle` iterated over `MAX_DISHES_PER_CATEGORY` (20)
when setting/incrementing portions. But the menu only has 2 primi, 2 secondi. Slots 2-19 got filled
with phantom portions that were never ordered and never counted in waste.

**Resolution:** Changed loop bounds to use actual menu item counts: `shm->food_menu.number_of_first_courses`,
`shm->food_menu.number_of_second_courses` in both `perform_initial_daily_refill()` and `handle_refill_cycle()`.

---

### BUG-20: `process_add_users_requests` can spin forever if add_users dies -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

If an `add_users` process crashed after receiving permission but before decrementing
`pending_add_users_count`, the master would spin forever in the wait loop.

**Resolution:** Added timeout of 1000 iterations × 10ms (~10 seconds). On timeout, forces
`pending_add_users_count = 0` and reconfigures the morning barrier with the current user count.

---

### BUG-21: Missing parent-side `setpgid` creates fork race -- FIXED
**File:** `src/programs/responsabile_mensa/setup_population.c`

When creating process groups, only the child called `setpgid(0, pgid)`. The parent never called
`setpgid(child_pid, pgid)`. This created a POSIX-defined race where the second child's `setpgid`
could execute before the first child had created the process group, causing EPERM.

**Resolution:** Added parent-side `setpgid(pid, pgid)` calls after fork in both operator station
loops and the cashier creation loop.

---

### BUG-22: `fase_servizio_caffe` doesn't guard against `dish_index == -1` -- FIXED
**File:** `src/programs/utente/utente.c`

When the menu has zero desserts and zero beverages, `selected_dessert_coffee_index` is -1.
`fase_servizio_caffe` would send a message with `dish_index = -1`, causing out-of-bounds access.

**Resolution:** Added `if (choice < 0) return;` guard at the start of `fase_servizio_caffe()`.

---

### BUG-23: No bounds check on `dish_index` from message queue in operator -- FIXED
**File:** `src/programs/operatore/operatore.c`

The operator read `payload.dish_index` from a message queue and immediately used it as an
array index into `stazione_ptr->portions[]` without any validation.

**Resolution:** Added bounds validation `if (payload.dish_index < 0 || payload.dish_index >= MAX_DISHES_PER_CATEGORY)`.
Out-of-range indices now receive `ORDER_STATUS_OUT_OF_STOCK` response instead of causing UB.

---

### BUG-24: Config parser uses `atol()` which truncates decimal prices -- FIXED
**File:** `src/config/config.c`

All configuration values were parsed with `atol()`, which returns a `long`. Prices like
`PRICE_PRIMI=5.50` were parsed as `5` (atol stops at the decimal point).

**Resolution:** Replaced `atol()` with `strtod()`. Integer fields use `(long)double_value` cast.
Price fields use `double_value` directly to preserve the fractional part.

---

## LOW -- Design Issues

### BUG-25: POSIX timers not explicitly deleted at end of simulation -- FIXED
**File:** `src/programs/responsabile_mensa/simulation_engine.c`

`daily_timer_id` and `refill_timer_id` were cleaned up at the start of each day cycle but not
when the simulation ended. A stale signal could fire during the shutdown path.

**Resolution:** Added `timer_delete()` calls for both timers at the end of `run_simulation_loop()`,
guarded by their respective `_active` boolean flags.

---

### BUG-26: `add_users_flag` data race across processes -- FIXED
**Files:** `src/programs/add_users/add_users.c`, `src/programs/responsabile_mensa/simulation_engine.c`

`add_users_flag` in shared memory was accessed from the add_users process, the master's signal
handler, and the master's main loop without any mutex or atomic operations.

**Resolution:** Removed the `shm->add_users_flag = 1` write from the SIGUSR1 signal handler
(now a no-op that just wakes `pause()`). The flag is set only in `add_users.c` under
`MUTEX_SHARED_DATA` before sending SIGUSR1.

---

### BUG-27: Config/menu lookup tables don't short-circuit on match -- FIXED
**Files:** `src/config/config.c`, `src/config/menu.c`

Both `resolve_configuration_key` and `resolve_menu_category_key` continued scanning the entire
table after finding a match.

**Resolution:** Added short-circuit via loop condition: `&& found_key == KEY_UNKNOWN` (config)
and `&& identified_category == CATEGORY_KEY_UNKNOWN` (menu).

---

## Fix Summary (All Bugs)

| Bug | Severity | Status | Files |
|-----|----------|--------|-------|
| BUG-1 | Critical | FIXED | `common.c` |
| BUG-2 | Critical | FIXED | `utente.c` |
| BUG-3 | Critical | FIXED | `simulation_engine.c` |
| BUG-4+14 | Critical | FIXED | `setup_population.c` |
| BUG-5 | Critical | FIXED | `simulation_engine.c` |
| BUG-6+7 | Medium | FIXED | `common.h`, `operatore_cassa.c` |
| BUG-8 | Medium | FIXED | `operatore_cassa.c` |
| BUG-9 | Medium | FIXED | `simulation_engine.c` |
| BUG-10 | Medium | FIXED | `simulation_engine.c` |
| BUG-11 | Low | FIXED | `communication_disorder.c` |
| BUG-12 | Low | FIXED | `common.c`, `common.h` |
| BUG-13 | Low | SKIPPED | -- |
| BUG-15 | Critical | FIXED | `simulation_engine.c` |
| BUG-16 | Critical | FIXED | `simulation_engine.c` |
| BUG-17 | Medium | BY DESIGN | -- |
| BUG-18 | Medium | FIXED | `simulation_engine.c` |
| BUG-19 | Medium | FIXED | `simulation_engine.c` |
| BUG-20 | Medium | FIXED | `simulation_engine.c` |
| BUG-21 | Medium | FIXED | `setup_population.c` |
| BUG-22 | Medium | FIXED | `utente.c` |
| BUG-23 | Medium | FIXED | `operatore.c` |
| BUG-24 | Medium | FIXED | `config.c` |
| BUG-25 | Low | FIXED | `simulation_engine.c` |
| BUG-26 | Low | FIXED | `add_users.c`, `simulation_engine.c` |
| BUG-27 | Low | FIXED | `config.c`, `menu.c` |
