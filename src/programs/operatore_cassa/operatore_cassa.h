/**
 * @file operatore_cassa.h
 * @brief Header per il processo Operatore di Cassa (Cassiere).
 * 
 * Gestisce l'incasso dei pagamenti, il calcolo dei totali giornalieri
 * e la sincronizzazione con gli utenti in uscita.
 * 
 * @see operatore_cassa.c per l'implementazione.
 */

#ifndef OPERATORE_CASSA_H
#define OPERATORE_CASSA_H

/* Includes */
#include <sys/types.h>
#include "common.h"

/**
 * @struct StatoCassiere
 * @brief Rappresenta lo stato interno e le statistiche di un singolo operatore di cassa.
 */
typedef struct {
    int shared_memory_id;               /**< ID della risorsa SHM passata via linea di comando */
    int assigned_checkout_index;        /**< ID numerico della postazione cassa occupata */
    
    int total_customers_processed;      /**< Statistica: numero di pagamenti gestiti oggi */
    int daily_breaks_taken;             /**< Statistica: numero di pause effettuate oggi */

    MainSharedMemory *shm_ptr;          /**< Puntatore alla memoria condivisa agganciata */
} StatoCassiere;

/**
 * @brief Inizializza la struttura stato cassiere analizzando gli argomenti d'ingresso.
 * 
 * @param cassiere Puntatore alla struttura da inizializzare.
 * @param argc Numero di argomenti da main.
 * @param argv Vettore degli argomenti da main.
 */
void init_cassiere(StatoCassiere *cassiere, int argc, char *argv[]);

/**
 * @brief Configura gli handler per i segnali asincroni (SIGUSR1, SIGUSR2, SIGTERM).
 */
void setup_cassiere_signals(void);

/**
 * @brief Esegue il ciclo di vita principale del cassiere.
 * 
 * Implementa 3 loop annidati:
 * 1. Loop Settimanale (Durata simulazione)
 * 2. Loop Giornaliero (Mattina -> Sera)
 * 3. Loop Turno (Postazione -> Cassa -> Pausa)
 * 
 * @param cassiere Puntatore allo stato del cassiere.
 */
void run_cassiere_simulation(StatoCassiere *cassiere);

/* ==========================================================================
 *                       SEZIONE: FUNZIONI DI FASE
 * ========================================================================== */

/**
 * @brief Implementa il ciclo di ricezione pagamenti (Loop 3).
 */
void fase_lavoro_cassa(StatoCassiere *cassiere);

/**
 * @brief Gestisce il rilascio della cassa e la decisione atomica della pausa.
 */
void fase_decisione_pausa_cassa(StatoCassiere *cassiere);

/**
 * @brief Simula il periodo di riposo del cassiere.
 */
void esegui_pausa_cassa(StatoCassiere *cassiere);

#endif
