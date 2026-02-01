/**
 * @file queue.c
 * @brief Implementazione wrapper per le code di messaggi System V IPC.
 * 
 * Fornisce un'astrazione sopra msgsnd, msgrcv e msgctl con gestione
 * automatica delle interruzioni da segnale (EINTR).
 * 
 * @see queue.h per la documentazione delle funzioni pubbliche.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

/* Includes del progetto */
#include "queue.h"

/* ==========================================================================
 *                       SEZIONE: CREAZIONE E RIMOZIONE
 * ========================================================================== */

/**
 * Crea o ottiene una coda di messaggi.
 */
int create_message_queue(key_t key, int message_flags) {
    int message_queue_id = msgget(key, message_flags);
    if (message_queue_id == -1) {
        perror("IPC Error: msgget failed in create_message_queue");
    }
    return message_queue_id;
}

/* ==========================================================================
 *                       SEZIONE: INVIO E RICEZIONE
 * ========================================================================== */

/**
 * Invia un messaggio alla coda.
 * Gestisce automaticamente EINTR riprovando l'operazione.
 */
int send_message_to_queue(int message_queue_id, SimulationMessage *message_pointer, size_t message_size, int message_flags) {
    /* Loop robusto: riprova se interrotto da segnale (EINTR) */
    while (msgsnd(message_queue_id, (void *)message_pointer, message_size, message_flags) == -1) {
        if (errno != EINTR) {
            perror("IPC Error: msgsnd failed in send_message_to_queue");
            return -1;
        }
    }
    return 0;
}

/**
 * Riceve un messaggio dalla coda.
 * Gestisce EINTR, ENOMSG, EIDRM silenziosamente.
 */
ssize_t receive_message_from_queue(int message_queue_id, SimulationMessage *message_pointer, size_t maximum_message_size, long message_type, int message_flags) {
    ssize_t result = msgrcv(message_queue_id, (void *)message_pointer, maximum_message_size, message_type, message_flags);
    
    if (result == -1) {
        /* Errori "normali" (segnali, coda vuota, rimossa) non loggati */
        if (errno != EINTR && errno != ENOMSG && errno != EIDRM && errno != EINVAL) {
            perror("IPC Error: msgrcv failed in receive_message_from_queue");
        }
        return -1;
    }
    return result;
}

/* ==========================================================================
 *                       SEZIONE: CONTROLLO E STATISTICHE
 * ========================================================================== */

/**
 * Rimuove definitivamente la coda dal sistema.
 */
int remove_message_queue(int message_queue_id) {
    if (msgctl(message_queue_id, IPC_RMID, NULL) == -1) {
        perror("IPC Error: msgctl(IPC_RMID) failed");
        return -1;
    }
    return 0;
}

/**
 * Recupera le statistiche della coda (wrapper msgctl IPC_STAT).
 */
int get_message_queue_statistics(int message_queue_id, struct msqid_ds *statistics_buffer) {
    if (msgctl(message_queue_id, IPC_STAT, statistics_buffer) == -1) {
        perror("IPC Error: msgctl(IPC_STAT) failed");
        return -1;
    }
    return 0;
}

/**
 * Ritorna il numero di messaggi attualmente in coda.
 */
int get_message_queue_length(int message_queue_id) {
    struct msqid_ds stats;
    if (msgctl(message_queue_id, IPC_STAT, &stats) == -1) {
        return -1;
    }
    return (int)stats.msg_qnum;
}
