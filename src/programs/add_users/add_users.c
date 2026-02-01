/**
 * @file add_users.c
 * @brief Utility esterna per aggiungere dinamicamente utenti alla simulazione.
 * 
 * @see add_users.h per i prototipi delle funzioni.
 * @see add_users_analysis.md per il design del protocollo.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/* Includes del progetto */
#include "common.h"
#include "shm.h"
#include "sem.h"
#include "queue.h"
#include "utils.h"
#include "add_users.h"

/* ==========================================================================
 *                             SEZIONE: MAIN
 * ========================================================================== */

int main(int argc, char *argv[]) {
    /* Validazione argomenti */
    if (argc > 3) {
        fprintf(stderr, "Uso: %s [numero_utenti]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* 1. Connessione alla simulazione */
    MainSharedMemory *shm;
    int shmid;
    if (connect_to_simulation(&shm, &shmid) != 0) {
        return EXIT_FAILURE;
    }

    /* 2. Parsing numero utenti */
    int users_to_add = parse_users_count(argc, argv, shm);
    if (users_to_add < 0) {
        return EXIT_FAILURE;
    }

    /* 3. Invio richiesta al Master */
    if (send_add_users_request(shm, users_to_add) != 0) {
        return EXIT_FAILURE;
    }

    /* 4. Attesa autorizzazione */
    if (wait_for_master_permission(shm) != 0) {
        return EXIT_FAILURE;
    }

    /* 5. Spawn dei gruppi utenti */
    int spawned = spawn_user_groups(shm, shmid, users_to_add);

    printf("[ADD_USERS] Completato. %d utenti aggiunti.\n", spawned);
    return EXIT_SUCCESS;
}

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE FUNZIONI
 * ========================================================================== */

int find_free_group_index(MainSharedMemory *shm) {
    for (int i = 0; i < shm->group_pool_size; i++) {
        if (shm->group_statuses[i].active_members == 0) {
            return i;
        }
    }
    return -1;
}

int connect_to_simulation(MainSharedMemory **shm_out, int *shmid_out) {
    key_t key = ftok(IPC_KEY_PATH, IPC_PROJECT_ID);
    if (key == -1) {
        perror("[ERROR] ftok fallita. Assicurati che config/config.conf esista");
        return -1;
    }

    int shmid = shmget(key, 0, 0);
    if (shmid == -1) {
        fprintf(stderr, "[ERROR] Impossibile trovare la memoria condivisa.\n");
        fprintf(stderr, "La simulazione è stata avviata?\n");
        return -1;
    }

    *shm_out = attach_to_simulation_shared_memory(shmid);
    *shmid_out = shmid;
    srand(time(NULL) ^ getpid());
    
    return 0;
}

int parse_users_count(int argc, char *argv[], MainSharedMemory *shm) {
    int users_to_add;
    
    if (argc >= 2) {
        users_to_add = atoi(argv[1]);
    } else {
        users_to_add = shm->configuration.quantities.number_of_new_users_batch;
        printf("[ADD_USERS] Nessun valore specificato. Uso default: %d\n", users_to_add);
    }

    if (users_to_add <= 0) {
        fprintf(stderr, "[ERROR] Numero utenti non valido (%d).\n", users_to_add);
        return -1;
    }
    
    return users_to_add;
}

int send_add_users_request(MainSharedMemory *shm, int users_count) {
    SimulationMessage msg;
    msg.message_type = MSG_TYPE_CONTROL;
    ControlPayload *payload = (ControlPayload *)msg.message_text;
    payload->users_count = users_count;

    if (send_message_to_queue(shm->control_queue_id, &msg, sizeof(ControlPayload), 0) == -1) {
        fprintf(stderr, "[ERROR] Invio richiesta alla coda di controllo fallito.\n");
        return -1;
    }

    shm->add_users_flag = 1;
    if (kill(shm->master_pid, SIGUSR1) == -1) {
        perror("[ERROR] Segnale SIGUSR1 al Master fallito");
        return -1;
    }

    printf("[ADD_USERS] Richiesti %d utenti. Attesa fine giornata...\n", users_count);
    return 0;
}

int wait_for_master_permission(MainSharedMemory *shm) {
    printf("[ADD_USERS] In attesa del permesso dal Master...\n");
    
    if (reserve_sem(shm->semaphore_mutex_id, MUTEX_ADD_USERS_PERMISSION) == -1) {
        perror("[ERROR] Attesa permesso fallita");
        return -1;
    }

    printf("[ADD_USERS] Autorizzazione ricevuta. Avvio spawn...\n");
    return 0;
}

void register_user_in_registry(MainSharedMemory *shm, pid_t pid, int group_index) {
    reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    
    bool registered = false;
    for (int r = 0; r < MAX_USERS_REGISTRY; r++) {
        if (shm->user_registry[r].pid == 0) {
            shm->user_registry[r].pid = pid;
            shm->user_registry[r].group_index = group_index;
            registered = true;
            break;
        }
    }
    
    release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
    
    if (!registered) {
        fprintf(stderr, "[WARNING] Registro pieno. PID %d non tracciato.\n", pid);
    }
}

void spawn_single_user(MainSharedMemory *shm, int shmid, int group_size, 
                       int sync_index, int member_index) {
    pid_t pid = fork();
    
    if (pid == 0) {
        setpgid(0, shm->process_group_pids[GROUP_USERS]);

        char shm_str[24], gsize_str[24], gindex_str[24], is_leader_str[8];
        sprintf(shm_str, "%d", shmid);
        sprintf(gsize_str, "%d", group_size);
        sprintf(gindex_str, "%d", sync_index);
        sprintf(is_leader_str, "%d", (member_index == 0));

        execl("./bin/utente", "utente", shm_str, gsize_str, gindex_str, is_leader_str, (char *)NULL);
        perror("[ERROR] execl fallita");
        exit(EXIT_FAILURE);
        
    } else if (pid > 0) {
        register_user_in_registry(shm, pid, sync_index);
    } else {
        perror("[ERROR] fork fallita");
    }
}

int spawn_user_groups(MainSharedMemory *shm, int shmid, int total_users) {
    int users_spawned = 0;

    while (users_spawned < total_users) {
        int group_size = (rand() % MAX_USERS_PER_GROUP) + 1;
        if (users_spawned + group_size > total_users) {
            group_size = total_users - users_spawned;
        }

        reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
        int sync_index = find_free_group_index(shm);
        
        if (sync_index == -1) {
            fprintf(stderr, "[ERROR] Pool gruppi saturo.\n");
            release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
            break;
        }
        
        shm->group_statuses[sync_index].active_members = group_size;
        shm->group_statuses[sync_index].group_leader_pid = 0;
        release_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);

        for (int i = 0; i < group_size; i++) {
            spawn_single_user(shm, shmid, group_size, sync_index, i);
        }
        
        users_spawned += group_size;
    }

    return users_spawned;
}
