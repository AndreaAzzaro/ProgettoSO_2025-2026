/**
 * @file conf.h
 * @brief Definizioni delle strutture dati per la configurazione della simulazione.
 * * Questo file contiene le struct che mappano i parametri letti dal file
 * di testo (es. config.txt) o passati da riga di comando.
 */

#ifndef CONF_H
#define CONF_H

/**
 * @brief Parametri quantitativi delle entità nella simulazione.
 * Gestisce il numero di persone e limiti di occorrenze.
 */
typedef struct {
    int n_workers;    /**< Numero totale di operatori attivi (cuochi + cassieri). */
    int n_users;      /**< Numero iniziale di utenti da lanciare. */
    int n_new_users;  /**< Numero di utenti generati periodicamente o in batch successivi. */
    int n_pause;      /**< Numero massimo (o durata) delle pause consentite. */
} ConfigQuantities;

/**
 * @brief Capacità delle risorse (Semafori).
 * Definisce quanti utenti possono essere serviti contemporaneamente in ogni stazione.
 * Esempio: primi = 3 significa che ci sono 3 posti liberi al bancone dei primi.
 */
typedef struct {
    int primi;    /**< Posti disponibili al bancone dei Primi. */
    int secondi;  /**< Posti disponibili al bancone dei Secondi. */
    int coffee;   /**< Posti disponibili al Bar/Dolci. */
    int cassa;    /**< Numero di casse attive (operatori cassa). */
    int seats;    /**< Numero totale di posti a sedere nella sala mensa. */
} ConfigSeats;

/**
 * @brief Prezzi unitari delle pietanze.
 * Usati per calcolare il conto totale alla cassa.
 */
typedef struct {
    int primi;    /**< Costo di un piatto di primi (in euro). */
    int secondi;  /**< Costo di un piatto di secondi. */
    int coffee;   /**< Costo di caffè o dessert. */
} ConfigPrice;

/**
 * @brief Parametri temporali della simulazione.
 * Definisce durate del servizio, velocità della simulazione e timeout.
 */
typedef struct {
    int sim_duration;       /**< Durata totale della simulazione in secondi reali. */
    long n_nano_secs;       /**< Fattore di conversione temporale (Time Scale Factor). */
    int avg_service_primi;  /**< Tempo medio di servizio ai Primi (in nanosec o unità sim). */
    int avg_service_main;   /**< Tempo medio di servizio ai Secondi. */
    int avg_service_coffee; /**< Tempo medio di servizio al Bar. */
    int avg_service_cassa;  /**< Tempo medio per il pagamento. */
    int avg_refill_time;    /**< Tempo necessario alla cucina per rifornire le scorte. */
    int stop_duration;      /**< Durata della pausa (disorder) o stop temporaneo. */
} ConfigTimings;

/**
 * @brief Soglie di allarme e limiti logici.
 */
typedef struct {
    int overload_threshold; /**< Numero max di persone in coda prima di dichiarare Overload. */
} ConfigThreshold;

/**
 * @brief Struttura Master di Configurazione.
 * Aggrega tutte le sottostrutture per facilitare il passaggio dei parametri
 * attraverso la Shared Memory.
 */
typedef struct {
    ConfigQuantities quantities; /**< Configurazioni numeriche (utenti, workers). */
    ConfigSeats seat;            /**< Capacità delle stazioni. */
    ConfigPrice price;           /**< Listino prezzi. */
    ConfigThreshold threshold;   /**< Soglie di sistema. */
    ConfigTimings timing;        /**< Tempi di servizio e durata. */
} SimConfig;

/**
 * @brief Carica la configurazione dal file di testo predefinito.
 * * Apre il file di config, esegue il parsing dei valori e popola la struct SimConfig.
 * In caso di errore di lettura o file mancante, termina il processo.
 * * @return SimConfig Una struct popolata con i valori letti.
 */
SimConfig loadConfig(); 

#endif