/**
 * @file operatore.c
 * @brief Implementazione del processo Operatore di distribuzione piatti.
 * 
 * Ciclo di vita a 3 Loop annidati:
 * 1. Loop Settimanale: Attivo finché shm->is_simulation_running è true.
 * 2. Loop Giornaliero: Sincronizzato via Barriere (Morning -> Work -> Evening).
 * 3. Loop Lavoro: Acquisisce postazione -> Serve Clienti -> Decide Pausa (Atomica).
 * 
 * @see operatore.h per le strutture dati.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

/* Includes del progetto */
#include "operatore.h"
#include "sem.h"
#include "shm.h"
#include "utils.h"
#include "queue.h"
#include "message.h"

/* ==========================================================================
 *                        VARIABILI GLOBALI (SEGNALI)
 * ========================================================================== */

/* Flag atomica per la gestione del ciclo giornaliero tramite segnali */
static volatile sig_atomic_t local_daily_cycle_is_active = 0;
/* Flag per identificare se l'operatore occupa una postazione (Loop 3) */
static volatile sig_atomic_t is_at_work = 0;

/* Prototypes locali */
static void handle_operatore_signals(int sig);

/* ==========================================================================
 *                             SEZIONE: MAIN
 * ========================================================================== */

int main(int argc, char *argv[]) {
    StatoOperatore operatore;
    
    /* 1. Inizializzazione Stato e Risorse */
    init_operatore(&operatore, argc, argv);

    /* 2. Setup Segnali */
    setup_operatore_signals();

    /* 3. Sincronizzazione di Startup Globale */
    sync_child_start(operatore.shm_ptr->semaphore_sync_id, BARRIER_STARTUP_READY, BARRIER_STARTUP_GATE);
    printf("[OPERATORE] PID %d: Initializzazione completata. Pronto.\n", getpid());

    /* 4. Avvio Cicli di Simulazione */
    run_operatore_simulation(&operatore);

    /* 5. Cleanup */
    detach_shared_memory_segment(operatore.shm_ptr);
    printf("[OPERATORE] PID %d: Terminazione pulita.\n", getpid());
    return EXIT_SUCCESS;
}

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE LOGICA
 * ========================================================================== */

void init_operatore(StatoOperatore *operatore, int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <shm_id> <station_type>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    operatore->shared_memory_id = atoi(argv[1]);
    operatore->station_type = atoi(argv[2]);
    operatore->assigned_post_index = -1;
    operatore->total_portions_served = 0;
    operatore->daily_breaks_taken = 0;

    /* Attach SHM */
    operatore->shm_ptr = attach_to_simulation_shared_memory(operatore->shared_memory_id);
}

void run_operatore_simulation(StatoOperatore *operatore) {
    /* LOOP 1: Ciclo "Settimanale" (Durata totale della simulazione) */
    while (operatore->shm_ptr->is_simulation_running) {
        bool already_counted_active_today = false;
        operatore->daily_breaks_taken = 0; /* Reset giornaliero pause */

        /* [INIZIO GIORNATA] Sincronizzazione Mattutina */
        local_daily_cycle_is_active = 1;
        sync_child_start(operatore->shm_ptr->semaphore_sync_id, BARRIER_MORNING_READY, BARRIER_MORNING_GATE);
        
        printf("[OPERATORE] PID %d: Inizio giornata %d.\n", getpid(), operatore->shm_ptr->current_simulation_day + 1);

        /* Configurazione Riferimenti Stazione e Tempi */
        FoodDistributionStation *stazione_ptr = NULL;
        int avg_service_time = 0;
        prepare_station_context(operatore, &stazione_ptr, &avg_service_time);

        /* LOOP 2: Ciclo "Giornaliero" (Periodo di vigenza del timer Master) */
        while (local_daily_cycle_is_active) {
            /* Acquisizione Postazione (Interrompibile) */
            int res = reserve_sem(stazione_ptr->semaphore_set_id, STATION_SEM_AVAILABLE_POSTS);
            
            if (res != -1) {
                is_at_work = 1; /* Turno iniziato */
                printf("[OPERATORE] PID %d: Postazione acquisita.\n", getpid());

                /* Tracciamento Operatore Attivo (Una volta al giorno) */
                if (!already_counted_active_today) {
                    reserve_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
                    operatore->shm_ptr->statistics.operators_statistics.daily_active_operators++;
                    operatore->shm_ptr->statistics.operators_statistics.total_active_operators_all_time++;
                    already_counted_active_today = true;
                    release_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
                }

                /* LOOP 3: Ciclo di Servizio */
                fase_lavoro_stazione(operatore, stazione_ptr, avg_service_time);

                /* Decisione Atomica Pausa/Fine Giorno */
                fase_decisione_pausa_atomica(operatore, stazione_ptr);
                
                /* Logica per gestione durata pausa (Simulazione) */
                if (local_daily_cycle_is_active && !is_at_work) {
                    esegui_pausa_operatore(operatore);
                }
            }
            /* Se res == -1 (interrotto da segnale), il loop ricontrolla local_daily_cycle_is_active */
        }

        /* [FINE GIORNATA] Sincronizzazione Serale */
        sync_child_start(operatore->shm_ptr->semaphore_sync_id, BARRIER_EVENING_READY, BARRIER_EVENING_GATE);
    }
}

void prepare_station_context(StatoOperatore *operatore, FoodDistributionStation **stazione_ptr, int *avg_service_time) {
    if (operatore->station_type == 0) {
        *stazione_ptr = &operatore->shm_ptr->first_course_station;
        *avg_service_time = operatore->shm_ptr->configuration.timings.average_service_time_primi;
    } else if (operatore->station_type == 1) {
        *stazione_ptr = &operatore->shm_ptr->second_course_station;
        *avg_service_time = operatore->shm_ptr->configuration.timings.average_service_time_secondi;
    } else {
        *stazione_ptr = &operatore->shm_ptr->coffee_dessert_station;
        *avg_service_time = operatore->shm_ptr->configuration.timings.average_service_time_coffee;
    }
}

void fase_lavoro_stazione(StatoOperatore *operatore, FoodDistributionStation *stazione_ptr, int avg_service_time) {
    while (local_daily_cycle_is_active && is_at_work) {
        /* [DESIGN] Probabilità spontanea di richiedere pausa tra un cliente e l'altro */
        if (generate_random_integer(1, 100) <= 25) {
            is_at_work = 0;
            break;
        }
        /* Check Cancello Refill */
        if (wait_for_zero(stazione_ptr->semaphore_set_id, STATION_SEM_REFILL_GATE) != -1) {
            
            SimulationMessage msg;
            /* Ricezione Ordine - Controllo esito esplicito */
            ssize_t result = receive_message_from_queue(stazione_ptr->message_queue_id, &msg, sizeof(StationPayload), MSG_TYPE_ORDER, 0);
            
            if (result == -1) {
                /* Se l'errore non è un'interruzione (EINTR), usciamo dal turno */
                if (errno != EINTR) {
                    is_at_work = 0;
                }
            } else {
                StationPayload *payload = (StationPayload *)msg.message_text;
                
                /* Verifica Disponibilità Porzioni */
                reserve_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
                bool available = false;
                
                if (operatore->station_type == 2) {
                    available = true; /* Caffè/Dessert sempre disponibili (?) o non decrementano */
                } else if (stazione_ptr->portions[payload->dish_index] > 0) {
                    stazione_ptr->portions[payload->dish_index]--;
                    available = true;
                }
                release_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);

                /* Simulazione Tempo e Feedback */
                if (available) {
                    payload->status = ORDER_STATUS_SERVED;
                    
                    /* [CONSEGNA 5.1] Calcolo tempo casuale nell'intorno ± variation% */
                    int variation = (operatore->station_type == 2) ? 80 : 50;
                    int varied_time = calculate_varied_time(avg_service_time, variation);
                    
                    simulate_time_passage(varied_time, operatore->shm_ptr->configuration.timings.nanoseconds_per_tick); 
                    operatore->total_portions_served++;

                    /* Aggiornamento Statistiche Globali (PROTEZIONE MUTEX_SIMULATION_STATS) */
                    reserve_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
                    if (operatore->station_type == 0) {
                        operatore->shm_ptr->statistics.daily_served_plates.first_course_count++;
                        operatore->shm_ptr->statistics.total_served_plates.first_course_count++;
                    } else if (operatore->station_type == 1) {
                        operatore->shm_ptr->statistics.daily_served_plates.second_course_count++;
                        operatore->shm_ptr->statistics.total_served_plates.second_course_count++;
                    } else {
                        operatore->shm_ptr->statistics.daily_served_plates.coffee_dessert_count++;
                        operatore->shm_ptr->statistics.total_served_plates.coffee_dessert_count++;
                    }
                    operatore->shm_ptr->statistics.daily_served_plates.total_plates_count++;
                    operatore->shm_ptr->statistics.total_served_plates.total_plates_count++;
                    release_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);

                } else {
                    payload->status = ORDER_STATUS_OUT_OF_STOCK;
                }

                /* Risposta all'Utente */
                msg.message_type = payload->user_pid;
                send_message_to_queue(stazione_ptr->message_queue_id, &msg, sizeof(StationPayload), 0);
            }
        }
    }
}

void fase_decisione_pausa_atomica(StatoOperatore *operatore, FoodDistributionStation *stazione_ptr) {
    reserve_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
    
    if (!local_daily_cycle_is_active) {
        release_sem(stazione_ptr->semaphore_set_id, STATION_SEM_AVAILABLE_POSTS);
        printf("[OPERATORE] PID %d: Fine giornata, postazione rilasciata.\n", getpid());
    } else {
        int total_seats = (operatore->station_type == 0) ? operatore->shm_ptr->configuration.seats.seats_first_course :
                          (operatore->station_type == 1) ? operatore->shm_ptr->configuration.seats.seats_second_course :
                                                           operatore->shm_ptr->configuration.seats.seats_coffee_dessert;
        
        int free_seats = get_sem_val(stazione_ptr->semaphore_set_id, STATION_SEM_AVAILABLE_POSTS);
        int current_active_operators = total_seats - free_seats;

        if (current_active_operators > 1 && operatore->daily_breaks_taken < operatore->shm_ptr->configuration.quantities.number_of_allowed_breaks) {
            release_sem(stazione_ptr->semaphore_set_id, STATION_SEM_AVAILABLE_POSTS);
            is_at_work = 0; /* Pausa concessa: esce dal Loop 3 */
            printf("[OPERATORE] PID %d: Pausa concessa (%d active), postazione rilasciata.\n", getpid(), current_active_operators);
        } else {
            is_at_work = 1; /* Negato: resta attivo */
            printf("[OPERATORE] PID %d: Pausa negata (ultimo attivo o fine permessi). Resto attivo.\n", getpid());
        }
    }
    
    release_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
}

void esegui_pausa_operatore(StatoOperatore *operatore) {
    printf("[OPERATORE] PID %d: Inizio simulazione riposo.\n", getpid());
    int break_mins = generate_random_integer(5, 30);
    
    operatore->daily_breaks_taken++;
    reserve_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    operatore->shm_ptr->statistics.operators_statistics.daily_breaks_taken++;
    operatore->shm_ptr->statistics.operators_statistics.total_breaks_taken++;
    release_sem(operatore->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);

    simulate_time_passage(break_mins, operatore->shm_ptr->configuration.timings.nanoseconds_per_tick);
    printf("[OPERATORE] PID %d: Fine pausa (%d min simulati), torno a competere per un posto.\n", getpid(), break_mins);
}

static void handle_operatore_signals(int sig) {
    if (sig == SIGUSR2 || sig == SIGTERM || sig == SIGINT) {
        local_daily_cycle_is_active = 0;
        is_at_work = 0;
    }
    if (sig == SIGUSR1) {
        is_at_work = 0; /* Richiesta di pausa: esce dal Loop 3 */
    }
}

void setup_operatore_signals(void) {
    struct sigaction sa;
    sa.sa_handler = handle_operatore_signals;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL); /* Pausa */
    sigaction(SIGUSR2, &sa, NULL); /* Fine Giorno */
    sigaction(SIGTERM, &sa, NULL); /* Fine Sim */
    sigaction(SIGINT,  &sa, NULL); /* Interrupt manuale */
}
