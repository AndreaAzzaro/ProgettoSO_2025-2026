/**
 * @file shm.h
 * @brief Wrapper per la gestione della Memoria Condivisa (System V IPC).
 * 
 * Questo modulo fornisce un'astrazione sopra le primitive shmget, shmat, shmdt
 * e shmctl per la gestione della memoria condivisa tra processi.
 * 
 * @see common.h per la struttura MainSharedMemory.
 */

#ifndef SHM_H
#define SHM_H

#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>

/* ==========================================================================
 *                         SEZIONE: FUNZIONI PUBBLICHE
 * ========================================================================== */

/**
 * @brief Crea o ottiene un identificatore per un segmento di memoria condivisa.
 * 
 * @param key Chiave IPC (generata tramite ftok o IPC_PRIVATE).
 * @param segment_size Dimensione totale del segmento in byte.
 * @param segment_flags Flag di creazione e permessi (es. IPC_CREAT | 0666).
 * @return int ID della memoria condivisa (shmid), o -1 in caso di errore.
 */
int create_shared_memory_segment(key_t key, size_t segment_size, int segment_flags);

/**
 * @brief Collega (attach) il segmento di memoria condivisa allo spazio di indirizzamento del processo.
 * 
 * @param shared_memory_id ID della memoria condivisa.
 * @param is_read_only Se true, collega il segmento in sola lettura.
 * @return void* Puntatore all'area di memoria collegata, NULL in caso di errore.
 */
void *attach_shared_memory_segment(int shared_memory_id, bool is_read_only);

/**
 * @brief Scollega (detach) il segmento di memoria condivisa dallo spazio di indirizzamento.
 * 
 * @param shared_memory_address Indirizzo di memoria restituito precedentemente da attach.
 * @return int 0 in caso di successo, -1 in caso di errore.
 */
int detach_shared_memory_segment(const void *shared_memory_address);

/**
 * @brief Rimuove definitivamente il segmento di memoria condivisa dal sistema operativo.
 * 
 * @param shared_memory_id ID della memoria condivisa da eliminare.
 * @return int 0 in caso di successo, -1 in caso di errore.
 */
int remove_shared_memory_segment(int shared_memory_id);

#endif /* SHM_H */
