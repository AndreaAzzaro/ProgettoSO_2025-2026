/**
 * @file statistics.h
 * @brief Definizioni per la raccolta e visualizzazione delle statistiche di simulazione.
 * 
 * Questo modulo definisce le strutture dati per tracciare:
 * - Piatti serviti e avanzati (giornalieri e totali)
 * - Tempi di attesa medi per stazione
 * - Flussi clienti (serviti, non serviti, con/senza ticket)
 * - Performance operatori e incassi
 * 
 * @see collect_simulation_statistics() per la raccolta dati.
 */

#ifndef STATISTICS_H
#define STATISTICS_H

/** Forward declaration per evitare dipendenze circolari con common.h */
struct MainSharedMemory;

/* ==========================================================================
 *                           SEZIONE: ENUMERAZIONI
 * ========================================================================== */

/**
 * @brief Identificatori per le cause di terminazione della simulazione.
 */
typedef enum {
    TERMINATION_REASON_NOT_TERMINATED = 0,
    TERMINATION_REASON_TIMEOUT,          /**< Raggiunta durata SIMURATION_DAYS */
    TERMINATION_REASON_OVERLOAD,         /**< Numero utenti in attesa > OVERLOAD_THRESHOLD */
    TERMINATION_REASON_SIGNAL            /**< Terminazione manuale via segnale (es. SIGINT) */
} TerminationReason;

/* ==========================================================================
 *                         SEZIONE: STRUTTURE PIATTI
 * ========================================================================== */

/**
 * @brief Conteggio dei piatti serviti o avanzati.
 */
typedef struct {
    int first_course_count;           /**< Numero di primi piatti */
    int second_course_count;          /**< Numero di secondi piatti */
    int coffee_dessert_count;         /**< Numero di caffè/dolci */
    int total_plates_count;           /**< Totale piatti (somma dei precedenti) */
} StatisticsPlateCounts;

/**
 * @brief Medie giornaliere dei piatti.
 */
typedef struct {
    double average_daily_first_courses;   /**< Media primi/giorno */
    double average_daily_second_courses;  /**< Media secondi/giorno */
    double average_daily_coffee_dessert;  /**< Media caffè-dolci/giorno */
    double average_daily_total;           /**< Media totale piatti/giorno */
} StatisticsPlateAverages;

/* ==========================================================================
 *                       SEZIONE: STRUTTURE TEMPI ATTESA
 * ========================================================================== */

/**
 * @brief Accumulatori per il calcolo preciso dei tempi di attesa.
 * 
 * Ogni stazione ha una somma dei tempi e un contatore di campioni
 * per permettere il calcolo della media alla fine della giornata.
 */
typedef struct {
    double sum_wait_first;    /**< Somma tempi attesa stazione Primi */
    int count_first;          /**< Numero campioni Primi */
    double sum_wait_second;   /**< Somma tempi attesa stazione Secondi */
    int count_second;         /**< Numero campioni Secondi */
    double sum_wait_coffee;   /**< Somma tempi attesa stazione Caffè */
    int count_coffee;         /**< Numero campioni Caffè */
    double sum_wait_cashier;  /**< Somma tempi attesa Cassa */
    int count_cashier;        /**< Numero campioni Cassa */
} WaitTimeAccumulator;

/**
 * @brief Tempi di attesa medi espressi in minuti simulati.
 */
typedef struct {
    double average_wait_first_course;   /**< Tempo medio attesa Primi */
    double average_wait_second_course;  /**< Tempo medio attesa Secondi */
    double average_wait_coffee_dessert; /**< Tempo medio attesa Caffè/Dolce */
    double average_wait_cash_desk;      /**< Tempo medio attesa Cassa */
    double average_wait_global;         /**< Tempo medio globale (tutte le stazioni) */
} StatisticsWaitTimes;

/* ==========================================================================
 *                    SEZIONE: STRUTTURE CLIENTI E OPERATORI
 * ========================================================================== */

/**
 * @brief Statistiche relative ai flussi dei clienti (Utenti).
 */
typedef struct {
    int daily_clients_served;             /**< Clienti serviti oggi */
    int daily_clients_not_served;         /**< Clienti non serviti oggi */
    int daily_clients_with_ticket;        /**< Clienti con ticket oggi */
    int daily_clients_without_ticket;     /**< Clienti senza ticket oggi */
    int total_clients_served;             /**< Totale clienti serviti nella simulazione */
    int total_clients_not_served;         /**< Totale clienti che hanno rinunciato */
    double average_daily_clients_served;  /**< Media clienti serviti/giorno */
    double average_daily_clients_not_served; /**< Media rinunce/giorno */
    int total_clients_with_ticket;        /**< Totale clienti con ticket */
    int total_clients_without_ticket;     /**< Totale clienti senza ticket */
} StatisticsClientData;

/**
 * @brief Performance e attività degli operatori della mensa.
 */
typedef struct {
    int total_active_operators_all_time; /**< Totale operatori attivati nella simulazione */
    int daily_active_operators;          /**< Operatori attivi nel giorno corrente */
    int daily_breaks_taken;              /**< Pause effettuate oggi */
    int total_breaks_taken;              /**< Totale pause effettuate nella simulazione */
    double average_daily_breaks;         /**< Media pause/giorno */
} StatisticsOperatorData;

/* ==========================================================================
 *                         SEZIONE: STRUTTURE INCASSI
 * ========================================================================== */

/**
 * @brief Analisi dei flussi finanziari della simulazione.
 */
typedef struct {
    double accumulated_total_income;  /**< Incasso totale accumulato */
    double current_daily_income;      /**< Incasso del giorno corrente */
    double average_daily_income;      /**< Media incasso/giorno */
} StatisticsIncomeData;

/* ==========================================================================
 *                      SEZIONE: STRUTTURA AGGREGATA
 * ========================================================================== */

/**
 * @brief Struttura aggregata delle Statistiche di Simulazione.
 * 
 * Contiene l'intero set di dati storici e giornalieri necessari
 * per il report finale e il monitoraggio in tempo reale.
 */
typedef struct {
    StatisticsPlateCounts daily_served_plates;      /**< Piatti serviti oggi */
    StatisticsPlateCounts total_served_plates;      /**< Piatti serviti totali nella simulazione */
    StatisticsPlateCounts daily_leftover_plates;    /**< Piatti avanzati oggi */
    StatisticsPlateCounts total_leftover_plates;    /**< Piatti avanzati totali nella simulazione */
    
    StatisticsPlateAverages average_daily_served_plates;   
    StatisticsPlateAverages average_daily_leftover_plates; 
    
    StatisticsWaitTimes total_average_wait_times;  /**< Medie complessive della simulazione */
    StatisticsWaitTimes daily_average_wait_times;  /**< Medie del giorno corrente */
    
    WaitTimeAccumulator daily_wait_accumulators;  /**< Accumulatori per il giorno corrente */
    WaitTimeAccumulator total_wait_accumulators;  /**< Accumulatori storici per l'intera simulazione */
    
    StatisticsClientData clients_statistics;
    StatisticsOperatorData operators_statistics;
    StatisticsIncomeData income_statistics;

    TerminationReason reason_for_termination; /**< Causa della fine della simulazione */
} SimulationStatistics;

/* ==========================================================================
 *                         SEZIONE: FUNZIONI PUBBLICHE
 * ========================================================================== */

/**
 * @brief Legge e calcola le statistiche attuali dalla memoria condivisa.
 * 
 * @param shared_memory_ptr Puntatore alla memoria condivisa principale.
 * @return SimulationStatistics Struttura popolata con i dati estratti.
 */
SimulationStatistics collect_simulation_statistics(struct MainSharedMemory *shared_memory_ptr);

/**
 * @brief Visualizza a terminale un report dettagliato dei dati giornalieri.
 * 
 * @param statistics Struttura contenente i dati da stampare.
 * @param simulation_day Indice del giorno corrente (partendo da 0).
 */
void display_daily_statistics_report(SimulationStatistics statistics, int simulation_day);

/**
 * Stampa a terminale il report finale complessivo della simulazione.
 * @param s Struttura statistiche con i dati accumulati.
 * @param total_days Numero totale di giorni simulati.
 */
void display_final_simulation_report(SimulationStatistics s, int total_days);

/**
 * @brief Salva le statistiche giornaliere in un file CSV.
 * 
 * Se il file non esiste, viene creato con l'header. Se esiste, i dati vengono
 * appesi in coda.
 * 
 * @param statistics Struttura contenente i dati da salvare.
 * @param simulation_day Indice del giorno corrente (partendo da 0).
 * @param filepath Percorso del file CSV di destinazione.
 * @return int 0 se successo, -1 se errore.
 */
int save_statistics_to_csv(SimulationStatistics statistics, int simulation_day, const char *filepath);

#endif /* STATISTICS_H */
