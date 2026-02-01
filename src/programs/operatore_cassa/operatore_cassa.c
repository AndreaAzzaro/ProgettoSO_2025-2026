/**
 * @file operatore_cassa.c
 * @brief Implementazione del processo Operatore di Cassa (Cassiere).
 * 
 * Gestisce i pagamenti degli utenti alla fine del percorso mensa.
 * Ciclo di vita:
 * 1. Loop Settimanale: Attivo finché shm->is_simulation_running è true.
 * 2. Loop Giornaliero: Sincronizzato con barriere.
 * 3. Loop Lavoro: Acquisisce cassa -> Processa Pagamento -> Gestisce Pause.
 * 
 * Supporta la gestione del "Communication Disorder" tramite semaforo di gate.
 * 
 * @see operatore_cassa.h
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

/* Includes del progetto */
#include "operatore_cassa.h"
#include "sem.h"
#include "shm.h"
#include "utils.h"
#include "queue.h"
#include "message.h"

/* ==========================================================================
 *                        VARIABILI GLOBALI (SEGNALI)
 * ========================================================================== */

/* Flag atomiche per la gestione del ciclo giornaliero */
static volatile sig_atomic_t local_daily_cycle_is_active = 0;
static volatile sig_atomic_t is_at_work = 0;

/* Prototypes locali */
static void handle_cassiere_signals(int sig);

/* ==========================================================================
 *                             SEZIONE: MAIN
 * ========================================================================== */

int main(int argc, char *argv[]) {
    StatoCassiere cassiere;
    
    /* 1. Inizializzazione Stato e Risorse */
    init_cassiere(&cassiere, argc, argv);

    /* 2. Setup Segnali */
    setup_cassiere_signals();

    /* 3. Sincronizzazione di Startup Globale */
    sync_child_start(cassiere.shm_ptr->semaphore_sync_id, BARRIER_STARTUP_READY, BARRIER_STARTUP_GATE);
    printf("[CASSIERE] PID %d: Inizializzazione completata. Pronto.\n", getpid());

    /* 4. Avvio Cicli di Simulazione */
    run_cassiere_simulation(&cassiere);

    /* 5. Cleanup */
    detach_shared_memory_segment(cassiere.shm_ptr);
    printf("[CASSIERE] PID %d: Terminazione pulita.\n", getpid());
    return EXIT_SUCCESS;
}

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE LOGICA
 * ========================================================================== */

void init_cassiere(StatoCassiere *cassiere, int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <shm_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    cassiere->shared_memory_id = atoi(argv[1]);
    cassiere->total_customers_processed = 0;
    cassiere->daily_breaks_taken = 0;

    /* Attach SHM */
    cassiere->shm_ptr = attach_to_simulation_shared_memory(cassiere->shared_memory_id);
}

void setup_cassiere_signals(void) {
    struct sigaction sa;
    sa.sa_handler = handle_cassiere_signals;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL); /* Pausa */
    sigaction(SIGUSR2, &sa, NULL); /* Fine Giorno */
    sigaction(SIGTERM, &sa, NULL); /* Fine Sim */
    sigaction(SIGINT,  &sa, NULL); /* Interrupt manuale */
}

void run_cassiere_simulation(StatoCassiere *cassiere) {
    /* LOOP 1: Ciclo "Settimanale" */
    while (cassiere->shm_ptr->is_simulation_running) {
        bool already_counted_active_today = false;
        cassiere->daily_breaks_taken = 0;

        /* [INIZIO GIORNATA] Sincronizzazione Mattutina */
        local_daily_cycle_is_active = 1;
        sync_child_start(cassiere->shm_ptr->semaphore_sync_id, BARRIER_MORNING_READY, BARRIER_MORNING_GATE);
        
        printf("[CASSIERE] PID %d: Inizio giornata %d.\n", getpid(), cassiere->shm_ptr->current_simulation_day + 1);

        /* LOOP 2: Ciclo "Giornaliero" */
        while (local_daily_cycle_is_active) {
            /* Competizione per la postazione cassa (Interrompibile) */
            int res = reserve_sem(cassiere->shm_ptr->register_station.semaphore_set_id, STATION_SEM_AVAILABLE_POSTS);
            
            if (res != -1) {
                is_at_work = 1;

                /* Tracciamento Cassiere Attivo nelle statistiche globali */
                if (!already_counted_active_today) {
                    reserve_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
                    cassiere->shm_ptr->statistics.operators_statistics.daily_active_operators++;
                    cassiere->shm_ptr->statistics.operators_statistics.total_active_operators_all_time++;
                    already_counted_active_today = true;
                    release_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
                }

                /* LOOP 3: Fase di Lavoro */
                fase_lavoro_cassa(cassiere);

                /* Decisione Atomica Pausa/Fine Giorno */
                fase_decisione_pausa_cassa(cassiere);

                /* Logica per gestione durata pausa */
                if (local_daily_cycle_is_active && !is_at_work) {
                    esegui_pausa_cassa(cassiere);
                }
            }
        }

        /* [FINE GIORNATA] Sincronizzazione Serale */
        sync_child_start(cassiere->shm_ptr->semaphore_sync_id, BARRIER_EVENING_READY, BARRIER_EVENING_GATE);
    }
}

void fase_lavoro_cassa(StatoCassiere *cassiere) {
    int avg_service_time = cassiere->shm_ptr->configuration.timings.average_service_time_cassa;

    while (local_daily_cycle_is_active && is_at_work) {
        /* [DESIGN] Probabilità spontanea di richiedere pausa tra un cliente e l'altro */
        if (generate_random_integer(1, 100) <= 25) {
            is_at_work = 0;
            break;
        }

        SimulationMessage msg;
        
        /* [COMMUNICATION DISORDER] Attesa se il gate è bloccato */
        /* [COMMUNICATION DISORDER] Attesa se il gate è bloccato */
        int wait_res = wait_for_zero(cassiere->shm_ptr->register_station.semaphore_set_id, STATION_SEM_STOP_GATE);
        
        /* Se wait ha successo (0) o se fallisce ma NON per interruzione segnale (es. errore grave, ignoriamo per ora), procediamo.
           Se wait fallisce per EINTR, il loop ricomincerebbe. Per evitare continue, usiamo un if grande. */
        
        if (wait_res == 0 || (wait_res == -1 && errno != EINTR)) {
            /* Ricezione Dati Pagamento (MSQ Cassa) */
            ssize_t result = receive_message_from_queue(cassiere->shm_ptr->register_station.message_queue_id, 
                                                       &msg, sizeof(CashierPayload), MSG_TYPE_ORDER, 0);
            
            if (result == -1) {
                if (errno != EINTR) {
                    is_at_work = 0;
                }
            } else {   
                CashierPayload *payload = (CashierPayload *)msg.message_text;
                double amount = 0.0;

                /* [PUNTO 4.1] Calcolo Importo in base ai prezzi configurati */
                if (payload->had_first)  amount += cassiere->shm_ptr->configuration.prices.price_first_course;
                if (payload->had_second) amount += cassiere->shm_ptr->configuration.prices.price_second_course;
                if (payload->want_coffee) amount += cassiere->shm_ptr->configuration.prices.price_coffee_dessert;

                /* Applicazione Sconto Ticket (es. 50% di sconto se ticket validato) */
                if (payload->has_discount) {
                    amount *= 0.5; 
                }

                /* [PUNTO 4.2] Aggiornamento Incassi (Protezione Mutex) */
                reserve_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
                cassiere->shm_ptr->register_station.daily_income += amount;
                cassiere->shm_ptr->register_station.total_income += amount;
                release_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);

                /* Aggiornamento Statistiche Globali (PROTEZIONE MUTEX_SIMULATION_STATS) */
                reserve_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
                cassiere->shm_ptr->statistics.income_statistics.current_daily_income += amount;
                cassiere->shm_ptr->statistics.income_statistics.accumulated_total_income += amount;
                release_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);

                /* [PUNTO 4.3] Simulazione Tempo di Servizio */
                int varied_time = calculate_varied_time(avg_service_time, 20);
                simulate_time_passage(varied_time, cassiere->shm_ptr->configuration.timings.nanoseconds_per_tick);
                cassiere->total_customers_processed++;

                /* Invio Ricevuta (Feedback all'Utente) */
                msg.message_type = payload->user_pid;
                send_message_to_queue(cassiere->shm_ptr->register_station.message_queue_id, 
                                     &msg, sizeof(CashierPayload), 0);
                
                printf("[CASSIERE] PID %d: Gestito Utente %d. Incassato: %.2f EUR.\n", 
                       getpid(), payload->user_pid, amount);
            }
        }
}
}

void fase_decisione_pausa_cassa(StatoCassiere *cassiere) {
    reserve_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
    
    if (!local_daily_cycle_is_active) {
        release_sem(cassiere->shm_ptr->register_station.semaphore_set_id, STATION_SEM_AVAILABLE_POSTS);
    } else {
        /* Controllo presidio minimo cassa */
        int total_checkouts = cassiere->shm_ptr->configuration.seats.seats_cash_desk;
        int free_checkouts = get_sem_val(cassiere->shm_ptr->register_station.semaphore_set_id, STATION_SEM_AVAILABLE_POSTS);
        int current_active = total_checkouts - free_checkouts;

        if (current_active > 1 && cassiere->daily_breaks_taken < cassiere->shm_ptr->configuration.quantities.number_of_allowed_breaks) {
            release_sem(cassiere->shm_ptr->register_station.semaphore_set_id, STATION_SEM_AVAILABLE_POSTS);
            is_at_work = 0; /* Pausa concessa: esce dal Loop 3 */
            printf("[CASSIERE] PID %d: Pausa concessa, cassa rilasciata.\n", getpid());
        } else {
            is_at_work = 1; /* Negato: resta alla cassa */
            printf("[CASSIERE] PID %d: Pausa negata (ultima cassa attiva o fine permessi).\n", getpid());
        }
    }
    
    release_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
}

void esegui_pausa_cassa(StatoCassiere *cassiere) {
    printf("[CASSIERE] PID %d: In pausa...\n", getpid());
    int break_mins = generate_random_integer(5, 20);
    
    cassiere->daily_breaks_taken++;
    reserve_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    cassiere->shm_ptr->statistics.operators_statistics.daily_breaks_taken++;
    cassiere->shm_ptr->statistics.operators_statistics.total_breaks_taken++;
    release_sem(cassiere->shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);

    simulate_time_passage(break_mins, cassiere->shm_ptr->configuration.timings.nanoseconds_per_tick);
}

static void handle_cassiere_signals(int sig) {
    if (sig == SIGUSR2 || sig == SIGTERM || sig == SIGINT) {
        local_daily_cycle_is_active = 0;
        is_at_work = 0;
    }
    if (sig == SIGUSR1) {
        is_at_work = 0; /* Richiesta di pausa */
    }
}
