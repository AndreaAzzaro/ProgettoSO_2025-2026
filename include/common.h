/**
 * @file common.h
 * @brief Definizioni globali, strutture dati della memoria condivisa e indici IPC.
 * 
 * Questo file centralizza tutte le strutture e le costanti necessarie per la 
 * comunicazione tra i diversi processi della simulazione (Responsabile, Operatori, Utenti).
 */

#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#include "statistics.h"
#include "menu.h"
#include "message.h"

/** Percorso e ID per la generazione delle chiavi IPC tramite ftok() */
#define IPC_KEY_PATH "config/config.conf"
#define IPC_PROJECT_ID 'A'

/** Numero di postazioni per la validazione automatica dei ticket all'ingresso */
#define TICKET_VALIDATORS_COUNT 4

/** Numero massimo di utenti per gruppo di amici */
#define MAX_USERS_PER_GROUP 8

/** Numero massimo di tavoli gestibili nell'area refezione */
#define MAX_TABLES 128

/* ==========================================================================
 *                         SEZIONE: INDICI SEMAFORICI
 * ========================================================================== */

/** Offsets semaforici per ogni entry del pool di gruppo */
typedef enum {
    GROUP_SEM_PRE_CASHIER = 0,  /**< Barriera pre-cassa */
    GROUP_SEM_TABLE_GATE = 1,   /**< Gate tavolo (Leader-Scout) */
    GROUP_SEM_EXIT = 2,         /**< Barriera uscita pasto */
    GROUP_SEMS_PER_ENTRY = 3    /**< Numero di semafori per ogni slot del pool */
} GroupSemaphoreOffset;

/**
 * @brief Indici per l'identificazione dei gruppi di processi.
 * Utile per la gestione dei segnali di gruppo (PGID).
 */
typedef enum {
    GROUP_CASHIERS,         /**< Gruppo degli Operatori di Cassa */
    GROUP_FIRST_COURSES,    /**< Gruppo degli Operatori dei Primi Piatti */
    GROUP_SECOND_COURSES,   /**< Gruppo degli Operatori dei Secondi Piatti */
    GROUP_DESSERT_COFFEE,   /**< Gruppo degli Operatori di Bar/Dolci */
    GROUP_USERS,            /**< Gruppo degli Utenti della mensa */
    MAX_PROCESS_GROUPS      /**< Numero totale di gruppi gestiti */
} ProcessGroupIndex;

/**
 * @brief Indici per il set di semafori della Barriera di Sincronizzazione Giornaliera.
 * Implementa il pattern "Ping-Pong Barrier" per l'avvio e la fine del giorno.
 */
typedef enum {
    BARRIER_STARTUP_READY = 0,  /**< Startup: Conteggio processi pronti all'avvio iniziale */
    BARRIER_STARTUP_GATE,       /**< Startup: Cancello di avvio globale (aperto dal Direttore) */
    BARRIER_MORNING_READY,      /**< Mattina: Conteggio processi pronti alla partenza giornaliera */
    BARRIER_MORNING_GATE,       /**< Mattina: Cancello di avvio giornaliero */
    BARRIER_EVENING_READY,      /**< Sera: Conteggio processi arrivati a fine giornata */
    BARRIER_EVENING_GATE,       /**< Sera: Cancello di chiusura/reset giorno e sblocco */
    SYNC_BARRIER_SEM_COUNT      /**< Totale semafori in questo set (incrementato a 6) */
} SyncBarrierIndex;

/**
 * @brief Indici per il set di semafori Mutex.
 * Garantiscono l'accesso atomico alle sezioni critiche della memoria condivisa.
 */
typedef enum {
    MUTEX_SIMULATION_STATS = 0, /**< Protegge la struttura delle statistiche globali */
    MUTEX_SHARED_DATA,          /**< Protegge i campi comuni della memoria condivisa (es. giorno corrente) */
    MUTEX_ADD_USERS_PERMISSION, /**< Permesso per i processi add_users di procedere */
    MUTEX_TABLES,               /**< Protegge l'accesso atomico all'array dei tavoli */
    MUTEX_SEMAPHORE_COUNT       /**< Numero totale di semafori mutex */
} MutexSemaphoreIndex;

/**
 * @brief Indici per i semafori di controllo di ogni stazione.
 */
typedef enum {
    STATION_SEM_AVAILABLE_POSTS = 0, /**< Semaforo a conteggio per i posti operatore disponibili */
    STATION_SEM_USER_QUEUE,          /**< Sincronizzazione per l'attesa degli utenti alla stazione */
    STATION_SEM_REFILL_GATE,         /**< Cancello refill: 0=Aperto (wait_for_zero passa), >0=Chiuso (operatori bloccati) */
    STATION_SEM_REFILL_ACK,          /**< Sincronizzazione (Appello) per il risveglio post-rifornimento */
    STATION_SEM_STOP_GATE,           /**< Cancello Communication Disorder: 0=Operativo, >0=Bloccato */
    STATION_SEM_COUNT                /**< Totale semafori per stazione */
} StationSemaphoreIndex;

/**
 * @brief Rappresentazione di una stazione di distribuzione cibo.
 */
typedef struct {
    int message_queue_id;               /**< ID della coda di messaggi per gli ordini */
    int semaphore_set_id;               /**< ID del set di semafori della stazione (StationSemaphoreIndex) */
    int num_operators_assigned;         /**< Numero di operatori assegnati a questa stazione */
    int portions[MAX_DISHES_PER_CATEGORY];  /**< Disponibilità fisica delle porzioni di cibo */
} FoodDistributionStation;

/**
 * @brief Rappresentazione della stazione di pagamento (Cassa).
 */
typedef struct {
    int message_queue_id;               /**< ID della coda di messaggi per i pagamenti */
    int semaphore_set_id;               /**< ID del set di semafori (Cassa) */
    double daily_income;                /**< Incasso specifico della giornata corrente */
    double total_income;                /**< Incasso totale accumulato nella simulazione */
} CashierStation;

/* ==========================================================================
 *                         SEZIONE: STRUTTURE DATI
 * ========================================================================== */

/** Capacità massima del registry per il tracciamento dei processi utente */
#define MAX_USERS_REGISTRY 4096

/**
 * @brief Informazioni di tracciamento per ogni processo utente.
 * Usato dal Master per gestire la morte asincrona e le barriere di gruppo.
 */
typedef struct {
    pid_t pid;                          /**< PID del processo */
    int group_index;                    /**< Indice del gruppo di appartenenza */
} UserProcessMetadata;

/**
 * @brief Rappresentazione di un singolo tavolo della mensa.
 */
typedef struct {
    int id;               /**< Identificativo univoco del tavolo */
    int capacity;         /**< Numero massimo di posti (es. 2, 4, 6) */
    int occupied_seats;   /**< Numero di posti attualmente occupati */
} Table;

/**
 * @brief Area dedicata al consumo dei pasti (Refezione).
 */
typedef struct {
    int condition_semaphore_id;         /**< Semaforo di segnalazione per posti liberati */
    int active_tables_count;            /**< Numero di tavoli effettivamente inizializzati */
    Table tables[MAX_TABLES];           /**< Stato dinamico della topologia dei tavoli */
} DiningArea;

/**
 * @brief Stato dinamico di un gruppo di utenti durante la giornata.
 */
typedef struct {
    int active_members;                 /**< Numero di membri del gruppo ancora in mensa */
    pid_t group_leader_pid;             /**< PID del leader attuale (per coordinamento tavolo) */
    int assigned_table_id;              /**< ID del tavolo occupato dal gruppo (Social Seating) */
} GroupStatus;

/* ==========================================================================
 *                     SEZIONE: MEMORIA CONDIVISA PRINCIPALE
 * ========================================================================== */

/**
 * @brief Struttura principale della Memoria Condivisa.
 * 
 * Contiene lo stato globale della simulazione accessibile a tutti i processi.
 * Allocata dinamicamente per supportare il Flexible Array Member `group_statuses`.
 */
struct MainSharedMemory {
    SimulationConfiguration configuration;    /**< Parametri di configurazione caricati dai file .conf */
    SimulationStatistics statistics;          /**< Statistiche globali aggiornate in tempo reale */
    SimulationMenu food_menu;                 /**< Menu della mensa con i nomi dei piatti */
    
    int shared_memory_id;               /**< ID della risorsa Shared Memory stessa */
    int semaphore_sync_id;              /**< ID Set Semafori Barriera (SyncBarrierIndex) */
    int semaphore_mutex_id;             /**< ID Set Semafori Mutex (MutexSemaphoreIndex) */
    int group_sync_semaphore_id;        /**< ID Pool Semafori per sincronizzazione gruppi */
    int group_pool_size;                /**< Numero totale di slot nel pool di sincronizzazione */
    int semaphore_ticket_id;            /**< ID Semaforo per la validazione ticket all'ingresso */

    pid_t master_pid;                   /**< PID del processo Responsabile Mensa */
    pid_t process_group_pids[MAX_PROCESS_GROUPS]; /**< PGID dei vari gruppi di processi */

    FoodDistributionStation first_course_station;
    FoodDistributionStation second_course_station;
    FoodDistributionStation coffee_dessert_station;

    CashierStation register_station;
    DiningArea seat_area;

    int control_queue_id;               /**< ID Coda per richieste add_users */
    int current_total_users;           /**< Numero attuale di utenti nella simulazione */
    int add_users_flag;                /**< Flag per segnalare richieste di aggiunta utenti */

    int current_simulation_day;         /**< Giorno attuale della simulazione */
    int simulation_minutes_passed;      /**< Minuti simulati trascorsi dall'inizio del giorno */
    int is_simulation_running;          /**< Flag globale (1: Attiva, 0: Arresto Totale) */
    int current_simulation_status;      /**< Stato attuale (Aperto, In Chiusura, Disorder) */

    /** Registry per tracciamento PID -> Group (Proposta 2 Punto 2) */
    UserProcessMetadata user_registry[MAX_USERS_REGISTRY];

    /**
     * @brief Stato dinamico dei gruppi.
     * Flexible Array Member dedicato alla gestione elastica dei gruppi.
     * Deve risiedere alla fine della struct per permettere l'allocazione dinamica della SHM.
     */
    GroupStatus group_statuses[]; 
};

typedef struct MainSharedMemory MainSharedMemory;

/* ==========================================================================
 *                         SEZIONE: PROTOTIPI FUNZIONI
 * ========================================================================== */

/**
 * @brief Collega il processo alla memoria condivisa della simulazione.
 * 
 * @param shared_memory_id ID della risorsa SHM (solitamente passato via argv).
 * @return MainSharedMemory* Puntatore all'area di memoria, termina il processo su errore.
 */
MainSharedMemory* attach_to_simulation_shared_memory(int shared_memory_id);

/**
 * @brief Esegue la pulizia di tutte le risorse IPC allocate.
 * @param shared_memory_ptr Puntatore alla struttura di memoria condivisa.
 */
void cleanup_ipc_resources(MainSharedMemory *shared_memory_ptr);

/**
 * @brief Termina la simulazione in modo controllato, pulendo le risorse.
 * @param shared_memory_ptr Puntatore alla memoria condivisa.
 * @param exit_code Codice di uscita per il processo principale.
 */
void terminate_simulation_gracefully(MainSharedMemory *shared_memory_ptr, int exit_code);

#endif
