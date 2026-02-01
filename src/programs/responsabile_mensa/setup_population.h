/**
 * @file setup_population.h
 * @brief Header per la configurazione e il lancio della popolazione della simulazione.
 * 
 * Questo modulo gestisce il calcolo della distribuzione degli operatori nelle 
 * stazioni e lo spawn dei processi figli (Operatori, Cassieri, Utenti).
 */

#ifndef SETUP_POPULATION_H
#define SETUP_POPULATION_H

/* Includes del progetto */
#include "common.h"

/* ==========================================================================
 *                       DISTRIBUZIONE E CALCOLO
 * ========================================================================== */

/**
 * @brief Calcola matematicamente la distribuzione dei lavoratori nelle stazioni.
 * 
 * Distribuisce N lavoratori in base ai tempi medi di servizio, garantendo
 * che le stazioni più lente ricevano proporzionalmente più personale.
 * 
 * @param total_workers Numero totale di lavoratori da distribuire.
 * @param average_times_array Array dei tempi medi di servizio per stazione.
 * @param distribution_results_array Array in cui salvare il numero di operatori calcolati.
 * @param stations_count Numero di stazioni totali.
 */
void calculate_worker_distribution(int total_workers, int *average_times_array, int *distribution_results_array, int stations_count);

/**
 * @brief Imposta la distribuzione degli operatori basandosi sulla configurazione.
 * 
 * @param shm_ptr Puntatore alla memoria condivisa principale.
 */
void setup_worker_distribution(MainSharedMemory *shm_ptr);

/**
 * @brief Calcola il numero di gruppi necessari per la popolazione iniziale di utenti.
 * 
 * Suddivide gli utenti iniziali in gruppi di dimensione casuale.
 * 
 * @param shm_ptr Puntatore alla memoria condivisa.
 * @return int Numero di gruppi creati (incluso un margine per espansioni future).
 */
int calculate_initial_groups_count(MainSharedMemory *shm_ptr);

/* ==========================================================================
 *                          SPAWN DEI PROCESSI
 * ========================================================================== */

/**
 * @brief Esegue il lancio (fork ed exec) di tutti i processi operatore e cassiere.
 * 
 * @param shared_memory_ptr Puntatore alla struttura di memoria condivisa.
 */
void launch_simulation_operators(MainSharedMemory *shared_memory_ptr);

/**
 * @brief Esegue lo spawn dei processi utente raggruppandoli in branchie di amici.
 * 
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 */
void launch_simulation_users(MainSharedMemory *shared_memory_ptr);

/* ==========================================================================
 *                       INIZIALIZZAZIONE RISORSE
 * ========================================================================== */

/**
 * @brief Inizializza i semafori dei posti operatore e dei tavoli.
 * 
 * Questa funzione deve essere chiamata dopo lo spawn o il calcolo della distribuzione.
 * 
 * @param shm_ptr Puntatore alla memoria condivisa.
 */
void initialize_station_operator_semaphores(MainSharedMemory *shm_ptr);

#endif /* SETUP_POPULATION_H */
