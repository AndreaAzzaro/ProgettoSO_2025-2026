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

/** Riferimento globale alla SHM per gli handler dei segnali. */
static MainSharedMemory *global_shm_ref = NULL;

/** Timer ID per il timer giornaliero (per evitare leak). */
static timer_t daily_timer_id = 0;

/** Timer ID per il timer di refill (per evitare leak). */
static timer_t refill_timer_id = 0;

/* ==========================================================================
 *                       SEZIONE: PROTOTIPI PRIVATI
 * ========================================================================== */

static void handle_daily_cycle_end(int sig);
static void handle_emergency_termination(int sig);
static void handle_add_users_request(int sig);
static void handle_refill_signal(int sig);
static void handle_sigchld(int sig);

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
        while (shm->is_simulation_running && wait_for_zero(shm->semaphore_sync_id, BARRIER_MORNING_READY) == -1) {
            if (errno != EINTR) break;
        }
        if (!shm->is_simulation_running) break;

        printf("[MASTER] --- INIZIO GIORNO %d ---\n", shm->current_simulation_day + 1);

        reset_daily_statistics(shm);
        perform_initial_daily_refill(shm);
        setup_group_barriers(shm);
        setup_refill_signal();

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
            pause(); /* Attesa segnali (Timer, Refill, Emergenza) */

            if (refill_requested && shm->is_simulation_running) {
                handle_refill_cycle(shm);
                refill_requested = 0;
                setup_refill_signal(); /* Ri-arma per il prossimo evento casuale */
            }
        }

        /* 3. Fase Chiusura Giorno */
        if (shm->current_simulation_day + 1 >= shm->configuration.timings.simulation_duration_days) {
            shm->is_simulation_running = 0;
            shm->statistics.reason_for_termination = TERMINATION_REASON_TIMEOUT;
        }

        /* Notifica figli (Fine turno o Fine Simulazione) */
        int end_sig = (shm->is_simulation_running) ? SIGUSR2 : SIGTERM;
        broadcast_signal_to_all_groups(shm, end_sig);

        /* Sincronizzazione serale (Sempre dovuta per permettere ai figli di finire) */
        while (wait_for_zero(shm->semaphore_sync_id, BARRIER_EVENING_READY) == -1) {
            if (errno != EINTR) break;
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
        }
        
        /* Apriamo sempre il cancello serale per liberare i figli (sia a metà simulazione che alla fine) */
        open_barrier_gate(shm->semaphore_sync_id, BARRIER_EVENING_GATE);
    }

    /* 4. Fine Simulazione */
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

    /* Refill Primi */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    release_sem(shm->first_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    for (int i = 0; i < MAX_DISHES_PER_CATEGORY; i++) {
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
    for (int i = 0; i < MAX_DISHES_PER_CATEGORY; i++) {
        shm->second_course_station.portions[i] += shm->configuration.thresholds.refill_amount_secondi;
        if (shm->second_course_station.portions[i] > shm->configuration.thresholds.maximum_portions_secondi) {
            shm->second_course_station.portions[i] = shm->configuration.thresholds.maximum_portions_secondi;
        }
    }
    reserve_sem(shm->second_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

    printf("[MASTER] Refill completato in %d min.\n", varied_refill_time);
}

void arm_daily_timer(MainSharedMemory *shm) {
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;

    /* Elimina il timer precedente se esiste per evitare leak */
    if (daily_timer_id != 0) {
        timer_delete(daily_timer_id);
        daily_timer_id = 0;
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
    if (refill_timer_id != 0) {
        timer_delete(refill_timer_id);
        refill_timer_id = 0;
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

    /* Nota: Questo handler non è completamente signal-safe (chiama printf, wait, cleanup),
     * ma per scopi pratici è accettabile dato che è un handler di terminazione definitiva.
     * Alternative più robuste richiederebbero self-pipe trick o signalfd. */

    /* Blocca segnali aggiuntivi durante il cleanup per evitare interruzioni */
    sigset_t mask, oldmask;
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, &oldmask);

    printf("\n[SIGNAL] Ricevuto segnale di terminazione (SIGINT/SIGTERM). Cleanup in corso...\n");

    if (global_shm_ref != NULL) {
        global_shm_ref->is_simulation_running = 0;
        global_shm_ref->statistics.reason_for_termination = TERMINATION_REASON_SIGNAL;
        daily_cycle_is_active = 0;

        /* Notifica tutti i processi figli della terminazione */
        printf("[SIGNAL] Notifica ai processi figli...\n");
        broadcast_signal_to_all_groups(global_shm_ref, SIGTERM);

        /* Attende che tutti i figli terminino (con timeout implicito via SIGCHLD) */
        printf("[SIGNAL] Attesa terminazione processi figli...\n");
        int child_count = 0;
        pid_t pid;
        /* Attende fino a 2 secondi per i figli, poi procede comunque */
        alarm(2);
        while ((pid = wait(NULL)) > 0) {
            child_count++;
        }
        alarm(0);
        printf("[SIGNAL] %d processi figli terminati.\n", child_count);

        /* Se ci sono ancora processi figli ribelli, usciamo comunque
         * Le risorse IPC verranno marcate per rimozione e il kernel le pulirà */

        /* Rimuove tutte le risorse IPC */
        printf("[SIGNAL] Rimozione risorse IPC...\n");
        cleanup_ipc_resources(global_shm_ref);
        printf("[SIGNAL] Cleanup completato.\n");
    }

    /* Ripristina maschera segnali e termina */
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    exit(EXIT_SUCCESS);
}

static void handle_add_users_request(int sig) {
    (void)sig;
    if (global_shm_ref != NULL) global_shm_ref->add_users_flag = 1;
}

static void handle_refill_signal(int sig) {
    (void)sig;
    refill_requested = 1;
}

static void handle_sigchld(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (global_shm_ref != NULL) {
            /*
             * Compensazione barriere rimossa: ora usiamo SEM_UNDO sui semafori delle barriere,
             * quindi il kernel compensa automaticamente quando un processo muore.
             *
             * NOTA OPERATORI: Gli operatori usano reserve_sem_no_undo() per STATION_SEM_AVAILABLE_POSTS
             * per evitare il problema dei "ghost posts" (doppio incremento se muoiono dopo release manuale).
             * Trade-off: se un operatore viene ucciso brutalmente prima di rilasciare il posto,
             * quel posto rimane occupato. Questo è accettabile in una simulazione dove gli operatori
             * terminano normalmente. Una soluzione completa richiederebbe un operator_registry con
             * tracking dello stato "sta tenendo un posto" per ogni operatore.
             */

            /* Aggiornamento metadati gruppi */
            for (int r = 0; r < MAX_USERS_REGISTRY; r++) {
                if (global_shm_ref->user_registry[r].pid == pid) {
                    int g_idx = global_shm_ref->user_registry[r].group_index;

                    if (global_shm_ref->group_statuses[g_idx].active_members > 0) {
                        global_shm_ref->group_statuses[g_idx].active_members--;
                    }

                    /*
                     * Compensazione semafori di gruppo rimossa: SEM_UNDO compensa automaticamente
                     * quando il processo muore, evitando double-decrement.
                     */

                    if (global_shm_ref->group_statuses[g_idx].group_leader_pid == pid) {
                        global_shm_ref->group_statuses[g_idx].group_leader_pid = 0;
                    }

                    global_shm_ref->user_registry[r].pid = 0;
                    break;
                }
            }
        }
    }
}

static void reset_daily_statistics(MainSharedMemory *shm) {
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
    /* Primi */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    release_sem(shm->first_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    for (int i = 0; i < MAX_DISHES_PER_CATEGORY; i++) {
        shm->first_course_station.portions[i] = shm->configuration.thresholds.refill_amount_primi;
    }
    reserve_sem(shm->first_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

    /* Secondi */
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    release_sem(shm->second_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    for (int i = 0; i < MAX_DISHES_PER_CATEGORY; i++) {
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

        /* Aspetta che tutte le operazioni add_users completino lo spawn */
        while (1) {
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
                break;
            }

            usleep(10000); /* 10ms di attesa tra i controlli */
        }

        printf("[MASTER] Spawn utenti completato. Aggiornamento barriere...\n");
    }
}
