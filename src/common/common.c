/**
 * @file common.c
 * @brief Implementazione delle funzioni comuni per gestione SHM e cleanup IPC.
 * 
 * @see common.h per la documentazione delle funzioni pubbliche.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

/* Includes del progetto */
#include "common.h"
#include "shm.h"
#include "sem.h"
#include "queue.h"

/* ==========================================================================
 *                     SEZIONE: GESTIONE MEMORIA CONDIVISA
 * ========================================================================== */

MainSharedMemory* attach_to_simulation_shared_memory(int shared_memory_id) {
    MainSharedMemory *shm_ptr = (MainSharedMemory *)attach_shared_memory_segment(shared_memory_id, false);
    if (shm_ptr == NULL) {
        perror("[ERROR] Impossibile collegarsi alla memoria condivisa");
        exit(EXIT_FAILURE);
    }
    return shm_ptr;
}

/* ==========================================================================
 *                       SEZIONE: CLEANUP E TERMINAZIONE
 * ========================================================================== */

/**
 * Rimuove tutte le risorse IPC allocate dalla simulazione.
 * Ordine: Code -> Semafori Stazione -> Semafori Globali -> SHM
 */
void cleanup_ipc_resources(MainSharedMemory *shared_memory_ptr) {
    if (!shared_memory_ptr) return;

    /* 1. Code messaggi stazioni */
    remove_message_queue(shared_memory_ptr->first_course_station.message_queue_id);
    remove_message_queue(shared_memory_ptr->second_course_station.message_queue_id);
    remove_message_queue(shared_memory_ptr->coffee_dessert_station.message_queue_id);
    remove_message_queue(shared_memory_ptr->register_station.message_queue_id);

    /* 2. Set semafori stazioni */
    delete_sem_set(shared_memory_ptr->first_course_station.semaphore_set_id);
    delete_sem_set(shared_memory_ptr->second_course_station.semaphore_set_id);
    delete_sem_set(shared_memory_ptr->coffee_dessert_station.semaphore_set_id);

    /* 3. Risorse globali (barriere, mutex, ticket, posti) */
    delete_sem_set(shared_memory_ptr->semaphore_sync_id);
    delete_sem_set(shared_memory_ptr->semaphore_mutex_id);
    delete_sem_set(shared_memory_ptr->semaphore_ticket_id);
    delete_sem_set(shared_memory_ptr->seat_area.condition_semaphore_id);

    /* 4. Memoria condivisa (detach prima, remove dopo) */
    int shmid = shared_memory_ptr->shared_memory_id;
    detach_shared_memory_segment(shared_memory_ptr);
    remove_shared_memory_segment(shmid);
}

/**
 * Termina la simulazione in modo controllato.
 * Attende tutti i figli prima di rimuovere le risorse IPC.
 */
void terminate_simulation_gracefully(MainSharedMemory *shared_memory_ptr, int exit_code) {
    printf("\n[SYSTEM] Terminazione simulazione in corso...\n");

    /* Attende che tutti i processi figli terminino prima di rimuovere le IPC */
    while (wait(NULL) > 0);

    cleanup_ipc_resources(shared_memory_ptr);
    exit(exit_code);
}
