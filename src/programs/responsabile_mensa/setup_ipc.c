/**
 * @file setup_ipc.c
 * @brief Implementazione dell'inizializzazione delle risorse IPC del Master.
 * 
 * Questo modulo implementa le funzioni definite in setup_ipc.h per la creazione
 * e configurazione di memoria condivisa, set di semafori e code di messaggi.
 * 
 * @see setup_ipc.h
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

/* Includes del progetto */
#include "common.h"
#include "sem.h"
#include "shm.h"
#include "queue.h"
#include "utils.h"
#include "setup_ipc.h"

/* ==========================================================================
 *                       SEZIONE: PROTOTIPI PRIVATI
 * ========================================================================== */

/**
 * @brief Inizializza le risorse (MQ + SEM) per una singola stazione di distribuzione.
 * @param station Puntatore alla struttura stazione in SHM.
 */
static void init_station_resource(FoodDistributionStation *station);

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE PUBBLICA
 * ========================================================================== */

MainSharedMemory* initialize_simulation_shared_memory(int group_pool_size) {
    int shmid;
    MainSharedMemory *shm_ptr;
    
    /* Calcolo della dimensione totale: struct + pool dinamico (Flexible Array Member) */
    size_t shm_size = sizeof(MainSharedMemory) + (group_pool_size * sizeof(GroupStatus));

    key_t key = ftok(IPC_KEY_PATH, IPC_PROJECT_ID);
    if (key == -1) {
        perror("[ERROR] ftok fallita per la memoria condivisa");
        exit(EXIT_FAILURE);
    }

    /* Creazione del segmento */
    shmid = create_shared_memory_segment(key, shm_size, IPC_CREAT | IPC_EXCL | 0666);
    
    if (shmid == -1) {
        if (errno == EEXIST) {
            /* Cleanup di emergenza se esiste già un vecchio segmento */
            printf("[WARNING] Segmento SHM esistente. Tentativo di rimozione e ricreazione...\n");
            int old_shmid = shmget(key, 0, 0);
            if (old_shmid != -1) {
                shmctl(old_shmid, IPC_RMID, NULL);
            }
            shmid = create_shared_memory_segment(key, shm_size, IPC_CREAT | 0666);
        }
        
        if (shmid == -1) {
            perror("[ERROR] Creazione memoria condivisa fallita");
            exit(EXIT_FAILURE);
        }
    }

    /* Attach alla memoria del processo Master */
    shm_ptr = (MainSharedMemory *)attach_shared_memory_segment(shmid, false);
    if (shm_ptr == NULL) {
        perror("[ERROR] Attach memoria condivisa fallito");
        exit(EXIT_FAILURE);
    }

    /* Azzeramento e inizializzazione campi base */
    memset(shm_ptr, 0, shm_size);
    shm_ptr->shared_memory_id = shmid;
    shm_ptr->group_pool_size = group_pool_size;
    shm_ptr->is_simulation_running = 1;
    shm_ptr->master_pid = getpid();

    return shm_ptr;
}

void initialize_simulation_start_barriers(MainSharedMemory *shared_memory_ptr) {
    int semid = create_sem_set(IPC_PRIVATE, SYNC_BARRIER_SEM_COUNT, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("[ERROR] Creazione barriere di sincronizzazione fallita");
        exit(EXIT_FAILURE);
    }
    shared_memory_ptr->semaphore_sync_id = semid;
}

void initialize_daily_cycle_barriers(MainSharedMemory *shared_memory_ptr) {
    int semid = shared_memory_ptr->semaphore_sync_id;
    
    /* Nota: Il valore effettivo dei semafori Ready/Gate viene impostato 
       dal simulation_engine all'inizio di ogni giornata. Qui azzeriamo solo. */
    for (int i = 0; i < SYNC_BARRIER_SEM_COUNT; i++) {
        init_sem_val(semid, i, 0);
    }
}

void initialize_global_simulation_mutexes(MainSharedMemory *shared_memory_ptr) {
    int semid = create_sem_set(IPC_PRIVATE, MUTEX_SEMAPHORE_COUNT, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("[ERROR] Creazione mutex globali fallita");
        exit(EXIT_FAILURE);
    }
    
    /* Mutex binari (inizializzati a 1) */
    init_sem_val(semid, MUTEX_SIMULATION_STATS, 1);
    init_sem_val(semid, MUTEX_SHARED_DATA, 1);
    
    /* Semaforo di controllo per add_users (inizializzato a 0, sbloccato dal Master) */
    init_sem_val(semid, MUTEX_ADD_USERS_PERMISSION, 0); 

    /* Mutex per la scansione dei tavoli (inizializzato a 1) */
    init_sem_val(semid, MUTEX_TABLES, 1);
    
    shared_memory_ptr->semaphore_mutex_id = semid;
}

void initialize_distribution_stations(MainSharedMemory *shared_memory_ptr) {
    init_station_resource(&shared_memory_ptr->first_course_station);
    init_station_resource(&shared_memory_ptr->second_course_station);
    init_station_resource(&shared_memory_ptr->coffee_dessert_station);
}

void initialize_dining_area_seats_semaphores(MainSharedMemory *shared_memory_ptr) {
    int semid = create_sem_set(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("[ERROR] Creazione semaforo posti a sedere fallita");
        exit(EXIT_FAILURE);
    }
    
    /* Il valore reale verrà impostato dal Master dopo il caricamento della configurazione */
    init_sem_val(semid, 0, 0);
    shared_memory_ptr->seat_area.condition_semaphore_id = semid;
}

void initialize_ticket_validation_semaphores(MainSharedMemory *shared_memory_ptr) {
    int semid = create_sem_set(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("[ERROR] Creazione semaforo ticket fallita");
        exit(EXIT_FAILURE);
    }
    
    init_sem_val(semid, 0, TICKET_VALIDATORS_COUNT);
    shared_memory_ptr->semaphore_ticket_id = semid;
}

void initialize_cashier_checkout_message_queue(MainSharedMemory *shared_memory_ptr) {
    /* Set semafori specifico per la cassa (include gate di blocco disorder) */
    int semid = create_sem_set(IPC_PRIVATE, STATION_SEM_COUNT, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("[ERROR] Creazione set semafori cassa fallita");
        exit(EXIT_FAILURE);
    }

    init_sem_val(semid, STATION_SEM_AVAILABLE_POSTS, 0); /* Gestito dinamicamente */
    init_sem_val(semid, STATION_SEM_USER_QUEUE, 0);
    init_sem_val(semid, STATION_SEM_REFILL_GATE, 0);
    init_sem_val(semid, STATION_SEM_REFILL_ACK, 0);
    init_sem_val(semid, STATION_SEM_STOP_GATE, 0); /* 0 significa cassa operativa */

    shared_memory_ptr->register_station.semaphore_set_id = semid;

    /* Coda messaggi per la cassa */
    int msqid = create_message_queue(IPC_PRIVATE, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("[ERROR] Creazione coda messaggi cassa fallita");
        exit(EXIT_FAILURE);
    }

    /* Ottimizzazione buffer coda per evitare blocchi su broadcast massivi */
    struct msqid_ds ds;
    if (msgctl(msqid, IPC_STAT, &ds) != -1) {
        ds.msg_qbytes = 65536; /* Espansione a 64KB */
        msgctl(msqid, IPC_SET, &ds);
    }
    shared_memory_ptr->register_station.message_queue_id = msqid;
}

void initialize_control_structures(MainSharedMemory *shm_ptr) {
    shm_ptr->current_total_users = shm_ptr->configuration.quantities.number_of_initial_users;
    shm_ptr->add_users_flag = 0;
    
    /* Coda di comunicazione Master <-> add_users.c */
    int msqid = create_message_queue(IPC_PRIVATE, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("[ERROR] Creazione coda di controllo fallita");
        exit(EXIT_FAILURE);
    }

    /* Ottimizzazione */
    struct msqid_ds ds;
    if (msgctl(msqid, IPC_STAT, &ds) != -1) {
        ds.msg_qbytes = 16384; 
        msgctl(msqid, IPC_SET, &ds);
    }
    shm_ptr->control_queue_id = msqid;
}

void initialize_group_sync_pool(MainSharedMemory *shm_ptr, int pool_size) {
    int total_sems = pool_size * GROUP_SEMS_PER_ENTRY;
    int semid = create_sem_set(IPC_PRIVATE, total_sems, IPC_CREAT | 0666);
    
    if (semid == -1) {
        perror("[ERROR] Creazione pool semafori gruppi fallita");
        exit(EXIT_FAILURE);
    }

    /* Inizializzazione di massa del pool */
    for (int i = 0; i < pool_size; i++) {
        int base = i * GROUP_SEMS_PER_ENTRY;
        init_sem_val(semid, base + GROUP_SEM_PRE_CASHIER, 0);  /* Bloccato finché tutti pronti */
        init_sem_val(semid, base + GROUP_SEM_TABLE_GATE, 1);   /* Chiuso (1) finché leader non prenota */
        init_sem_val(semid, base + GROUP_SEM_EXIT, 0);         /* Bloccato finché tutti finito */
    }

    shm_ptr->group_sync_semaphore_id = semid;
    shm_ptr->group_pool_size = pool_size;
}

void initialize_ipc_sources(MainSharedMemory *shm_ptr) {
    initialize_simulation_start_barriers(shm_ptr);
    initialize_daily_cycle_barriers(shm_ptr);
    initialize_global_simulation_mutexes(shm_ptr);
    initialize_distribution_stations(shm_ptr);
    initialize_dining_area_seats_semaphores(shm_ptr);
    initialize_ticket_validation_semaphores(shm_ptr);
    initialize_cashier_checkout_message_queue(shm_ptr);
    initialize_control_structures(shm_ptr);
}

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE PRIVATA
 * ========================================================================== */

static void init_station_resource(FoodDistributionStation *station) {
    /* 1. Coda Messaggi */
    station->message_queue_id = create_message_queue(IPC_PRIVATE, IPC_CREAT | 0666);
    if (station->message_queue_id == -1) {
        perror("[ERROR] Creazione coda messaggi stazione fallita");
        exit(EXIT_FAILURE);
    }

    /* Ottimizzazione throughput */
    struct msqid_ds ds;
    if (msgctl(station->message_queue_id, IPC_STAT, &ds) != -1) {
        ds.msg_qbytes = 65536; 
        msgctl(station->message_queue_id, IPC_SET, &ds);
    }

    /* 2. Set Semafori Stazione */
    station->semaphore_set_id = create_sem_set(IPC_PRIVATE, STATION_SEM_COUNT, IPC_CREAT | 0666);
    if (station->semaphore_set_id == -1) {
        perror("[ERROR] Creazione set semafori stazione fallita");
        exit(EXIT_FAILURE);
    }

    /* Valori iniziali */
    init_sem_val(station->semaphore_set_id, STATION_SEM_AVAILABLE_POSTS, 0); 
    init_sem_val(station->semaphore_set_id, STATION_SEM_USER_QUEUE, 0);
    init_sem_val(station->semaphore_set_id, STATION_SEM_REFILL_GATE, 0); 
    init_sem_val(station->semaphore_set_id, STATION_SEM_REFILL_ACK, 0);
}
