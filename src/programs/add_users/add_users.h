/**
 * @file add_users.h
 * @brief Header per l'utility add_users.
 * 
 * Protocollo di handshake:
 * 1. Invia richiesta alla coda di controllo
 * 2. Segnala SIGUSR1 al Master
 * 3. Attende autorizzazione via semaforo
 * 4. Spawna i nuovi processi utente
 * 
 * @see add_users.c per l'implementazione.
 */

#ifndef ADD_USERS_H
#define ADD_USERS_H

#include "common.h"
#include <sys/types.h>

/* ==========================================================================
 *                       SEZIONE: PROTOTIPI FUNZIONI
 * ========================================================================== */

/**
 * @brief Cerca uno slot libero nel pool di sincronizzazione gruppi.
 * @return Indice del gruppo libero o -1 se pieno.
 */
int find_free_group_index(MainSharedMemory *shm);

/**
 * @brief Connette alla memoria condivisa della simulazione tramite ftok.
 * @return 0 successo, -1 errore.
 */
int connect_to_simulation(MainSharedMemory **shm_out, int *shmid_out);

/**
 * @brief Parsing del numero di utenti da aggiungere.
 * @return Numero utenti (>0) o -1 se errore.
 */
int parse_users_count(int argc, char *argv[], MainSharedMemory *shm);

/**
 * @brief Invia la richiesta al Master e notifica via segnale.
 * @return 0 successo, -1 errore.
 */
int send_add_users_request(MainSharedMemory *shm, int users_count);

/**
 * @brief Attende l'autorizzazione dal Master (semaforo).
 * @return 0 successo, -1 errore.
 */
int wait_for_master_permission(MainSharedMemory *shm);

/**
 * @brief Registra un nuovo utente nel registry per tracciamento SIGCHLD.
 */
void register_user_in_registry(MainSharedMemory *shm, pid_t pid, int group_index);

/**
 * @brief Spawna un singolo utente del gruppo.
 */
void spawn_single_user(MainSharedMemory *shm, int shmid, int group_size, 
                       int sync_index, int member_index);

/**
 * @brief Spawna tutti i gruppi di utenti richiesti.
 * @return Numero di utenti effettivamente spawnati.
 */
int spawn_user_groups(MainSharedMemory *shm, int shmid, int total_users);

#endif /* ADD_USERS_H */
