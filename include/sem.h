/**
 * @file sem.h
 * @brief Wrapper per la gestione dei semafori System V IPC.
 * 
 * Questo modulo fornisce un'astrazione sopra le primitive semget, semop e semctl
 * per facilitare la sincronizzazione tra i processi della simulazione.
 * 
 * Include funzioni per:
 * - Creazione e distruzione di set di semafori
 * - Operazioni P (reserve) e V (release)
 * - Barriere di sincronizzazione (pattern ping-pong)
 * - Lettura del valore corrente
 */

#ifndef SEM_H
#define SEM_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

/* ==========================================================================
 *                        SEZIONE: TIPI E STRUTTURE
 * ========================================================================== */

/**
 * @brief Unione necessaria per le operazioni semctl(SETVAL, etc.).
 * 
 * Definita secondo lo standard POSIX per garantire la compatibilità
 * del controllo dei semafori IPC System V.
 */
union semun {
    int val;                /**< Valore per SETVAL */
    struct semid_ds *buf;   /**< Buffer per IPC_STAT, IPC_SET */
    unsigned short *array;  /**< Array per GETALL, SETALL */
    struct seminfo *__buf;  /**< Buffer per IPC_INFO */
};

/* ==========================================================================
 *                    SEZIONE: CREAZIONE E DISTRUZIONE
 * ========================================================================== */

/**
 * @brief Crea un set di semafori.
 * 
 * @param key Chiave IPC (generata tramite ftok o IPC_PRIVATE).
 * @param sem_num Numero di semafori da includere nel set.
 * @param flags Permessi e flag di creazione (es. IPC_CREAT | 0666).
 * @return int ID del set di semafori, o -1 in caso di errore.
 */
int create_sem_set(key_t key, int sem_num, int flags);

/**
 * @brief Inizializza il valore di un singolo semaforo nel set.
 * 
 * @param sem_id ID del set di semafori.
 * @param sem_num Indice del semaforo all'interno del set (0..nsems-1).
 * @param init_val Valore intero da assegnare al semaforo.
 * @return int 0 successo, -1 errore.
 */
int init_sem_val(int sem_id, int sem_num, int init_val);

/**
 * @brief Rimuove definitivamente un set di semafori dal sistema.
 * 
 * @param sem_id ID del set di semafori da eliminare.
 * @return int 0 successo, -1 errore.
 */
int delete_sem_set(int sem_id);

/* ==========================================================================
 *                      SEZIONE: OPERAZIONI P (RESERVE)
 * ========================================================================== */

/**
 * @brief Esegue l'operazione di acquisizione risorsa (P).
 * 
 * Decrementa il valore del semaforo di 1. Se il valore è 0, il processo si blocca.
 * Utilizza SEM_UNDO. Riprova automaticamente se interrotta da segnali (EINTR).
 * 
 * @param sem_id ID del set di semafori.
 * @param semaphore_index Indice del semaforo su cui operare (0-based).
 * @return int 0 successo, -1 errore critico.
 */
int reserve_sem(int sem_id, int semaphore_index);

/**
 * @brief Esegue l'acquisizione atomica di una quantità N di risorse.
 * 
 * @param sem_id ID del set di semafori.
 * @param semaphore_index Indice del semaforo.
 * @param amount Quantità di risorse da acquisire contemporaneamente.
 * @return int 0 successo, -1 errore.
 */
int reserve_sem_amount(int sem_id, int semaphore_index, int amount);

/**
 * @brief Esegue l'operazione di acquisizione risorsa (P) senza flag SEM_UNDO.
 * 
 * Da utilizzare specificamente per le barriere di sincronizzazione dove le morti 
 * sono gestite manualmente dall'handler SIGCHLD del Master.
 */
int reserve_sem_no_undo(int sem_id, int semaphore_index);

/**
 * @brief Esegue l'operazione di rilascio risorsa (V).
 * 
 * Incrementa il valore del semaforo di 1. Sblocca eventuali processi in attesa.
 * Utilizza SEM_UNDO.
 * 
 * @param sem_id ID del set di semafori.
 * @param semaphore_index Indice del semaforo su cui operare.
 * @return int 0 successo, -1 errore.
 */
int release_sem(int sem_id, int semaphore_index);

/**
 * @brief Esegue il rilascio atomico di una quantità N di risorse.
 * 
 * @param sem_id ID del set di semafori.
 * @param semaphore_index Indice del semaforo.
 * @param amount Quantità di risorse da rilasciare.
 * @return int 0 successo, -1 errore.
 */
int release_sem_amount(int sem_id, int semaphore_index, int amount);

/**
 * @brief Esegue l'operazione di rilascio risorsa (V) senza flag SEM_UNDO.
 */
int release_sem_no_undo(int sem_id, int semaphore_index);

/**
 * @brief Tenta di acquisire una risorsa (P) in modo non bloccante e senza SEM_UNDO.
 * 
 * Se la risorsa non è disponibile (valore 0), la funzione ritorna immediatamente -1 
 * con errno impostato a EAGAIN. Ideale per l'uso in handler di segnali.
 */
int reserve_sem_try_no_undo(int sem_id, int semaphore_index);

/* ==========================================================================
 *                       SEZIONE: ATTESA E UTILITY
 * ========================================================================== */

/**
 * @brief Attende che il valore del semaforo diventi 0.
 * 
 * Riprova automaticamente se interrotta da segnali (EINTR).
 * Usato per i "cancelli" delle barriere.
 * 
 * @param sem_id ID del set di semafori.
 * @param semaphore_index Indice del semaforo su cui attendere.
 * @return int 0 successo, -1 errore critico.
 */
int wait_for_zero(int sem_id, int semaphore_index);

/**
 * @brief Recupera il valore attuale di un semaforo.
 * 
 * @param sem_id ID del set di semafori.
 * @param sem_num Indice del semaforo all'interno del set (0-based).
 * @return int Il valore attuale (>=0) o -1 in caso di errore.
 */
int get_sem_val(int sem_id, int sem_num);

/**
 * @brief Esegue P (reserve) ma non riprova su EINTR.
 * Utile per bloccare un processo permettendogli però di svegliarsi se arriva un segnale (es. fine giorno).
 */
int reserve_sem_interruptible(int sem_id, int semaphore_index);

/**
 * @brief Attende lo zero ma non riprova su EINTR.
 */
int wait_for_zero_interruptible(int sem_id, int semaphore_index);

/* ==========================================================================
 *                     SEZIONE: BARRIERE DI SINCRONIZZAZIONE
 * ========================================================================== */

/**
 * @brief Inizializza o resetta una barriera di sincronizzazione.
 * 
 * Imposta il semaforo dei pronti al numero di processi attesi e chiude il cancello 
 * (impostandolo a 1).
 * 
 * @param sem_id ID del set di semafori.
 * @param ready_idx Indice del semaforo dei pronti.
 * @param gate_idx Indice del semaforo cancello.
 * @param n_processes Numero di processi da attendere.
 * @return int 0 successo, -1 errore.
 */
int setup_barrier(int sem_id, int ready_idx, int gate_idx, int n_processes);

/**
 * @brief Apre il cancello della barriera, sbloccando i processi in attesa.
 * 
 * Imposta il valore del semaforo cancello a 0.
 * 
 * @param sem_id ID del set di semafori.
 * @param gate_idx Indice del semaforo cancello.
 * @return int 0 successo, -1 errore.
 */
int open_barrier_gate(int sem_id, int gate_idx);

/**
 * @brief Sincronizza l'avvio di un processo figlio.
 * 
 * Utilizza una barriera a due stadi: incrementa (idealmente decrementa per segnalare presenza) 
 * un semaforo di pronti e attende che il cancello (gate) diventi zero.
 * 
 * @param sem_id ID del set di semafori di avvio.
 * @param ready_idx Indice del semaforo su cui segnalare la disponibilità.
 * @param gate_idx Indice del semaforo cancello da attendere.
 * @return int 0 successo, -1 errore.
 */
int sync_child_start(int sem_id, int ready_idx, int gate_idx);

#endif /* SEM_H */
