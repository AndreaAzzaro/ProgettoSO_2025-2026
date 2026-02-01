/**
 * @file setup_ipc.h
 * @brief Header per l'inizializzazione delle risorse IPC del Responsabile Mensa.
 * 
 * Questo modulo gestisce l'allocazione e la configurazione iniziale di semafori, 
 * code di messaggi e memoria condivisa necessari per il funzionamento della simulazione.
 * 
 * @see setup_ipc.c per l'implementazione della logica.
 */

#ifndef SETUP_IPC_H
#define SETUP_IPC_H

/* Includes del progetto */
#include "common.h"

/* ==========================================================================
 *                          ALLOCAZIONE RISORSE
 * ========================================================================== */

/**
 * @brief Alloca e inizializza il segmento principale di memoria condivisa.
 * 
 * Utilizza una dimensione dinamica per supportare il Flexible Array Member 
 * dedicato allo stato dei gruppi.
 * 
 * @param group_pool_size Numero di slot per lo stato dei gruppi nel pool.
 * @return MainSharedMemory* Puntatore all'area di memoria condivisa agganciata.
 */
MainSharedMemory* initialize_simulation_shared_memory(int group_pool_size);

/**
 * @brief Orchestratore globale per l'inizializzazione di tutte le risorse IPC.
 * 
 * Inizializza barriere, mutex, stazioni di distribuzione, cassa e strutture di controllo.
 * 
 * @param shm_ptr Puntatore alla memoria condivisa principale.
 */
void initialize_ipc_sources(MainSharedMemory *shm_ptr);

/* ==========================================================================
 *                       INIZIALIZZAZIONE SPECIFICA
 * ========================================================================== */

/**
 * @brief Inizializza le barriere di sincronizzazione per lo startup globale.
 * 
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void initialize_simulation_start_barriers(MainSharedMemory *shared_memory_ptr);

/**
 * @brief Inizializza il pool di semafori per la sincronizzazione dei gruppi di utenti.
 * 
 * @param shm_ptr Puntatore alla memoria condivisa.
 * @param pool_size Numero di entry (slot) da creare nel pool.
 */
void initialize_group_sync_pool(MainSharedMemory *shm_ptr, int pool_size);

/**
 * @brief Inizializza le barriere di sincronizzazione per i cicli giornalieri (Mattina/Sera).
 * 
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void initialize_daily_cycle_barriers(MainSharedMemory *shared_memory_ptr);

/**
 * @brief Crea e inizializza i semafori Mutex per la protezione dei dati globali.
 * 
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void initialize_global_simulation_mutexes(MainSharedMemory *shared_memory_ptr);

/**
 * @brief Inizializza le code di messaggi e i semafori per le stazioni di cibo.
 * 
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void initialize_distribution_stations(MainSharedMemory *shared_memory_ptr);

/**
 * @brief Inizializza il semaforo a conteggio per la gestione dei posti a sedere.
 * 
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void initialize_dining_area_seats_semaphores(MainSharedMemory *shared_memory_ptr);

/**
 * @brief Inizializza il semaforo per la validazione dei ticket all'ingresso.
 * 
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void initialize_ticket_validation_semaphores(MainSharedMemory *shared_memory_ptr);

/**
 * @brief Crea la coda di messaggi e i semafori per la stazione di Cassa.
 * 
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void initialize_cashier_checkout_message_queue(MainSharedMemory *shared_memory_ptr);

/**
 * @brief Inizializza la coda di controllo e le strutture per l'aggiunta dinamica di utenti.
 * 
 * @param shm_ptr Puntatore alla memoria condivisa.
 */
void initialize_control_structures(MainSharedMemory *shm_ptr);

#endif /* SETUP_IPC_H */
