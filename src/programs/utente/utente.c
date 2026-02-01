/**
 * @file utente.c
 * @brief Implementazione del processo Utente.
 * 
 * Gestisce il ciclo di vita completo dell'utente all'interno della mensa:
 * 1. Sincronizzazione Mattutina (Barriera).
 * 2. Percorso: Ticket -> Primi -> Secondi -> Meeting Point -> Cassa -> Tavolo -> Bar -> Uscita.
 * 3. Gestione Gruppi: Coordinamento tramite il Group Leader per tavoli e riunioni.
 * 
 * @see utente.h per le strutture dati e i prototipi pubblici.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>

/* Includes del progetto */
#include "utente.h"
#include "sem.h"
#include "shm.h"
#include "queue.h"
#include "utils.h"

/* ==========================================================================
 *                        VARIABILI GLOBALI (SEGNALI)
 * ========================================================================== */

/** Flag atomica per la gestione del ciclo giornaliero tramite segnali dal Master. */
static volatile sig_atomic_t local_daily_cycle_is_active = 0;

/* ==========================================================================
 *                             SEZIONE: MAIN
 * ========================================================================== */

int main(int argc, char *argv[]) {
    StatoUtente utente;
    
    if (argc < 5) {
        fprintf(stderr, "[ERROR] %s: Parametri insufficienti\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* 1. Inizializzazione Stato e Risorse */
    init_utente(&utente, argc, argv);
    
    /* Diversificazione seed per ogni processo utente */
    srand(time(NULL) ^ getpid());

    /* 2. Avvio Cicli di Simulazione */
    run_utente_simulation(&utente);

    /* 3. Cleanup */
    detach_shared_memory_segment(utente.shm_ptr);
    printf("[UTENTE] PID %d: Terminazione pulita.\n", getpid());
    
    return EXIT_SUCCESS;
}

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE PUBBLICA
 * ========================================================================== */

void init_utente(StatoUtente *utente, int argc, char *argv[]) {
    (void)argc;
    
    /* Parsing parametri da linea di comando */
    utente->shared_memory_id = atoi(argv[1]);
    utente->group_size = atoi(argv[2]);
    int sync_index = atoi(argv[3]);
    utente->is_group_leader = (atoi(argv[4]) == 1);
    
    /* Group ID basato sull'indice nel pool di sincronizzazione */
    utente->group_id = sync_index; 

    /* Connessione alla memoria condivisa */
    utente->shm_ptr = attach_to_simulation_shared_memory(utente->shared_memory_id);

    /* Definizione profilo utente (ticket, gusti, pazienza) */
    genera_identita_casuale(utente);
}

void run_utente_simulation(StatoUtente *utente) {
    /* Setup segnali e startup */
    setup_utente_signals();
    sincronizza_startup_utente(utente);

    while (utente->shm_ptr->is_simulation_running) {
        /* Preparazione giornata */
        reset_stato_giornaliero_utente(utente);
        sync_child_start(utente->shm_ptr->semaphore_sync_id, BARRIER_MORNING_READY, BARRIER_MORNING_GATE);
        
        /* Esecuzione Percorso Operativo */
        if (local_daily_cycle_is_active) {
            esegui_percorso_mensa_giornaliero(utente);
        }

        /* Fine giornata */
        sync_child_start(utente->shm_ptr->semaphore_sync_id, BARRIER_EVENING_READY, BARRIER_EVENING_GATE);
    }
}

void sincronizza_startup_utente(StatoUtente *utente) {
    if (utente->shm_ptr->current_simulation_day == 0) {
        printf("[DEBUG] Utente PID %d: In attesa barriera di Startup.\n", getpid());
        sync_child_start(utente->shm_ptr->semaphore_sync_id, BARRIER_STARTUP_READY, BARRIER_STARTUP_GATE);
    } else {
        printf("[DEBUG] Utente PID %d: Inizializzazione a simulazione in corso.\n", getpid());
    }
    
    if (utente->is_group_leader) {
        reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
        utente->shm_ptr->group_statuses[utente->group_id].group_leader_pid = getpid();
        release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
    }
    printf("[DEBUG] Utente PID %d: Pronto.\n", getpid());
}

void reset_stato_giornaliero_utente(StatoUtente *utente) {
    local_daily_cycle_is_active = 1;
    genera_identita_casuale(utente);
    utente->ticket_is_validated = false;
    utente->assigned_table_id = -1;
}

void esegui_percorso_mensa_giornaliero(StatoUtente *utente) {
    printf("[UTENTE] PID %d: Inizio giornata %d.\n", getpid(), utente->shm_ptr->current_simulation_day + 1);

    fase_validazione_ticket(utente);

    bool got_first = fase_servizio_stazione(utente, 0);  /* Primi */
    bool got_second = fase_servizio_stazione(utente, 1); /* Secondi */

    /* Gestione Abbandono */
    if (local_daily_cycle_is_active && !got_first && !got_second) {
        fase_ritiro_formale(utente);
    }

    /* Flusso Post-Servizio */
    if (local_daily_cycle_is_active) {
        fase_riunione_gruppo(utente);
        fase_pagamento_cassa(utente, got_first, got_second);
        fase_prenotazione_tavolo(utente);
        fase_consumazione_pasto(utente, got_first, got_second);
        fase_servizio_caffe(utente);
        
        aggiorna_statistiche_servito(utente);
        fase_uscita_collettiva(utente);
    } else {
        aggiorna_statistiche_non_servito(utente);
    }
}

/* ==========================================================================
 *                SEZIONE: IMPLEMENTAZIONE FASI OPERATIVE
 * ========================================================================== */

void fase_validazione_ticket(StatoUtente *utente) {
    if (utente->has_ticket && local_daily_cycle_is_active) {
        printf("[UTENTE] PID %d: In coda per validazione ticket...\n", getpid());
        if (reserve_sem_interruptible(utente->shm_ptr->semaphore_ticket_id, 0) != -1) {
            if (local_daily_cycle_is_active) {
                int avg_ticket_time = utente->shm_ptr->configuration.timings.average_service_time_ticket;
                int varied_time = calculate_varied_time(avg_ticket_time, 20);
                
                simulate_time_passage(varied_time, utente->shm_ptr->configuration.timings.nanoseconds_per_tick);
                utente->ticket_is_validated = true;
                printf("[UTENTE] PID %d: Ticket validato.\n", getpid());
            }
            release_sem(utente->shm_ptr->semaphore_ticket_id, 0);
        }
    }
}

bool fase_servizio_stazione(StatoUtente *utente, int stazione_tipo) {
    if (!local_daily_cycle_is_active) return false;

    FoodDistributionStation *stazione = (stazione_tipo == 0) ? 
                &utente->shm_ptr->first_course_station : 
                &utente->shm_ptr->second_course_station;
    
    int choice = (stazione_tipo == 0) ? 
                utente->selected_first_course_index : 
                utente->selected_second_course_index;

    if (choice == -1) return false;

    /* Check disponibilità e ripiego atomico */
    reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
    
    if (stazione->portions[choice] <= 0) {
        int num_dishes = (stazione_tipo == 0) ? 
                        utente->shm_ptr->food_menu.number_of_first_courses : 
                        utente->shm_ptr->food_menu.number_of_second_courses;
        
        bool found_alt = false;
        for (int i = 0; i < num_dishes; i++) {
            if (stazione->portions[i] > 0) {
                choice = i;
                found_alt = true;
                break;
            }
        }

        if (!found_alt) {
            release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
            printf("[UTENTE] PID %d: Piatti ESAURITI alla stazione %s.\n", 
                   getpid(), (stazione_tipo == 0 ? "Primi" : "Secondi"));
            return false;
        }
        printf("[UTENTE] PID %d: Piatto preferito terminato. Scelgo alternativa %d.\n", getpid(), choice);
    }
    release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);

    /* Check soglia pazienza (coda IPC) */
    int q_len = get_message_queue_length(stazione->message_queue_id);
    if (q_len > utente->shm_ptr->configuration.thresholds.queue_patience_threshold) {
        printf("[UTENTE] PID %d: Troppa coda alla stazione %s (%d utenti). Salto.\n", 
               getpid(), (stazione_tipo == 0 ? "Primi" : "Secondi"), q_len);
        return false;
    }

    return fase_checkout_piatto(utente, stazione, &choice, stazione_tipo);
}

void fase_ritiro_formale(StatoUtente *utente) {
    printf("[UTENTE] PID %d: Abbandono per mancanza cibo o pazienza.\n", getpid());
    local_daily_cycle_is_active = 0;
    
    int s_idx = utente->group_id;
    reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
    if (utente->shm_ptr->group_statuses[s_idx].active_members > 0) {
        utente->shm_ptr->group_statuses[s_idx].active_members--;
        if (utente->is_group_leader) {
            utente->shm_ptr->group_statuses[s_idx].group_leader_pid = 0;
            utente->is_group_leader = false;
        }
    }
    release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);

    /* Sblocco semafori di gruppo per evitare deadlock degli altri membri */
    int base_sem = s_idx * GROUP_SEMS_PER_ENTRY;
    reserve_sem_no_undo(utente->shm_ptr->group_sync_semaphore_id, base_sem + GROUP_SEM_PRE_CASHIER);
    reserve_sem_no_undo(utente->shm_ptr->group_sync_semaphore_id, base_sem + GROUP_SEM_EXIT);
}

void fase_riunione_gruppo(StatoUtente *utente) {
    if (utente->group_size <= 1 || !local_daily_cycle_is_active) return;

    int s_idx = utente->group_id;
    reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
    if (utente->shm_ptr->group_statuses[s_idx].group_leader_pid == 0) {
        utente->shm_ptr->group_statuses[s_idx].group_leader_pid = getpid();
        utente->is_group_leader = true;
    }
    release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);

    int base_sem = s_idx * GROUP_SEMS_PER_ENTRY;
    printf("[UTENTE] PID %d: Riunione amici al meeting point...\n", getpid());
    
    if (reserve_sem_interruptible(utente->shm_ptr->group_sync_semaphore_id, base_sem + GROUP_SEM_PRE_CASHIER) != -1) {
        if (local_daily_cycle_is_active) {
            wait_for_zero_interruptible(utente->shm_ptr->group_sync_semaphore_id, base_sem + GROUP_SEM_PRE_CASHIER);
        }
    }
}

void fase_pagamento_cassa(StatoUtente *utente, bool p1, bool p2) {
    if (!local_daily_cycle_is_active) return;

    struct timespec start_t, end_t;
    clock_gettime(CLOCK_MONOTONIC, &start_t);

    SimulationMessage msg;
    msg.message_type = MSG_TYPE_ORDER; 
    
    CashierPayload *payload = (CashierPayload *)msg.message_text;
    payload->user_pid = getpid();
    payload->had_first = p1;
    payload->had_second = p2;
    payload->want_coffee = true; 
    payload->has_discount = utente->ticket_is_validated;

    printf("[UTENTE] PID %d: In coda alla Cassa...\n", getpid());

    if (send_message_to_queue(utente->shm_ptr->register_station.message_queue_id, &msg, sizeof(CashierPayload), 0) != -1) {
        if (receive_message_with_soft_timeout(utente->shm_ptr->register_station.message_queue_id, &msg, sizeof(CashierPayload), getpid()) != -1) {
            if (local_daily_cycle_is_active) {
                clock_gettime(CLOCK_MONOTONIC, &end_t);
                double w_min = get_simulated_minutes(start_t, end_t, utente->shm_ptr->configuration.timings.nanoseconds_per_tick);
                update_wait_time_stat(utente, w_min, 3); /* 3: Cassa */
                printf("[UTENTE] PID %d: Pagamento completato.\n", getpid());
            }
        }
    }
}

void fase_prenotazione_tavolo(StatoUtente *utente) {
    if (!local_daily_cycle_is_active) return;

    int s_idx = utente->group_id;
    int base_sem = s_idx * GROUP_SEMS_PER_ENTRY;

    if (utente->is_group_leader) {
        reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
        int members = utente->shm_ptr->group_statuses[s_idx].active_members;
        release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);

        printf("[UTENTE] PID %d: Leader cerca tavolo per %d persone...\n", getpid(), members);
        
        bool found = false;
        while (local_daily_cycle_is_active && !found) {
            reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_TABLES);
            
            for (int i = 0; i < utente->shm_ptr->seat_area.active_tables_count; i++) {
                Table *t = &utente->shm_ptr->seat_area.tables[i];
                if ((t->capacity - t->occupied_seats) >= members) {
                    t->occupied_seats += members;
                    utente->assigned_table_id = i;
                    
                    reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
                    utente->shm_ptr->group_statuses[s_idx].assigned_table_id = i;
                    release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
                    
                    found = true;
                    break;
                }
            }
            
            release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_TABLES);
            
            if (!found && local_daily_cycle_is_active) {
                /* Attesa passiva su semaforo di condizione - RITORNA SU SEGNALE FINE GIORNO */
                reserve_sem_interruptible(utente->shm_ptr->seat_area.condition_semaphore_id, 0);
            }
        }

        if (found) {
            printf("[UTENTE] PID %d: Tavolo %d trovato e occupato per il gruppo.\n", getpid(), utente->assigned_table_id);
            open_barrier_gate(utente->shm_ptr->group_sync_semaphore_id, base_sem + GROUP_SEM_TABLE_GATE);
        }
    } else {
        printf("[UTENTE] PID %d: In attesa del leader per il tavolo...\n", getpid());
        wait_for_zero_interruptible(utente->shm_ptr->group_sync_semaphore_id, base_sem + GROUP_SEM_TABLE_GATE);
        
        if (local_daily_cycle_is_active) {
            reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
            utente->assigned_table_id = utente->shm_ptr->group_statuses[s_idx].assigned_table_id;
            release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
        }
    }
}

void fase_consumazione_pasto(StatoUtente *utente, bool p1, bool p2) {
    if (!local_daily_cycle_is_active || utente->assigned_table_id == -1) return;

    int count = (p1?1:0) + (p2?1:0);
    if (count > 0) {
        /* [FIX] Tempo di consumo proporzionale e allineato alla scala temporale */
        int minutes_to_eat = generate_random_integer(5 * count, 10 * count);
        simulate_time_passage(minutes_to_eat, utente->shm_ptr->configuration.timings.nanoseconds_per_tick);
    }
    
    /* Fase Rilascio Posto (Step 4) */
    reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_TABLES);
    utente->shm_ptr->seat_area.tables[utente->assigned_table_id].occupied_seats--;
    release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_TABLES);
    
    /* Notifica a chi è in attesa */
    release_sem(utente->shm_ptr->seat_area.condition_semaphore_id, 0);

    printf("[UTENTE] PID %d: Pasto terminato al tavolo %d. Posto liberato.\n", 
           getpid(), utente->assigned_table_id);
}

void fase_servizio_caffe(StatoUtente *utente) {
    /* [FIX] Permettiamo il caffè anche se il timer è scaduto, purché il pasto sia finito */
    
    FoodDistributionStation *stazione = &utente->shm_ptr->coffee_dessert_station;
    int choice = utente->selected_dessert_coffee_index;

    printf("[UTENTE] PID %d: Coda Caffè/Dolce...\n", getpid());
    fase_checkout_piatto(utente, stazione, &choice, 2); /* 2: Caffè */
}

void fase_uscita_collettiva(StatoUtente *utente) {
    if (utente->group_size <= 1 || !local_daily_cycle_is_active) return;
    int s_idx = utente->group_id;
    int base_sem = s_idx * GROUP_SEMS_PER_ENTRY;
    
    if (reserve_sem_interruptible(utente->shm_ptr->group_sync_semaphore_id, base_sem + GROUP_SEM_EXIT) != -1) {
        if (local_daily_cycle_is_active) {
            wait_for_zero_interruptible(utente->shm_ptr->group_sync_semaphore_id, base_sem + GROUP_SEM_EXIT);
        }
    }
    printf("[UTENTE] PID %d: Uscita gruppo completata.\n", getpid());
}

void aggiorna_statistiche_servito(StatoUtente *utente) {
    reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    
    /* Incremento Giornaliero */
    utente->shm_ptr->statistics.clients_statistics.daily_clients_served++;
    /* Incremento Totale */
    utente->shm_ptr->statistics.clients_statistics.total_clients_served++;
    
    if (utente->has_ticket) {
        utente->shm_ptr->statistics.clients_statistics.daily_clients_with_ticket++;
        utente->shm_ptr->statistics.clients_statistics.total_clients_with_ticket++;
    } else {
        utente->shm_ptr->statistics.clients_statistics.daily_clients_without_ticket++;
        utente->shm_ptr->statistics.clients_statistics.total_clients_without_ticket++;
    }
    release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
}

void aggiorna_statistiche_non_servito(StatoUtente *utente) {
    reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    utente->shm_ptr->statistics.clients_statistics.daily_clients_not_served++;
    utente->shm_ptr->statistics.clients_statistics.total_clients_not_served++;
    release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
}

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE PRIVATA
 * ========================================================================== */

void handle_utente_signals(int sig) {
    if (sig == SIGUSR2 || sig == SIGTERM || sig == SIGINT) {
        local_daily_cycle_is_active = 0;
    }
}

void setup_utente_signals(void) {
    struct sigaction sa;
    sa.sa_handler = handle_utente_signals;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL); /* Fine Giorno */
    sigaction(SIGTERM, &sa, NULL); /* Terminazione Master */
    sigaction(SIGINT,  &sa, NULL); /* Interruzione manuale */
}

void genera_identita_casuale(StatoUtente *utente) {
    /* PID % 5 != 0 garantisce circa l'80% di utenti con ticket sconto */
    utente->has_ticket = ((getpid() % 5) != 0);
    
    int n_primi = utente->shm_ptr->food_menu.number_of_first_courses;
    int n_secondi = utente->shm_ptr->food_menu.number_of_second_courses;
    int n_desserts = utente->shm_ptr->food_menu.number_of_dessert_courses;

    utente->selected_first_course_index = (n_primi > 0) ? generate_random_integer(0, n_primi - 1) : -1;
    utente->selected_second_course_index = (n_secondi > 0) ? generate_random_integer(0, n_secondi - 1) : -1;
    utente->selected_dessert_coffee_index = (n_desserts > 0) ? generate_random_integer(0, n_desserts - 1) : -1;
    
    utente->group_patience_threshold = generate_random_integer(30, 120);
}

ssize_t receive_message_with_soft_timeout(int queue_id, SimulationMessage *msg, size_t size, long type) {
    while (local_daily_cycle_is_active) {
        ssize_t res = receive_message_from_queue(queue_id, msg, size, type, IPC_NOWAIT);
        if (res != -1) return res;
        if (errno != ENOMSG) return -1;
        usleep(5000); /* 5ms prima del retry */
    }
    return -1;
}

bool fase_checkout_piatto(StatoUtente *utente, FoodDistributionStation *stazione, int *choice, int stazione_tipo) {
    struct timespec s_t, e_t;
    clock_gettime(CLOCK_MONOTONIC, &s_t);
    
    SimulationMessage msg;
    msg.message_type = MSG_TYPE_ORDER;
    StationPayload *pay = (StationPayload *)msg.message_text;
    pay->user_pid = getpid();
    pay->dish_index = *choice;
    pay->status = 0;

    if (send_message_to_queue(stazione->message_queue_id, &msg, sizeof(StationPayload), 0) == -1) return false;
    if (receive_message_with_soft_timeout(stazione->message_queue_id, &msg, sizeof(StationPayload), getpid()) == -1) return false;

    if (!local_daily_cycle_is_active) return false;

    clock_gettime(CLOCK_MONOTONIC, &e_t);
    double w_min = get_simulated_minutes(s_t, e_t, utente->shm_ptr->configuration.timings.nanoseconds_per_tick);
    update_wait_time_stat(utente, w_min, stazione_tipo);

    if (pay->status == ORDER_STATUS_SERVED) {
        printf("[UTENTE] PID %d: Piatto %d ricevuto.\n", getpid(), *choice);
        return true;
    }
    return false;
}

double get_simulated_minutes(struct timespec start, struct timespec end, long nanosecs_per_tick) {
    long long delta_ns = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    return (nanosecs_per_tick > 0) ? (double)delta_ns / nanosecs_per_tick : 0;
}

void update_wait_time_stat(StatoUtente *utente, double wait_min, int type) {
    reserve_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    
    WaitTimeAccumulator *daily = &utente->shm_ptr->statistics.daily_wait_accumulators;
    WaitTimeAccumulator *total = &utente->shm_ptr->statistics.total_wait_accumulators;
    
    if (type == 0)      { daily->sum_wait_first += wait_min; daily->count_first++; total->sum_wait_first += wait_min; total->count_first++; }
    else if (type == 1) { daily->sum_wait_second += wait_min; daily->count_second++; total->sum_wait_second += wait_min; total->count_second++; }
    else if (type == 2) { daily->sum_wait_coffee += wait_min; daily->count_coffee++; total->sum_wait_coffee += wait_min; total->count_coffee++; }
    else if (type == 3) { daily->sum_wait_cashier += wait_min; daily->count_cashier++; total->sum_wait_cashier += wait_min; total->count_cashier++; }
    
    release_sem(utente->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
}