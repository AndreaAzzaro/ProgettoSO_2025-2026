/**
 * @file shm.c
 * @brief Implementazione wrapper per la memoria condivisa System V IPC.
 * 
 * Fornisce un'astrazione sopra shmget, shmat, shmdt e shmctl.
 * 
 * @see shm.h per la documentazione delle funzioni pubbliche.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/* Includes del progetto */
#include "shm.h"

/* ==========================================================================
 *                       SEZIONE: CREAZIONE E RIMOZIONE
 * ========================================================================== */

/** Crea o ottiene un segmento di memoria condivisa. */
int create_shared_memory_segment(key_t key, size_t segment_size, int segment_flags) {
    int shared_memory_id = shmget(key, segment_size, segment_flags);
    if (shared_memory_id == -1) {
        perror("IPC Error: shmget failed in create_shared_memory_segment");
    }
    return shared_memory_id;
}

/* ==========================================================================
 *                        SEZIONE: ATTACH E DETACH
 * ========================================================================== */

/** Collega il segmento allo spazio di indirizzamento del processo. */

void *attach_shared_memory_segment(int shared_memory_id, bool is_read_only) {
    int connection_flags = 0;
    if (is_read_only) {
        connection_flags = SHM_RDONLY;
    }

    void *attached_address = shmat(shared_memory_id, NULL, connection_flags);
    
    /* Controllo errore specifico per shmat che ritorna (void*)-1 */
    if (attached_address == (void *)-1) {
        perror("IPC Error: shmat failed in attach_shared_memory_segment");
        attached_address = NULL;
    }
    
    return attached_address;
}

/** Scollega il segmento dallo spazio di indirizzamento. */
int detach_shared_memory_segment(const void *shared_memory_address) {
    if (shmdt(shared_memory_address) == -1) {
        perror("IPC Error: shmdt failed in detach_shared_memory_segment");
        return -1;
    }
    return 0;
}

/** Rimuove definitivamente il segmento dal sistema. */
int remove_shared_memory_segment(int shared_memory_id) {
    if (shmctl(shared_memory_id, IPC_RMID, NULL) == -1) {
        perror("IPC Error: shmctl(IPC_RMID) failed in remove_shared_memory_segment");
        return -1;
    }
    return 0;
}