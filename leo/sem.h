/**
 * @file sem.h
 * @brief Wrapper per la gestione semplificata dei Semafori POSIX.
 * * Fornisce funzioni per inizializzare, attendere e segnalare semafori
 * gestendo automaticamente i controlli di errore.
 */

#ifndef SEM_H
#define SEM_H

#include <semaphore.h>
#include <sys/types.h>

/**
 * @brief Inizializza un semaforo POSIX in modalità condivisa tra processi.
 * * Imposta il flag 'pshared' a 1, rendendo il semaforo utilizzabile
 * da processi diversi che accedono alla stessa Shared Memory.
 * * @param sem   Puntatore all'area di memoria (in SHM) dove risiede il semaforo.
 * @param value Valore iniziale del contatore (es. 0 per rosso, N per verde/posti).
 * * @note Termina il processo (exit) in caso di errore di inizializzazione.
 */
void init_sem(sem_t *sem, int value);

/**
 * @brief Esegue l'operazione di WAIT (P) - Decremento/Blocco.
 * * Se il semaforo è > 0, lo decrementa e prosegue.
 * Se il semaforo è 0, il processo si blocca in attesa.
 * * @param sem Puntatore al semaforo.
 */
void wait_sem(sem_t *sem);

/**
 * @brief Esegue l'operazione di POST (V) - Incremento/Sblocco.
 * * Incrementa il valore del semaforo e sveglia un eventuale processo
 * in attesa sulla wait_sem.
 * * @param sem Puntatore al semaforo.
 */
void post_sem(sem_t *sem);

/**
 * @brief Distrugge il semaforo rilasciando le risorse del kernel.
 * * Da chiamare solo al termine della simulazione (cleanup).
 * * @param sem Puntatore al semaforo da distruggere.
 */
void destroy_sem(sem_t *sem);

#endif