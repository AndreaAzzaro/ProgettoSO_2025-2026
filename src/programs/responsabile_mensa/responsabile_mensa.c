/**
 * @file responsabile_mensa.c
 * @brief Punto di ingresso del Master (Responsabile Mensa).
 * 
 * Il processo Master coordina l'intera simulazione:
 * 1. Inizializza le risorse IPC (SHM, Semafori, Code).
 * 2. Carica la configurazione e il menu.
 * 3. Esegue lo spawn dei processi figli (Operatori, Utenti, Cassieri).
 * 4. Gestisce il loop dei giorni tramite il simulation_engine.
 * 5. Esegue la terminazione pulita e la rimozione delle risorse.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "responsabile_mensa.h"
#include "sem.h"
#include "shm.h"
#include "config.h"
#include "menu.h"

/* ==========================================================================
 *                             SEZIONE: MAIN
 * ========================================================================== */

int main(int argc, char *argv[]) {
    const char *config_path = (argc > 1) ? argv[1] : NULL;
    
    printf("[MASTER] Responsabile Mensa in avvio...\n");

    /* 1. Caricamento Configurazione e Menu */
    SimulationConfiguration config = load_simulation_configuration(config_path);
    SimulationMenu menu = load_simulation_menu();
    
    /* 2. Setup SHM e Risorse IPC */
    /* Calcoliamo il pool dei gruppi basandoci sul numero di utenti iniziali + margine di espansione */
    int users_to_assign = config.quantities.number_of_initial_users;
    int dynamic_group_pool_size = users_to_assign + 100; 

    MainSharedMemory *shm_ptr = initialize_simulation_shared_memory(dynamic_group_pool_size);
    shm_ptr->configuration = config;
    shm_ptr->food_menu = menu;
    
    printf("[MASTER] SHM Inizializzata. ID: %d\n", shm_ptr->shared_memory_id);
    
    /* Inizializzazione di base delle risorse IPC globali */
    initialize_ipc_sources(shm_ptr);
    
    /* Configurazione handler dei segnali (SIGCHLD per zombie, SIGINT/SIGALRM per master) */
    setup_sigchld_handler(shm_ptr);
    setup_signal_close_day(shm_ptr);

    /* 3. Inizializzazione Sincronizzazione Gruppi */
    int total_required_groups = calculate_initial_groups_count(shm_ptr);
    initialize_group_sync_pool(shm_ptr, total_required_groups);

    /* 4. Setup Popolazione e Barriere */
    setup_worker_distribution(shm_ptr);
    initialize_station_operator_semaphores(shm_ptr);

    /* Preparazione barriera di sincronizzazione iniziale */
    setup_prework_barrier(shm_ptr);

    /* Setup barriere operative (Morning/Evening) per il ciclo giornaliero */
    setup_daily_barriers(shm_ptr);
    
    /* Inizializzazione barriere di sincronizzazione utenti/gruppi */
    setup_group_barriers(shm_ptr);

    /* 5. Lancio Processi Figli */
    launch_simulation_operators(shm_ptr);
    launch_simulation_users(shm_ptr);

    /* Attesa della sincronizzazione di startup (Tutti i figli pronti) */
    synchronize_prework_barrier(shm_ptr);

    /* 6. Avvio Ciclo della Simulazione (Loop dei giorni) */
    start_simulation(shm_ptr);

    /* 7. Terminazione Coordinata */
    printf("[MASTER] Fine simulazione rilevata. Notifica ai figli e rimozione risorse...\n");
    terminate_simulation_gracefully(shm_ptr, EXIT_SUCCESS);

    return EXIT_SUCCESS;
}

/* ==========================================================================
 *                          SEZIONE: IMPLEMENTAZIONE
 * ========================================================================== */

void setup_prework_barrier(MainSharedMemory *shm_ptr) {
    /* Il numero totale di processi che devono raggiungere la barriera di startup */
    int total_processes = shm_ptr->configuration.quantities.number_of_workers + 
                        shm_ptr->configuration.seats.seats_cash_desk + 
                        shm_ptr->current_total_users;
    
    setup_barrier(shm_ptr->semaphore_sync_id, BARRIER_STARTUP_READY, BARRIER_STARTUP_GATE, total_processes);
}

void setup_daily_barriers(MainSharedMemory *shm_ptr) {
    int total_processes = shm_ptr->configuration.quantities.number_of_workers + 
                        shm_ptr->configuration.seats.seats_cash_desk + 
                        shm_ptr->current_total_users;
    
    /* Configura entrambe le barriere operative (Mattina/Sera) con il conteggio attuale della popolazione */
    setup_barrier(shm_ptr->semaphore_sync_id, BARRIER_MORNING_READY, BARRIER_MORNING_GATE, total_processes);
    setup_barrier(shm_ptr->semaphore_sync_id, BARRIER_EVENING_READY, BARRIER_EVENING_GATE, total_processes);
}

void synchronize_prework_barrier(MainSharedMemory *shm_ptr) {
    printf("[MASTER] In attesa dei figli per il via libera globale (Startup Barrier)...\n");
    
    /* Attesa che il contatore READY arrivi a 0 (Tutti i figli hanno chiamato sync_child_start) */
    wait_for_zero(shm_ptr->semaphore_sync_id, BARRIER_STARTUP_READY);
    
    /* Sblocco del cancello (GATE -> 0) per permettere ai figli di procedere */
    open_barrier_gate(shm_ptr->semaphore_sync_id, BARRIER_STARTUP_GATE);
    
    printf("[MASTER] Startup completata! Inizio servizio mensa.\n");
}

void start_simulation(MainSharedMemory *shm_ptr) {
    /* Chiamata al motore di simulazione definito in simulation_engine.c */
    run_simulation_loop(shm_ptr);
}
