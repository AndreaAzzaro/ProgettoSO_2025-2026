/**
 * @file simulation_engine.c
 * @brief Implementazione del motore di simulazione del Master.
 * 
 * Gestisce il core loop temporale della mensa, coordinando i cicli giornalieri,
 * la sincronizzazione dei processi figli tramite barriere e i rifornimenti
 * asincroni delle stazioni.
 * 
 * @see simulation_engine.h
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>

/* Includes del progetto */
#include "common.h"
#include "simulation_engine.h"
#include "sem.h"
#include "utils.h"
#include "statistics.h"
#include "queue.h"
#include "message.h"

/* ==========================================================================
 *                        VARIABILI GLOBALI (STATO ENGINE)
 * ========================================================================== */

/** Flag atomica per il ciclo giornaliero. */
static volatile sig_atomic_t daily_cycle_is_active = 0;

/** Flag atomica per la richiesta di refill asincrono. */
static volatile sig_atomic_t refill_requested = 0;

/** Flag atomica per terminazione d'emergenza (SIGINT/SIGTERM). */
static volatile sig_atomic_t termination_requested = 0;

/** Flag atomica per notifica SIGCHLD (BUG-16: deferred reaping). */
static volatile sig_atomic_t sigchld_received = 0;

/** Riferimento globale alla SHM per gli handler dei segnali. */
static MainSharedMemory *global_shm_ref = NULL;

/** Timer ID per il timer giornaliero (per evitare leak). */
static timer_t daily_timer_id;
static bool daily_timer_active = false;

/** Timer ID per il timer di refill (per evitare leak). */
static timer_t refill_timer_id;
static bool refill_timer_active = false;

/* ==========================================================================
 *                       SEZIONE: PROTOTIPI PRIVATI
 * ========================================================================== */

static void handle_daily_cycle_end(int sig);
static void handle_emergency_termination(int sig);
static void handle_add_users_request(int sig);
static void handle_refill_signal(int sig);
static void handle_sigchld(int sig);

static void reap_dead_children(MainSharedMemory *shm);
static void reset_daily_statistics(MainSharedMemory *shm);
static void calculate_food_waste_and_teardown(MainSharedMemory *shm);
static void perform_initial_daily_refill(MainSharedMemory *shm);
static void process_add_users_requests(MainSharedMemory *shm);

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE PUBBLICA
 * ========================================================================== */

void run_simulation_loop(MainSharedMemory *shm) {
    printf("[MASTER] Engine in esecuzione. Avvio loop settimanale...\n");
    global_shm_ref = shm;

    /* LOOP SETTIMANALE: Gestione dei simulation_duration_days */
    while (shm->is_simulation_running && shm->current_simulation_day < shm->configuration.timings.simulation_duration_days) {

        /* 1. Fase Preparazione Giorno */
        int morning_ok = 0;
        while (shm->is_simulation_running && !morning_ok) {
            if (wait_for_zero(shm->semaphore_sync_id, BARRIER_MORNING_READY) == 0) {
                morning_ok = 1;
            } else if (errno != EINTR) {
                morning_ok = -1; /* Errore non recuperabile */
            }
        }

        if (shm->is_simulation_running && morning_ok > 0) {
            printf("[MASTER] --- INIZIO GIORNO %d ---\n", shm->current_simulation_day + 1);

            reset_daily_statistics(shm);
            perform_initial_daily_refill(shm);
            setup_group_barriers(shm);
            setup_refill_signal();

            /* BUG-15/16: Raccoglie figli morti prima di usare current_total_users */
            if (sigchld_received) {
                reap_dead_children(shm);
                sigchld_received = 0;
            }

            /* Setup barriera serale in base alla popolazione attuale */
            int evening_count = shm->configuration.quantities.number_of_workers +
                                shm->configuration.seats.seats_cash_desk +
                                shm->current_total_users;
            setup_barrier(shm->semaphore_sync_id, BARRIER_EVENING_READY, BARRIER_EVENING_GATE, evening_count);

            /* 2. Fase Operativa Attiva */
            daily_cycle_is_active = 1;
            arm_daily_timer(shm);
            open_barrier_gate(shm->semaphore_sync_id, BARRIER_MORNING_GATE);

            while (daily_cycle_is_active && shm->is_simulation_running) {
                pause(); /* Attesa segnali (Timer, Refill, Emergenza, SIGCHLD) */

                /* BUG-15/16: Reap figli morti nel contesto normale */
                if (sigchld_received) {
                    reap_dead_children(shm);
                    sigchld_received = 0;
                }

                if (refill_requested && shm->is_simulation_running) {
                    handle_refill_cycle(shm);
                    refill_requested = 0;
                    setup_refill_signal(); /* Ri-arma per il prossimo evento casuale */
                }
            }

            /* 3. Gestione terminazione d'emergenza (SIGINT/SIGTERM) */
            if (termination_requested) {
                printf("\n[SIGNAL] Ricevuto segnale di terminazione. Cleanup in corso...\n");
                shm->statistics.reason_for_termination = TERMINATION_REASON_SIGNAL;
                shm->is_simulation_running = 0;
                broadcast_signal_to_all_groups(shm, SIGTERM);
            }

            if (!termination_requested) {
                /* 4. Fase Chiusura Giorno */
                if (shm->current_simulation_day + 1 >= shm->configuration.timings.simulation_duration_days) {
                    shm->is_simulation_running = 0;
                    shm->statistics.reason_for_termination = TERMINATION_REASON_TIMEOUT;
                }

                /* Notifica figli (Fine turno o Fine Simulazione) */
                int end_sig = (shm->is_simulation_running) ? SIGUSR2 : SIGTERM;
                broadcast_signal_to_all_groups(shm, end_sig);

                /* BUG-15/16: Reap prima della sincronizzazione serale */
                if (sigchld_received) {
                    reap_dead_children(shm);
                    sigchld_received = 0;
                }

                /* Sincronizzazione serale (Sempre dovuta per permettere ai figli di finire) */
                int evening_wait_ok = 1;
                while (evening_wait_ok && wait_for_zero(shm->semaphore_sync_id, BARRIER_EVENING_READY) == -1) {
                    if (errno != EINTR) {
                        evening_wait_ok = 0;
                    } else {
                        /* BUG-15/16: Reap durante attesa serale */
                        if (sigchld_received) {
                            reap_dead_children(shm);
                            sigchld_received = 0;
                        }
                    }
                }

                if (shm->is_simulation_running || shm->statistics.reason_for_termination == TERMINATION_REASON_TIMEOUT) {
                    /* Elaborazione richieste asincrone di espansione utenti */
                    int users_before_process = shm->current_total_users;
                    process_add_users_requests(shm);
                    int users_after_process = shm->current_total_users;

                    /* Preparazione barriera mattutina per il giorno dopo (solo se non ci sono stati add_users) */
                    if (users_before_process == users_after_process) {
                        int next_morning_count = shm->configuration.quantities.number_of_workers +
                                                 shm->configuration.seats.seats_cash_desk +
                                                 shm->current_total_users;
                        setup_barrier(shm->semaphore_sync_id, BARRIER_MORNING_READY, BARRIER_MORNING_GATE, next_morning_count);
                    }

                    open_barrier_gate(shm->semaphore_sync_id, BARRIER_EVENING_GATE);

                    /* Calcolo sprechi prima del report */
                    calculate_food_waste_and_teardown(shm);

                    /* Reporting */
                    SimulationStatistics daily_stats = collect_simulation_statistics(shm);

                    /* Controllo OVERLOAD (Sez 5.6 della Consegna) */
                    if (daily_stats.clients_statistics.daily_clients_not_served > shm->configuration.thresholds.overload_threshold) {
                        printf("[MASTER] TERMINAZIONE PER OVERLOAD: %d utenti rinunciatari oggi (Soglia: %d)\n",
                               daily_stats.clients_statistics.daily_clients_not_served,
                               shm->configuration.thresholds.overload_threshold);
                        shm->is_simulation_running = 0;
                        shm->statistics.reason_for_termination = TERMINATION_REASON_OVERLOAD;
                    }

                    display_daily_statistics_report(daily_stats, shm->current_simulation_day);
                    save_statistics_to_csv(daily_stats, shm->current_simulation_day, "statistics_report.csv");

                    shm->current_simulation_day++;
                    printf("[MASTER] --- FINE GIORNO %d ---\n", shm->current_simulation_day);
                } else {
                    /* Percorso di uscita anticipata: apriamo il cancello per liberare i figli */
                    open_barrier_gate(shm->semaphore_sync_id, BARRIER_EVENING_GATE);
                }
            }
        }
    }

    /* 4. Fine Simulazione */

    /* BUG-25 FIX: Elimina i timer POSIX per evitare SIGALRM/SIGRTMIN+1 stale durante lo shutdown */
    if (daily_timer_active) {
        timer_delete(daily_timer_id);
        daily_timer_active = false;
    }
    if (refill_timer_active) {
        timer_delete(refill_timer_id);
        refill_timer_active = false;
    }

    /* Ultimo reaping di eventuali figli terminati durante lo shutdown */
    reap_dead_children(shm);

    usleep(500000); /* Breve attesa per permettere ai figli di stampare i log di chiusura */
    printf("\n[MASTER] Elaborazione report finale in corso...\n");

    SimulationStatistics final_stats = collect_simulation_statistics(shm);
    display_final_simulation_report(final_stats, shm->current_simulation_day);
}
void handle_refill_cycle(MainSharedMemory *shm) {
    /* [CONSEGNA 6] Simulazione tempo di esecuzione refill (AVG ± 20%) */
    int refill_avg = shm->configuration.timings.average_refill_time;
    int varied_refill_time = calculate_varied_time(refill_avg, 20);

    simulate_time_passage(varied_refill_time, shm->configuration.timings.nanoseconds_per_tick);

    /* BUG-19 FIX: Usa il numero effettivo di piatti dal menu, non MAX_DISHES_PER_CATEGORY */

    /* Refill Primi */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    release_sem(shm->first_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    for (int i = 0; i < shm->food_menu.number_of_first_courses; i++) {
        shm->first_course_station.portions[i] += shm->configuration.thresholds.refill_amount_primi;
        if (shm->first_course_station.portions[i] > shm->configuration.thresholds.maximum_portions_primi) {
            shm->first_course_station.portions[i] = shm->configuration.thresholds.maximum_portions_primi;
        }
    }
    reserve_sem(shm->first_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

    /* Refill Secondi */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    release_sem(shm->second_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    for (int i = 0; i < shm->food_menu.number_of_second_courses; i++) {
        shm->second_course_station.portions[i] += shm->configuration.thresholds.refill_amount_secondi;
        if (shm->second_course_station.portions[i] > shm->configuration.thresholds.maximum_portions_secondi) {
            shm->second_course_station.portions[i] = shm->configuration.thresholds.maximum_portions_secondi;
        }
    }
    reserve_sem(shm->second_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

    /* BUG-18 FIX: Refill Caffè/Dolci (mai riforniti mid-day prima di questo fix) */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    release_sem(shm->coffee_dessert_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    int total_coffee_dessert = shm->food_menu.number_of_dessert_courses + shm->food_menu.number_of_beverage_courses;
    for (int i = 0; i < total_coffee_dessert; i++) {
        shm->coffee_dessert_station.portions[i] += shm->configuration.thresholds.refill_amount_secondi;
        if (shm->coffee_dessert_station.portions[i] > shm->configuration.thresholds.maximum_portions_secondi) {
            shm->coffee_dessert_station.portions[i] = shm->configuration.thresholds.maximum_portions_secondi;
        }
    }
    reserve_sem(shm->coffee_dessert_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

    printf("[MASTER] Refill completato in %d min.\n", varied_refill_time);
}

void arm_daily_timer(MainSharedMemory *shm) {
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;

    /* Elimina il timer precedente se esiste per evitare leak */
    if (daily_timer_active) {
        timer_delete(daily_timer_id);
        daily_timer_active = false;
    }

    sa.sa_handler = handle_daily_cycle_end;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("[ERROR] sigaction(SIGALRM) fallita");
        exit(EXIT_FAILURE);
    }

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value.sival_ptr = &daily_timer_id;
    if (timer_create(CLOCK_REALTIME, &sev, &daily_timer_id) == -1) {
        perror("[ERROR] timer_create(daily) fallita");
        exit(EXIT_FAILURE);
    }
    daily_timer_active = true;

    long long meal_ns = (long long)shm->configuration.timings.meal_duration_minutes *
                         shm->configuration.timings.nanoseconds_per_tick;

    its.it_value.tv_sec = (time_t)(meal_ns / 1000000000LL);
    its.it_value.tv_nsec = (long)(meal_ns % 1000000000LL);
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (timer_settime(daily_timer_id, 0, &its, NULL) == -1) {
        perror("[ERROR] timer_settime(daily) fallita");
        exit(EXIT_FAILURE);
    }
}

void broadcast_signal_to_all_groups(MainSharedMemory *shm, int signal) {
    for (int i = 0; i < MAX_PROCESS_GROUPS; i++) {
        pid_t pgid = shm->process_group_pids[i];
        if (pgid > 0) {
            kill(-pgid, signal);
        }
    }
}

void setup_sigchld_handler(MainSharedMemory *shm) {
    struct sigaction sa;
    global_shm_ref = shm;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("[ERROR] sigaction(SIGCHLD) fallita");
        exit(EXIT_FAILURE);
    }
}

void setup_signal_close_day(MainSharedMemory *shm) {
    struct sigaction sa;
    global_shm_ref = shm;
    sa.sa_handler = handle_emergency_termination;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("[ERROR] sigaction(SIGINT) fallita");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("[ERROR] sigaction(SIGTERM) fallita");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = handle_add_users_request;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("[ERROR] sigaction(SIGUSR1) fallita");
        exit(EXIT_FAILURE);
    }
}

void setup_refill_signal(void) {
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;

    /* Elimina il timer precedente se esiste per evitare leak */
    if (refill_timer_active) {
        timer_delete(refill_timer_id);
        refill_timer_active = false;
    }

    sa.sa_handler = handle_refill_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGRTMIN + 1, &sa, NULL) == -1) {
        perror("[ERROR] sigaction(SIGRTMIN+1) fallita");
        exit(EXIT_FAILURE);
    }

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN + 1;
    sev.sigev_value.sival_ptr = &refill_timer_id;
    if (timer_create(CLOCK_REALTIME, &sev, &refill_timer_id) == -1) {
        perror("[ERROR] timer_create(refill) fallita");
        exit(EXIT_FAILURE);
    }
    refill_timer_active = true;

    /* [CONSEGNA 5.2] Trigger refill ogni 10 minuti simulati */
    int trigger_minutes = 10;
    long long refill_ns = (long long)trigger_minutes * global_shm_ref->configuration.timings.nanoseconds_per_tick;

    its.it_value.tv_sec = (time_t)(refill_ns / 1000000000LL);
    its.it_value.tv_nsec = (long)(refill_ns % 1000000000LL);
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (timer_settime(refill_timer_id, 0, &its, NULL) == -1) {
        perror("[ERROR] timer_settime(refill) fallita");
        exit(EXIT_FAILURE);
    }
}

void setup_group_barriers(MainSharedMemory *shm_ptr) {
    sigset_t oldset;
    block_sigchld(&oldset);  /* Protegge l'accesso a group_statuses dal signal handler SIGCHLD */

    for (int i = 0; i < shm_ptr->group_pool_size; i++) {
        int active = shm_ptr->group_statuses[i].active_members;
        if (active > 0) {
            int base = i * GROUP_SEMS_PER_ENTRY;
            init_sem_val(shm_ptr->group_sync_semaphore_id, base + GROUP_SEM_PRE_CASHIER, active);
            init_sem_val(shm_ptr->group_sync_semaphore_id, base + GROUP_SEM_TABLE_GATE, 1);
            init_sem_val(shm_ptr->group_sync_semaphore_id, base + GROUP_SEM_EXIT, active);
        }
    }

    unblock_sigchld(&oldset);
}

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE PRIVATA
 * ========================================================================== */

static void handle_daily_cycle_end(int sig) {
    (void)sig;
    daily_cycle_is_active = 0;
}

static void handle_emergency_termination(int sig) {
    (void)sig;
    termination_requested = 1;
    daily_cycle_is_active = 0;
    if (global_shm_ref != NULL) {
        global_shm_ref->is_simulation_running = 0;
    }
}

static void handle_add_users_request(int sig) {
    (void)sig;
    /* BUG-26 FIX: Rimosso accesso a shm->add_users_flag (data race cross-processo).
     * Il flag è già impostato da add_users.c sotto mutex prima di inviare SIGUSR1.
     * Questo handler serve solo a svegliare il master da pause(). */
}

static void handle_refill_signal(int sig) {
    (void)sig;
    refill_requested = 1;
}

static void handle_sigchld(int sig) {
    (void)sig;
    /* BUG-16 FIX: Solo flag async-signal-safe. Il reaping e l'aggiornamento
     * dei metadati avviene nel main loop sotto mutex (reap_dead_children). */
    sigchld_received = 1;
}

/**
 * @brief Raccoglie i processi figli terminati e aggiorna i metadati.
 *
 * BUG-15 FIX: Decrementa current_total_users per utenti morti.
 * BUG-16 FIX: Eseguita nel contesto normale (non signal handler) con mutex.
 */
static void reap_dead_children(MainSharedMemory *shm) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /*
         * Compensazione barriere rimossa: ora usiamo SEM_UNDO sui semafori delle barriere,
         * quindi il kernel compensa automaticamente quando un processo muore.
         */

        /* Aggiornamento metadati gruppi sotto mutex */
        reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
        int found = 0;
        for (int r = 0; r < MAX_USERS_REGISTRY && !found; r++) {
            if (shm->user_registry[r].pid == pid) {
                int g_idx = shm->user_registry[r].group_index;

                if (shm->group_statuses[g_idx].active_members > 0) {
                    shm->group_statuses[g_idx].active_members--;
                }

                if (shm->group_statuses[g_idx].group_leader_pid == pid) {
                    shm->group_statuses[g_idx].group_leader_pid = 0;
                }

                shm->user_registry[r].pid = 0;

                /* BUG-15 FIX: Decrementa il conteggio utenti totali */
                if (shm->current_total_users > 0) {
                    shm->current_total_users--;
                }

                found = 1;
            }
        }
        release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    }
}

static void flush_station_queue(int queue_id) {
    SimulationMessage discard;
    while (receive_message_from_queue(queue_id, &discard, MAX_MESSAGE_TEXT_SIZE, 0, IPC_NOWAIT) != -1);
}

static void reset_daily_statistics(MainSharedMemory *shm) {
    /* Svuota le code delle stazioni da messaggi orfani del giorno precedente */
    flush_station_queue(shm->first_course_station.message_queue_id);
    flush_station_queue(shm->second_course_station.message_queue_id);
    flush_station_queue(shm->coffee_dessert_station.message_queue_id);
    flush_station_queue(shm->register_station.message_queue_id);

    reserve_sem(shm->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    
    /* Reset Utenti Giornalieri */
    shm->statistics.clients_statistics.daily_clients_served = 0;
    shm->statistics.clients_statistics.daily_clients_not_served = 0;
    shm->statistics.clients_statistics.daily_clients_with_ticket = 0;
    shm->statistics.clients_statistics.daily_clients_without_ticket = 0;
    
    /* Reset Piatti Giornalieri */
    memset(&shm->statistics.daily_served_plates, 0, sizeof(StatisticsPlateCounts));
    memset(&shm->statistics.daily_leftover_plates, 0, sizeof(StatisticsPlateCounts));

    /* Reset Operatori e Incassi Giornalieri */
    shm->statistics.income_statistics.current_daily_income = 0.0;
    shm->statistics.operators_statistics.daily_active_operators = 0;
    shm->statistics.operators_statistics.daily_breaks_taken = 0;
    
    /* Reset Accumulatori Tempi del Giorno */
    memset(&shm->statistics.daily_wait_accumulators, 0, sizeof(WaitTimeAccumulator));
    
    release_sem(shm->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
}

/**
 * Calcola i piatti avanzati nelle stazioni alla fine della giornata.
 */
static void calculate_food_waste_and_teardown(MainSharedMemory *shm) {
    /* ORDINE GLOBALE DI ACQUISIZIONE: MUTEX_SHARED_DATA -> MUTEX_SIMULATION_STATS
     * per evitare deadlock ABBA con altri thread che prendono i mutex nello stesso ordine */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    
    int first_waste = 0;
    for (int i = 0; i < shm->food_menu.number_of_first_courses; i++) {
        first_waste += shm->first_course_station.portions[i];
    }
    
    int second_waste = 0;
    for (int i = 0; i < shm->food_menu.number_of_second_courses; i++) {
        second_waste += shm->second_course_station.portions[i];
    }
    
    /* Nota: Anche caffè e dolci contano come waste se avanzano a fine turno */
    int coffee_waste = 0;
    int total_coffee_dessert = shm->food_menu.number_of_dessert_courses + shm->food_menu.number_of_beverage_courses;
    for (int i = 0; i < total_coffee_dessert; i++) {
        coffee_waste += shm->coffee_dessert_station.portions[i];
    }
    printf("[DEBUG] Waste caffè/dolci: Contati %d tipi, Waste totale=%d porzioni\n", total_coffee_dessert, coffee_waste);

    /* Aggiornamento Statistiche Giornaliere */
    shm->statistics.daily_leftover_plates.first_course_count = first_waste;
    shm->statistics.daily_leftover_plates.second_course_count = second_waste;
    shm->statistics.daily_leftover_plates.coffee_dessert_count = coffee_waste;
    shm->statistics.daily_leftover_plates.total_plates_count = first_waste + second_waste + coffee_waste;

    /* Aggiornamento Statistiche Totali (Accumulo) */
    shm->statistics.total_leftover_plates.first_course_count += first_waste;
    shm->statistics.total_leftover_plates.second_course_count += second_waste;
    shm->statistics.total_leftover_plates.coffee_dessert_count += coffee_waste;
    shm->statistics.total_leftover_plates.total_plates_count += (first_waste + second_waste + coffee_waste);

    /* Rilascio in ordine inverso all'acquisizione */
    release_sem(shm->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
}



static void perform_initial_daily_refill(MainSharedMemory *shm) {
    /* BUG-19 FIX: Usa il numero effettivo di piatti dal menu, non MAX_DISHES_PER_CATEGORY */

    /* Primi */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    release_sem(shm->first_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    for (int i = 0; i < shm->food_menu.number_of_first_courses; i++) {
        shm->first_course_station.portions[i] = shm->configuration.thresholds.refill_amount_primi;
    }
    reserve_sem(shm->first_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

    /* Secondi */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    release_sem(shm->second_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    for (int i = 0; i < shm->food_menu.number_of_second_courses; i++) {
        shm->second_course_station.portions[i] = shm->configuration.thresholds.refill_amount_secondi;
    }
    reserve_sem(shm->second_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

    /* Caffè e Dessert */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    release_sem(shm->coffee_dessert_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    int total_coffee_dessert = shm->food_menu.number_of_dessert_courses + shm->food_menu.number_of_beverage_courses;
    printf("[DEBUG] Refill caffè/dolci: Dolci=%d, Bevande=%d, Totale=%d, Quantità/tipo=%d\n",
           shm->food_menu.number_of_dessert_courses, shm->food_menu.number_of_beverage_courses,
           total_coffee_dessert, shm->configuration.thresholds.refill_amount_secondi);
    for (int i = 0; i < total_coffee_dessert; i++) {
        /* Usa lo stesso valore di refill dei secondi per coerenza */
        shm->coffee_dessert_station.portions[i] = shm->configuration.thresholds.refill_amount_secondi;
    }
    reserve_sem(shm->coffee_dessert_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
}

static void process_add_users_requests(MainSharedMemory *shm) {
    int processed = 0;
    if (shm->add_users_flag) {
        SimulationMessage msg;
        while (receive_message_from_queue(shm->control_queue_id, &msg, sizeof(ControlPayload), 0, IPC_NOWAIT) != -1) {
            processed++;
            /* Non incrementiamo current_total_users qui - lo farà add_users dopo lo spawn effettivo */
        }
    }

    if (processed > 0) {
        shm->add_users_flag = 0;

        /* Incrementa il contatore delle operazioni pendenti */
        reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
        shm->pending_add_users_count += processed;
        int users_before_add = shm->current_total_users;
        release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

        /* Rilascia il permesso per ogni richiesta */
        for (int i = 0; i < processed; i++) {
            release_sem(shm->semaphore_mutex_id, MUTEX_ADD_USERS_PERMISSION);
        }

        printf("[MASTER] Elaborati %d blocchi add_users. Attesa completamento spawn...\n", processed);

        /* BUG-20 FIX: Aspetta con timeout (max ~10s) per evitare spin infinito se add_users muore */
        int max_wait_iterations = 1000; /* 1000 * 10ms = 10 secondi */
        int pending_done = 0;
        while (max_wait_iterations-- > 0 && !pending_done) {
            reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
            int pending = shm->pending_add_users_count;
            int users_after_add = shm->current_total_users;
            release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

            if (pending == 0) {
                /* Riconfigura la barriera mattutina con il nuovo conteggio utenti */
                int next_morning_count = shm->configuration.quantities.number_of_workers +
                                        shm->configuration.seats.seats_cash_desk +
                                        users_after_add;
                setup_barrier(shm->semaphore_sync_id, BARRIER_MORNING_READY, BARRIER_MORNING_GATE, next_morning_count);
                printf("[MASTER] Barriera mattutina riconfigurata: %d -> %d utenti totali (%+d nuovi)\n",
                       users_before_add, users_after_add, users_after_add - users_before_add);
                pending_done = 1;
            } else {
                usleep(10000); /* 10ms di attesa tra i controlli */
            }
        }

        if (!pending_done) {
            /* Timeout: forza reset del contatore pendente e prosegui */
            printf("[WARNING] Timeout attesa add_users. Forzatura reset pending_count.\n");
            reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
            shm->pending_add_users_count = 0;
            int users_after_add = shm->current_total_users;
            release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

            int next_morning_count = shm->configuration.quantities.number_of_workers +
                                    shm->configuration.seats.seats_cash_desk +
                                    users_after_add;
            setup_barrier(shm->semaphore_sync_id, BARRIER_MORNING_READY, BARRIER_MORNING_GATE, next_morning_count);
        }

        printf("[MASTER] Spawn utenti completato. Aggiornamento barriere...\n");
    }
}
