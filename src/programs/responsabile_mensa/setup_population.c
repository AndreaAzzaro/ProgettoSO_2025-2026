/**
 * @file setup_population.c
 * @brief Implementazione della creazione dei processi figli e gestione popolazione.
 * 
 * Gestisce l'orchestrazione dei fork() per creare operatori, cassieri e utenti,
 * assicurando la corretta assegnazione ai gruppi di processi (PGID) e la
 * registrazione nel registro globale per la gestione dei deadlock.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

/* Includes del progetto */
#include "common.h"
#include "setup_population.h"
#include "utils.h"
#include "sem.h"

/* ==========================================================================
 *                        VARIABILI GLOBALI (PRIVATE)
 * ========================================================================== */

/** Conteggio dei gruppi pianificati per lo spawn iniziale. */
static int planned_groups_count = 0;

/** Array dinamico contenente le dimensioni di ciascun gruppo pianificato. */
static int *planned_group_sizes = NULL;

/* ==========================================================================
 *                       SEZIONE: PROTOTIPI PRIVATI
 * ========================================================================== */

/**
 * @brief Esegue l'execl per un processo operatore di stazione.
 * @param shmid ID della SHM.
 * @param station_type Tipo di stazione (0, 1, 2).
 */
static void exec_worker(int shmid, int station_type);

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE PUBBLICA
 * ========================================================================== */

void calculate_worker_distribution(int total_workers, int *average_times_array, int *distribution_results_array, int stations_count) {
    int total_time = 0;
    for (int i = 0; i < stations_count; i++) {
        total_time += average_times_array[i];
    }

    if (total_time == 0) {
        /* Caso fallback: distribuzione perfettamente uniforme */
        for (int i = 0; i < stations_count; i++) {
            distribution_results_array[i] = total_workers / stations_count;
        }
    } else {
        int distributed = 0;
        for (int i = 0; i < stations_count; i++) {
            /* Assegnazione proporzionale al tempo medio (più tempo = più bisogno di staff) */
            distribution_results_array[i] = (int)((double)total_workers * average_times_array[i] / total_time);
            distributed += distribution_results_array[i];
        }
        /* Gestione del resto (assegnato alla prima stazione per chiudere il totale) */
        distribution_results_array[0] += (total_workers - distributed);
    }
}

void setup_worker_distribution(MainSharedMemory *shm_ptr) {
    int total_workers = shm_ptr->configuration.quantities.number_of_workers;
    int stations_count = 3; 
    int average_times[3];
    int worker_results[3];

    /* Recupero i tempi medi dalle configurazioni specifiche */
    average_times[0] = shm_ptr->configuration.timings.average_service_time_primi;
    average_times[1] = shm_ptr->configuration.timings.average_service_time_secondi;
    average_times[2] = shm_ptr->configuration.timings.average_service_time_coffee;

    calculate_worker_distribution(total_workers, average_times, worker_results, stations_count);

    shm_ptr->first_course_station.num_operators_assigned = worker_results[0];
    shm_ptr->second_course_station.num_operators_assigned = worker_results[1];
    shm_ptr->coffee_dessert_station.num_operators_assigned = worker_results[2];
}

void launch_simulation_operators(MainSharedMemory *shared_memory_ptr) {
    int shmid = shared_memory_ptr->shared_memory_id;
    
    /* Mapping stazioni e gruppi di processi */
    int station_operators[] = {
        shared_memory_ptr->first_course_station.num_operators_assigned,
        shared_memory_ptr->second_course_station.num_operators_assigned,
        shared_memory_ptr->coffee_dessert_station.num_operators_assigned
    };
    int groups[] = { GROUP_FIRST_COURSES, GROUP_SECOND_COURSES, GROUP_DESSERT_COFFEE };
    
    /* 1. Lancio Operatori di Stazione */
    for (int s = 0; s < 3; s++) {
        pid_t pgid = 0;
        for (int i = 0; i < station_operators[s]; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                setpgid(0, pgid); /* Assegna al PGID della stazione */
                exec_worker(shmid, s);
            } else if (pid > 0) {
                if (i == 0) pgid = pid; /* Il primo figlio definisce il PGID del gruppo */
            }
        }
        shared_memory_ptr->process_group_pids[groups[s]] = pgid;
    }

    /* 2. Lancio Operatori di Cassa (Cassieri) */
    pid_t cassa_pgid = 0;
    int num_cashiers = shared_memory_ptr->configuration.seats.seats_cash_desk;
    
    for (int i = 0; i < num_cashiers; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            setpgid(0, cassa_pgid);
            char shm_str[20];
            sprintf(shm_str, "%d", shmid);
            execl("./bin/operatore_cassa", "operatore_cassa", shm_str, (char *)NULL);
            perror("[ERROR] execl operatore_cassa fallita");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            if (i == 0) cassa_pgid = pid;
        }
    }
    shared_memory_ptr->process_group_pids[GROUP_CASHIERS] = cassa_pgid;
}

int calculate_initial_groups_count(MainSharedMemory *shm_ptr) {
    int users_to_assign = shm_ptr->configuration.quantities.number_of_initial_users;
    
    /* Allocazione buffer temporaneo per le dimensioni dei gruppi */
    planned_group_sizes = (int *)malloc(users_to_assign * sizeof(int));
    if (planned_group_sizes == NULL) {
        perror("[ERROR] Allocazione dinamica gruppi fallita");
        exit(EXIT_FAILURE);
    }

    planned_groups_count = 0;
    while (users_to_assign > 0) {
        int g_size = generate_random_integer(1, MAX_USERS_PER_GROUP);
        if (g_size > users_to_assign) g_size = users_to_assign;
        
        planned_group_sizes[planned_groups_count] = g_size;
        planned_groups_count++;
        users_to_assign -= g_size;
    }
    
    /* Margine extra di 100 slot per nuovi gruppi creati tramite add_users utility */
    return planned_groups_count + 100;
}

void launch_simulation_users(MainSharedMemory *shared_memory_ptr) {
    int shmid = shared_memory_ptr->shared_memory_id;
    int current_sync_index = 0;

    printf("[MASTER] Lancio popolazione utenti (%d gruppi)...\n", planned_groups_count);

    for (int g = 0; g < planned_groups_count; g++) {
        int group_size = planned_group_sizes[g];

        /* Setup stato del gruppo in SHM prima della creazione dei processi */
        shared_memory_ptr->group_statuses[current_sync_index].active_members = group_size;
        shared_memory_ptr->group_statuses[current_sync_index].group_leader_pid = 0;

        for (int i = 0; i < group_size; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                /* Aggancio al PGID globale degli utenti per segnali broadcast */
                pid_t users_global_pgid = shared_memory_ptr->process_group_pids[GROUP_USERS];
                setpgid(0, users_global_pgid);

                char shm_str[24], gsize_str[24], gindex_str[24], is_leader_str[8];
                sprintf(shm_str, "%d", shmid);
                sprintf(gsize_str, "%d", group_size);
                sprintf(gindex_str, "%d", current_sync_index);
                sprintf(is_leader_str, "%d", (i == 0)); /* Il primo del gruppo è il leader */

                execl("./bin/utente", "utente", shm_str, gsize_str, gindex_str, is_leader_str, (char *)NULL);
                perror("[ERROR] execl utente fallita");
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                /* Registrazione nel registro di sistema per gestione zombie e deadlock */
                reserve_sem(shared_memory_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);
                for (int r = 0; r < MAX_USERS_REGISTRY; r++) {
                    if (shared_memory_ptr->user_registry[r].pid == 0) {
                        shared_memory_ptr->user_registry[r].pid = pid;
                        shared_memory_ptr->user_registry[r].group_index = current_sync_index;
                        break;
                    }
                }
                release_sem(shared_memory_ptr->semaphore_mutex_id, MUTEX_SHARED_DATA);

                /* Definizione PGID globale basato sul primissimo utente creato */
                if (shared_memory_ptr->process_group_pids[GROUP_USERS] == 0) {
                    shared_memory_ptr->process_group_pids[GROUP_USERS] = pid;
                }
            }
        }
        current_sync_index++;
    }

    /* Cleanup memoria temporanea pianificazione */
    if (planned_group_sizes != NULL) {
        free(planned_group_sizes);
        planned_group_sizes = NULL;
    }
}

/**
 * @brief Genera la topologia dinamica dei tavoli nell'area di refezione.
 * Distribuisce posti tra tavoli da 2, 4 e 6 fino a NOFTABLESEATS.
 */
static void initialize_table_topology(MainSharedMemory *shm_ptr) {
    int total_seats_to_assign = shm_ptr->configuration.seats.total_dining_seats;
    int table_idx = 0;

    while (total_seats_to_assign > 0 && table_idx < MAX_TABLES) {
        int capacity;
        int rnd = generate_random_integer(1, 100);

        if (rnd <= 30)      capacity = 2; // 30% piccoli
        else if (rnd <= 80) capacity = 4; // 50% medi
        else                capacity = 6; // 20% grandi

        if (capacity > total_seats_to_assign) capacity = total_seats_to_assign;

        shm_ptr->seat_area.tables[table_idx].id = table_idx;
        shm_ptr->seat_area.tables[table_idx].capacity = capacity;
        shm_ptr->seat_area.tables[table_idx].occupied_seats = 0;

        total_seats_to_assign -= capacity;
        table_idx++;
    }
    shm_ptr->seat_area.active_tables_count = table_idx;
    printf("[MASTER] Topologia tavoli: %d tavoli pronti (Capacità Tot: %d).\n", 
           table_idx, shm_ptr->configuration.seats.total_dining_seats);
}

void initialize_station_operator_semaphores(MainSharedMemory *shm_ptr) {
    /* Set semaforo posti disponibili basato sulla distribuzione calcolata */
    init_sem_val(shm_ptr->first_course_station.semaphore_set_id, 
                 STATION_SEM_AVAILABLE_POSTS, 
                 shm_ptr->first_course_station.num_operators_assigned);
    
    init_sem_val(shm_ptr->second_course_station.semaphore_set_id, 
                 STATION_SEM_AVAILABLE_POSTS, 
                 shm_ptr->second_course_station.num_operators_assigned);
    
    init_sem_val(shm_ptr->coffee_dessert_station.semaphore_set_id, 
                 STATION_SEM_AVAILABLE_POSTS, 
                 shm_ptr->coffee_dessert_station.num_operators_assigned);
    
    /* Cassieri: valore secco da configurazione */
    init_sem_val(shm_ptr->register_station.semaphore_set_id, 
                 STATION_SEM_AVAILABLE_POSTS, 
                 shm_ptr->configuration.seats.seats_cash_desk);
 
    /* Semaforo di condizione (segnalazione) per i tavoli della mensa */
    init_sem_val(shm_ptr->seat_area.condition_semaphore_id, 0, 0);

    /* Inizializza l'array dei tavoli (Step 2 Social Seating) */
    initialize_table_topology(shm_ptr);
}

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE PRIVATA
 * ========================================================================== */

static void exec_worker(int shmid, int station_type) {
    char shm_str[24];
    char type_str[10];
    sprintf(shm_str, "%d", shmid);
    sprintf(type_str, "%d", station_type);
    
    execl("./bin/operatore", "operatore", shm_str, type_str, (char *)NULL);
    perror("[ERROR] execl operatore fallita");
    exit(EXIT_FAILURE);
}
