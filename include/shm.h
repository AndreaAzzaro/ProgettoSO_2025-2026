#ifndef SHM_H
#define SHM_H

#include <sys/types.h> // per size_t

/**
 * @brief Nome univoco del segmento di memoria condivisa.
 * Definirlo qui lega la libreria a questo progetto specifico.
 */
#define SHM_NAME "/mensa_shm_project"

/**
 * @brief Crea o apre un segmento di memoria condivisa POSIX.
 * * Questa funzione incapsula le chiamate a shm_open, ftruncate e mmap.
 * Se il segmento non esiste, lo crea. Se esiste, lo apre.
 * * @param name Il nome univoco della shared memory (deve iniziare con '/').
 * @param size La dimensione in byte da allocare (solitamente sizeof(SharedMensa)).
 * * @return void* Puntatore all'inizio della memoria condivisa mappata.
 * * @pre Il parametro 'name' non deve essere NULL.
 * @post Il puntatore ritornato è valido e pronto per la lettura/scrittura.
 * @note In caso di errore critico (es. permessi o memoria piena), 
 * la funzione stampa l'errore su stderr e termina il processo (exit).
 */
void* alloc_shared_memory(const char *name, size_t size);

/**
 * @brief Rilascia le risorse della memoria condivisa.
 * * Esegue l'operazione di detach (munmap) e, opzionalmente, 
 * la rimozione fisica del file (shm_unlink).
 * * @param ptr    Puntatore all'area di memoria da liberare (ottenuto da alloc).
 * @param size   La dimensione del segmento da rilasciare.
 * @param name   Il nome del segmento (necessario solo se unlink == 1).
 * @param unlink Flag booleana (0 o 1):
 * - 0: Esegue solo il detach (usato da Operatori/Utenti).
 * - 1: Esegue detach E distruzione del file (usato SOLO dal Responsabile).
 * * @pre 'ptr' deve essere un puntatore valido restituito da mmap.
 */
void free_shared_memory(void *ptr, size_t size, const char *name, int unlink);

#endif