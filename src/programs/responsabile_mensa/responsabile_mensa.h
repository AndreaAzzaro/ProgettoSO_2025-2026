/**
 * @file responsabile_mensa.h
 * @brief Header principale per il modulo Responsabile Mensa (Master).
 * 
 * Questo header funge da Facade (Punto di accesso unico) che aggrega 
 * i prototipi dei vari sotto-moduli del Master:
 * - setup_ipc: Gestione risorse condivise.
 * - setup_population: Calcolo e spawn dei figli.
 * - simulation_engine: Core loop temporale.
 */

#ifndef RESPONSABILE_MENSA_H
#define RESPONSABILE_MENSA_H

/* Includes del progetto */
#include "common.h"
#include "setup_ipc.h"
#include "setup_population.h"
#include "simulation_engine.h"
#include "config.h"
#include "menu.h"

/* ==========================================================================
 *                          PROTOTIPI FACADE (Core Infrastructure)
 * ========================================================================= */

/** 
 * @brief Inizializza tutte le risorse IPC necessarie per la simulazione.
 * @param shared_memory_ptr Puntatore alla memoria condivisa principale.
 */
void initialize_ipc_sources(MainSharedMemory *shared_memory_ptr);

/** 
 * @brief Prepara la barriera di sincronizzazione iniziale per lo startup di tutti i processi.
 * @param shm_ptr Puntatore alla memoria condivisa.
 */
void setup_prework_barrier(MainSharedMemory *shm_ptr);

/** 
 * @brief Gestisce l'attesa del Master sulla barriera di startup e l'apertura del cancello.
 * @param shm_ptr Puntatore alla memoria condivisa.
 */
void synchronize_prework_barrier(MainSharedMemory *shm_ptr);

/** 
 * @brief Configura le barriere giornaliere (Morning/Evening) in base alla popolazione corrente.
 * @param shm_ptr Puntatore alla memoria condivisa.
 */
void setup_daily_barriers(MainSharedMemory *shm_ptr);

/** 
 * @brief Avvia ufficialmente il ciclo di simulazione delegando al simulation_engine.
 * @param shm_ptr Puntatore alla memoria condivisa.
 */
void start_simulation(MainSharedMemory *shm_ptr);

/* ==========================================================================
 *                          PROTOTIPI FACADE (Popolazione)
 * ========================================================================= */

/** 
 * @brief Esegue lo spawn degli operatori (Distributori e Cassieri).
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void launch_simulation_operators(MainSharedMemory *shared_memory_ptr);

/** 
 * @brief Esegue lo spawn iniziale degli utenti.
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void launch_simulation_users(MainSharedMemory *shared_memory_ptr);

/* ==========================================================================
 *                          PROTOTIPI FACADE (Motore)
 * ========================================================================= */

/** 
 * @brief Avvia il loop dei giorni della simulazione. 
 * @param shm Puntatore alla memoria condivisa.
 */
void run_simulation_loop(MainSharedMemory *shm);

/** 
 * @brief Termina la simulazione in modo pulito, inviando segnali ai figli e rimuovendo IPC.
 * @param shm Puntatore alla memoria condivisa.
 * @param exit_code Codice di uscita da restituire al SO.
 */
void terminate_simulation_gracefully(MainSharedMemory *shm, int exit_code);

#endif /* RESPONSABILE_MENSA_H */
