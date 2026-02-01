/**
 * @file simulation_engine.h
 * @brief Header per il motore di simulazione del Responsabile Mensa.
 * 
 * Questo modulo gestisce il core loop temporale della simulazione, il controllo
 * dei timer POSIX, il rifornimento delle stazioni e la gestione dei segnali 
 * di broadcast verso gli utenti e gli operatori.
 * 
 * @see simulation_engine.c per l'implementazione della logica.
 */

#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

/* Includes di sistema */
#include <sys/types.h>
#include <signal.h>

/* Includes del progetto */
#include "common.h"

/* ==========================================================================
 *                       LOGICA CORE (LOOP SIMULAZIONE)
 * ========================================================================== */

/**
 * @brief Avvia il ciclo principale della simulazione per la durata totale prevista.
 * 
 * Coordina il passaggio dei giorni, l'attivazione dei timer quotidiani
 * e la sincronizzazione globale tra processi figli.
 * 
 * @param shm Puntatore alla memoria condivisa principale.
 */
void run_simulation_loop(MainSharedMemory *shm);

/**
 * @brief Configura e arma il timer POSIX per la fine della giornata lavorativa.
 * 
 * Al completamento del timer, il Master riceve un segnale (solitamente SIGALRM)
 * che innesca la fase di chiusura serale.
 * 
 * @param shm Puntatore alla memoria condivisa per i parametri di timing.
 */
void arm_daily_timer(MainSharedMemory *shm);

/* ==========================================================================
 *                        GESTIONE RIFORNIMENTO (REFILL)
 * ========================================================================== */

/**
 * @brief Gestisce un ciclo di rifornimento dei piatti nelle stazioni.
 * 
 * Incrementa le porzioni disponibili basandosi sui consumi rilevati
 * e i limiti configurati.
 * 
 * @param shm Puntatore alla memoria condivisa.
 */
void handle_refill_cycle(MainSharedMemory *shm);

/**
 * @brief Configura il timer/segnale per il rifornimento periodico.
 */
void setup_refill_signal(void);

/* ==========================================================================
 *                        GESTIONE SEGNALI E BROADCAST
 * ========================================================================== */

/**
 * @brief Configura l'handler per gestire la terminazione asincrona dei figli (waitpid).
 * 
 * Previene la creazione di processi zombie durante la simulazione.
 * 
 * @param shm Puntatore alla memoria condivisa.
 */
void setup_sigchld_handler(MainSharedMemory *shm);

/**
 * @brief Configura i segnali per la gestione della chiusura giornata (SIGINT, SIGALRM).
 * 
 * @param shm Puntatore alla memoria condivisa.
 */
void setup_signal_close_day(MainSharedMemory *shm);

/**
 * @brief Invia un segnale a tutti i processi registrati nella mappatura SHM.
 * 
 * Viene usato per notificare a tutti l'inizio/fine giornata o fine simulazione.
 * 
 * @param shm Puntatore alla memoria condivisa.
 * @param signal Il segnale POSIX da inviare (es. SIGUSR1, SIGUSR2).
 */
void broadcast_signal_to_all_groups(MainSharedMemory *shm, int signal);

/* ==========================================================================
 *                        SINCRONIZZAZIONE GRUPPI
 * ========================================================================== */

/**
 * @brief Inizializza o resetta le barriere di sincronizzazione per i gruppi.
 * 
 * Pulisce i semafori di gruppo (meeting point, tavoli, uscita) per una nuova giornata.
 * 
 * @param shm_ptr Puntatore alla memoria condivisa.
 */
void setup_group_barriers(MainSharedMemory *shm_ptr);

#endif /* SIMULATION_ENGINE_H */
