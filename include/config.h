/**
 * @file config.h
 * @brief Definizioni delle strutture dati per la configurazione della simulazione.
 * 
 * Questo modulo definisce le strutture che incapsulano tutti i parametri
 * configurabili della simulazione, caricati dal file `config/config.conf`.
 * 
 * @see load_simulation_configuration() per il caricamento dei valori.
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ==========================================================================
 *                      SEZIONE: STRUTTURE DI CONFIGURAZIONE
 * ========================================================================== */

/**
 * @brief Parametri quantitativi delle entità nella simulazione.
 */
typedef struct {
    int number_of_workers;              /**< Numero totale di operatori attivi */
    int number_of_initial_users;        /**< Numero iniziale di utenti all'avvio */
    int number_of_new_users_batch;     /**< Numero di utenti aggiunti in ogni batch */
    int number_of_allowed_breaks;       /**< Numero massimo di pause consentite per operatore */
    int maximum_users_per_group;        /**< Dimensione massima di un gruppo di utenti */
} ConfigurationQuantities;

/**
 * @brief Capacità e posti disponibili nelle varie aree.
 */
typedef struct {
    int seats_first_course;            /**< Posti al bancone dei Primi Piatti */
    int seats_second_course;           /**< Posti al bancone dei Secondi Piatti */
    int seats_coffee_dessert;          /**< Posti al bancone Bar/Dolci */
    int seats_cash_desk;               /**< Postazioni attive in Cassa */
    int total_dining_seats;            /**< NOFTABLESEATS: Posti a sedere totali nell'area refezione */
} ConfigurationSeats;

/**
 * @brief Prezzi unitari per tipologia di piatto (in EUR).
 */
typedef struct {
    double price_first_course;          /**< Prezzo di un primo piatto */
    double price_second_course;         /**< Prezzo di un secondo piatto */
    double price_coffee_dessert;        /**< Prezzo di caffè/dolce */
} ConfigurationPrices;

/**
 * @brief Parametri temporali della simulazione.
 * 
 * Definisce durate, tempi medi di servizio e il fattore di scala temporale.
 */
typedef struct {
    int simulation_duration_days;       /**< Numero di giorni totali della simulazione (SIM_DURATION) */
    int meal_duration_minutes;          /**< Durata fittizia di un pasto in minuti simulati */
    long nanoseconds_per_tick;          /**< Nanosecondi reali per minuto simulato (scala tempo) */
    int average_service_time_primi;     /**< Tempo medio servizio stazione Primi (sec simulati) */
    int average_service_time_secondi;   /**< Tempo medio servizio stazione Secondi (sec simulati) */
    int average_service_time_coffee;    /**< Tempo medio servizio stazione Caffè (sec simulati) */
    int average_service_time_cassa;     /**< Tempo medio servizio in Cassa (sec simulati) */
    int average_service_time_ticket;    /**< Tempo medio per la validazione del ticket */
    int average_refill_time;            /**< Tempo medio per un rifornimento stazione */
    int stop_duration_minutes;          /**< Durata del blocco Communication Disorder (sec reali) */
} ConfigurationTimings;

/**
 * @brief Soglie operative e limiti di rifornimento.
 */
typedef struct {
    int overload_threshold;             /**< Soglia di carico per attivazione logiche di emergenza */
    int maximum_portions_primi;         /**< Capacità massima scorte Primi */
    int maximum_portions_secondi;       /**< Capacità massima scorte Secondi */
    int refill_amount_primi;            /**< Quantità aggiunta ad ogni refill Primi */
    int refill_amount_secondi;          /**< Quantità aggiunta ad ogni refill Secondi */
    int queue_patience_threshold;       /**< Tempo massimo di attesa prima che un utente abbandoni */
} ConfigurationThresholds;

/**
 * @brief Struttura Master di Configurazione.
 */
typedef struct {
    ConfigurationQuantities quantities;
    ConfigurationSeats seats;
    ConfigurationPrices prices;
    ConfigurationThresholds thresholds;
    ConfigurationTimings timings;
} SimulationConfiguration;

/* ==========================================================================
 *                         SEZIONE: FUNZIONI PUBBLICHE
 * ========================================================================== */

/**
 * @brief Carica la configurazione dal file di sistema.
 * 
 * Legge il file specificato (o config/config.conf di default) e popola la struttura SimulationConfiguration.
 * In caso di errore di lettura, termina il processo con EXIT_FAILURE.
 * 
 * @param filepath Percorso al file di configurazione (NULL per il default).
 * @return SimulationConfiguration Struttura popolata con i parametri letti.
 */
SimulationConfiguration load_simulation_configuration(const char *filepath);

#endif /* CONFIG_H */