/**
 * @file operatore.h
 * @brief Header per il processo Operatore (Distributore pasti).
 * 
 * Gestisce l'interazione con le stazioni di distribuzione (Primi, Secondi, Bar).
 * Coordina lavoro, pause e sincronizzazione giornate.
 * 
 * @see operatore.c per l'implementazione.
 */

#ifndef OPERATORE_H
#define OPERATORE_H

/* Includes */
#include <sys/types.h>
#include "common.h"
#include "config.h"  /* Per FoodDistributionStation */

/**
 * @struct StatoOperatore
 * @brief Rappresenta lo stato interno e le statistiche di un singolo operatore di distribuzione.
 */
typedef struct {
    int shared_memory_id;               /**< ID della risorsa SHM passata via linea di comando */
    int station_type;                   /**< Bancone di appartenenza (0: PRIMI, 1: SECONDI, 2: BAR) */
    int assigned_post_index;            /**< ID numerico della postazione occupata all'interno del bancone */
    
    int total_portions_served;          /**< Statistica: piatti serviti nella giornata */
    int daily_breaks_taken;             /**< Statistica: numero di pause effettuate oggi */

    MainSharedMemory *shm_ptr;          /**< Puntatore alla memoria condivisa agganciata */
} StatoOperatore;

/**
 * @brief Inizializza la struttura stato operatore analizzando gli argomenti d'ingresso.
 * 
 * @param operatore Puntatore alla struttura da inizializzare.
 * @param argc Numero di argomenti da main.
 * @param argv Vettore degli argomenti da main.
 */
void init_operatore(StatoOperatore *operatore, int argc, char *argv[]);

/**
 * @brief Configura gli handler per i segnali asincroni (SIGUSR1, SIGUSR2, SIGTERM).
 */
void setup_operatore_signals(void);

/**
 * @brief Esegue il ciclo di vita principale dell'operatore.
 * 
 * Implementa 3 loop annidati:
 * 1. Loop Settimanale (Durata simulazione)
 * 2. Loop Giornaliero (Mattina -> Sera)
 * 3. Loop Turno (Postazione -> Servizio -> Pausa)
 * 
 * @param operatore Puntatore allo stato dell'operatore.
 */
void run_operatore_simulation(StatoOperatore *operatore);

/* ==========================================================================
 *                       SEZIONE: FUNZIONI DI FASE
 * ========================================================================== */

/**
 * @brief Prepara i riferimenti alla stazione e i tempi medi di servizio.
 */
void prepare_station_context(StatoOperatore *operatore, FoodDistributionStation **stazione_ptr, int *avg_service_time);

/**
 * @brief Implementa il ciclo di ricezione e servizio ordini (Loop 3).
 */
void fase_lavoro_stazione(StatoOperatore *operatore, FoodDistributionStation *stazione_ptr, int avg_service_time);

/**
 * @brief Gestisce il rilascio della postazione e la decisione atomica della pausa.
 */
void fase_decisione_pausa_atomica(StatoOperatore *operatore, FoodDistributionStation *stazione_ptr);

/**
 * @brief Simula il periodo di riposo dell'operatore.
 */
void esegui_pausa_operatore(StatoOperatore *operatore);

#endif
