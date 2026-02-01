/**
 * @file sem.c
 * @brief Implementazione wrapper per i semafori System V IPC.
 * 
 * Fornisce operazioni P/V robuste con gestione automatica EINTR,
 * funzioni per barriere di sincronizzazione e lettura valori.
 * 
 * @see sem.h per la documentazione delle funzioni pubbliche.
 */

/* Includes di sistema */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

/* Includes del progetto */
#include "sem.h"

/* ==========================================================================
 *                          SEZIONE: FUNZIONI PRIVATE
 * ========================================================================== */

/**
 * Esegue un'operazione semaforica atomica con retry su EINTR.
 */

static int _execute_sem_op(int sem_id, int sem_idx, short op, short flags) {
    struct sembuf sb;
    sb.sem_num = (unsigned short)sem_idx;
    sb.sem_op = op;
    sb.sem_flg = flags;

    /* Loop robusto: riprova se interrotto da segnale (EINTR) */
    while (semop(sem_id, &sb, 1) == -1) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return 0;
}

/* ==========================================================================
 *                    SEZIONE: CREAZIONE E DISTRUZIONE
 * ========================================================================== */

/** Crea un nuovo set di semafori. */
int create_sem_set(key_t key, int sem_num, int flags) {
    int sem_id = semget(key, sem_num, flags);
    if (sem_id == -1) {
        perror("IPC Error: semget failed");
    }
    return sem_id;
}

/** Inizializza un semaforo a un valore specifico. */
int init_sem_val(int sem_id, int sem_num, int init_val) {
    union semun arg;
    arg.val = init_val;
    
    if (semctl(sem_id, sem_num, SETVAL, arg) == -1) {
        perror("IPC Error: semctl SETVAL failed");
        return -1;
    }
    return 0;
}

/** Rimuove un set di semafori dal sistema. */
int delete_sem_set(int sem_id) {
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("IPC Error: semctl IPC_RMID failed");
        return -1;
    }
    return 0;
}

/* ==========================================================================
 *                      SEZIONE: OPERAZIONI P (RESERVE)
 * ========================================================================== */

/** P (Wait) con SEM_UNDO. */
int reserve_sem(int sem_id, int semaphore_index) {
    return _execute_sem_op(sem_id, semaphore_index, -1, SEM_UNDO);
}

/** P di N risorse con SEM_UNDO. */
int reserve_sem_amount(int sem_id, int semaphore_index, int amount) {
    if (amount <= 0) return 0;
    return _execute_sem_op(sem_id, semaphore_index, (short)(-amount), SEM_UNDO);
}

/** P senza UNDO (per barriere gestite manualmente). */
int reserve_sem_no_undo(int sem_id, int semaphore_index) {
    return _execute_sem_op(sem_id, semaphore_index, -1, 0);
}

/** P non bloccante senza UNDO (per handler segnali). */
int reserve_sem_try_no_undo(int sem_id, int semaphore_index) {
    return _execute_sem_op(sem_id, semaphore_index, -1, IPC_NOWAIT);
}

/* ==========================================================================
 *                      SEZIONE: OPERAZIONI V (RELEASE)
 * ========================================================================== */

/** V (Signal) con SEM_UNDO. */
int release_sem(int sem_id, int semaphore_index) {
    return _execute_sem_op(sem_id, semaphore_index, 1, SEM_UNDO);
}

/** V di N risorse con SEM_UNDO. */
int release_sem_amount(int sem_id, int semaphore_index, int amount) {
    if (amount <= 0) return 0;
    return _execute_sem_op(sem_id, semaphore_index, (short)amount, SEM_UNDO);
}

/** V senza UNDO. */
int release_sem_no_undo(int sem_id, int semaphore_index) {
    return _execute_sem_op(sem_id, semaphore_index, 1, 0);
}

/** V di N senza UNDO. */
int release_sem_n(int sem_id, int semaphore_index, int increment_value) {
    return _execute_sem_op(sem_id, semaphore_index, (short)increment_value, 0);
}

/* ==========================================================================
 *                       SEZIONE: ATTESA E UTILITY
 * ========================================================================== */

/** Attende che il semaforo diventi 0 (cancello aperto). */
int wait_for_zero(int sem_id, int semaphore_index) {
    return _execute_sem_op(sem_id, semaphore_index, 0, 0);
}

/** Inizializzazione riserva interrompibile (ritorna su EINTR) */
int reserve_sem_interruptible(int sem_id, int semaphore_index) {
    struct sembuf sb;
    sb.sem_num = (unsigned short)semaphore_index;
    sb.sem_op = -1;
    sb.sem_flg = SEM_UNDO;
    return semop(sem_id, &sb, 1);
}

/** Attesa zero interrompibile (ritorna su EINTR) */
int wait_for_zero_interruptible(int sem_id, int semaphore_index) {
    struct sembuf sb;
    sb.sem_num = (unsigned short)semaphore_index;
    sb.sem_op = 0;
    sb.sem_flg = 0;
    return semop(sem_id, &sb, 1);
}

/** Legge il valore corrente di un semaforo. */
int get_sem_val(int sem_id, int sem_num) {
    int val = semctl(sem_id, sem_num, GETVAL);
    if (val == -1) {
        perror("IPC Error: semctl(GETVAL) failed");
    }
    return val;
}

/* ==========================================================================
 *                     SEZIONE: BARRIERE DI SINCRONIZZAZIONE
 * ========================================================================== */

/** Inizializza una barriera a N processi (ready=N, gate=1). */
int setup_barrier(int sem_id, int ready_idx, int gate_idx, int n_processes) {
    if (init_sem_val(sem_id, ready_idx, n_processes) == -1) return -1;
    return init_sem_val(sem_id, gate_idx, 1);
}

/** Apre il cancello della barriera (gate=0). */
int open_barrier_gate(int sem_id, int gate_idx) {
    return init_sem_val(sem_id, gate_idx, 0);
}

/** Sincronizza un figlio: decrementa ready, attende gate=0. */
int sync_child_start(int sem_id, int ready_idx, int gate_idx) {
    /* Per le barriere usiamo NO_UNDO: il SIGCHLD del Master gestisce le morti */
    if (reserve_sem_no_undo(sem_id, ready_idx) == -1) return -1;
    return wait_for_zero(sem_id, gate_idx);
}
