/**
 * @file utente.h
 * @brief Header per il processo Utente (Cliente della mensa).
 * 
 * Definisce lo stato interno dell'utente, le fasi del suo percorso
 * all'interno della simulazione e i meccanismi di sincronizzazione di gruppo.
 * 
 * @see utente.c per l'implementazione della logica.
 */

#ifndef UTENTE_H
#define UTENTE_H

/* Includes di sistema */
#include <stdbool.h>
#include <sys/types.h>

/* Includes del progetto */
#include "common.h"
#include "queue.h"    /* Per SimulationMessage */
#include "config.h"   /* Per FoodDistributionStation */

/* ==========================================================================
 *                          STRUTTURE DATI (USER STATE)
 * ========================================================================== */

/**
 * @struct StatoUtente
 * @brief Rappresenta lo stato interno e le proprietà di un utente della mensa.
 * 
 * Contiene i dati necessari per gestire il flusso operativo, le scelte del menu
 * e i riferimenti alle risorse IPC.
 */
typedef struct {
    int shared_memory_id;               /**< ID della risorsa SHM passata via linea di comando */
    bool has_ticket;                    /**< Possesso iniziale del ticket sconto (80% dei casi) */
    bool ticket_is_validated;           /**< Stato di convalida del ticket dopo la fase ticket */
    
    int group_id;                       /**< Indice del gruppo utile per la sincronizzazione IPC */
    int group_size;                     /**< Numero totale di membri del gruppo */
    bool is_group_leader;               /**< Flag di leadership per la prenotazione dei tavoli */

    MainSharedMemory *shm_ptr;          /**< Puntatore alla memoria condivisa agganciata */
    
    /* Scelte Menu del giorno (Indici negli array del Menu in SHM) */
    int selected_first_course_index;     /**< Scelta random per il primo piatto */
    int selected_second_course_index;    /**< Scelta random per il secondo piatto */
    int selected_dessert_coffee_index;   /**< Scelta random per il caffè/dolce */

    int group_patience_threshold;       /**< Minuti simulati di attesa tollerati prima dell'abbandono */

    /* Social Seating (Step 3) */
    int assigned_table_id;              /**< ID del tavolo occupato dal gruppo (-1 se nessuno) */
} StatoUtente;

/* ==========================================================================
 *                         LOGICA PRINCIPALE (CICLO VITA)
 * ========================================================================== */

/**
 * @brief Inizializza lo stato utente analizzando gli argomenti d'ingresso.
 * 
 * @param utente Puntatore alla struttura da inizializzare.
 * @param argc Conteggio argomenti da main.
 * @param argv Vettore argomenti da main.
 */
void init_utente(StatoUtente *utente, int argc, char *argv[]);

/**
 * @brief Esegue il ciclo di vita principale (Start -> Giorno -> Fine).
 * 
 * Coordina tutte le fasi (ticket, distribuzione, cassa, consumo) finché
 * la simulazione è attiva.
 * 
 * @param utente Puntatore allo stato utente.
 */
void run_utente_simulation(StatoUtente *utente);

/** @brief Gestisce la sincronizzazione di startup e leadership iniziale. */
void sincronizza_startup_utente(StatoUtente *utente);

/** @brief Esegue l'intera sequenza di azioni quotidiane dell'utente. */
void esegui_percorso_mensa_giornaliero(StatoUtente *utente);

/** @brief Resetta le flag e rigenera l'id casuale all'inizio della giornata. */
void reset_stato_giornaliero_utente(StatoUtente *utente);

/* ==========================================================================
 *                       UTILITY INTERNE (PROTOTIPI)
 * ========================================================================== */

/** @brief Gestisce i segnali asincroni (SIGUSR2, SIGTERM, SIGINT). */
void handle_utente_signals(int sig);

/** @brief Configura gli handler per i segnali asincroni. */
void setup_utente_signals(void);

/** @brief Definisce il profilo casuale (ticket, gusti, pazienza). */
void genera_identita_casuale(StatoUtente *utente);

/** @brief Gestisce l'invio dell'ordine via MQ e l'attesa della risposta. */
bool fase_checkout_piatto(StatoUtente *utente, FoodDistributionStation *stazione, int *choice, int stazione_tipo);

/** @brief Ricezione da MQ con timeout soft per evitare blocchi infiniti. */
ssize_t receive_message_with_soft_timeout(int queue_id, SimulationMessage *msg, size_t size, long type);

/** @brief Converte delta temporali in minuti simulati. */
double get_simulated_minutes(struct timespec start, struct timespec end, long nanosecs_per_tick);

/** @brief Aggiorna gli accumulatori delle statistiche in SHM. */
void update_wait_time_stat(StatoUtente *utente, double wait_min, int type);

/* ==========================================================================
 *                          FASI OPERATIVE (MODULARI)
 * ========================================================================== */

/** @brief Gestisce la coda e la convalida del ticket d'ingresso. */
void fase_validazione_ticket(StatoUtente *utente);

/** 
 * @brief Gestisce l'interazione con Primi o Secondi (include logica di ripiego). 
 * @param utente Stato utente.
 * @param stazione_tipo 0 per Primi, 1 per Secondi.
 * @return true se il piatto è stato ottenuto, false altrimenti.
 */
bool fase_servizio_stazione(StatoUtente *utente, int stazione_tipo);

/** @brief Gestisce l'abbandono forzato, sbloccando i semafori del gruppo. */
void fase_ritiro_formale(StatoUtente *utente);

/** @brief Coordina la riunione del gruppo al meeting point pre-cassa. */
void fase_riunione_gruppo(StatoUtente *utente);

/** 
 * @brief Invia i dati al cassiere e attende lo scontrino. 
 * @param utente Stato utente.
 * @param p1 Vero se ha ricevuto il primo.
 * @param p2 Vero se ha ricevuto il secondo.
 */
void fase_pagamento_cassa(StatoUtente *utente, bool p1, bool p2);

/** @brief Il leader prenota il tavolo per tutti o gli utenti attendono il via. */
void fase_prenotazione_tavolo(StatoUtente *utente);

/** @brief Simula la consumazione e rilascia il posto a sedere nei posti SHM. */
void fase_consumazione_pasto(StatoUtente *utente, bool p1, bool p2);

/** @brief Interagisce con la stazione Bar/Dolci. */
void fase_servizio_caffe(StatoUtente *utente);

/** @brief Sincronizza il gruppo per l'uscita simultanea dalla mensa. */
void fase_uscita_collettiva(StatoUtente *utente);

/* ==========================================================================
 *                         STATISTICHE E REPORTING
 * ========================================================================== */

/** @brief Incrementa i contatori globali per utenti serviti con successo. */
void aggiorna_statistiche_servito(StatoUtente *utente);

/** @brief Incrementa i contatori globali per mancato servizio/abbandono. */
void aggiorna_statistiche_non_servito(StatoUtente *utente);

#endif /* UTENTE_H */
