/**
 * @file queue.h
 * @brief Definizioni per la gestione delle code di messaggi (System V IPC).
 * 
 * Questo modulo fornisce un'astrazione sopra le primitive msgsnd, msgrcv e msgctl
 * per facilitare lo scambio di dati tra i processi della simulazione.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>  
#include <sys/ipc.h>    
#include <sys/msg.h>    
#include <stddef.h>

/* ==========================================================================
 *                        SEZIONE: COSTANTI E STRUTTURE
 * ========================================================================== */

/** Dimensione massima del buffer di testo per i messaggi della simulazione */
#define MAX_MESSAGE_TEXT_SIZE 256

/**
 * @brief Struttura standard per lo scambio di messaggi IPC.
 * 
 * NOTA: Il kernel richiede che il primo campo sia un long (mtype).
 * Questa struttura verrà utilizzata per passare ordini, feedback e stati.
 */
typedef struct {
    long message_type;                           /**< Tipo/Priorità del messaggio (deve essere > 0) */
    char message_text[MAX_MESSAGE_TEXT_SIZE];    /**< Payload del messaggio */
} SimulationMessage;

/* ==========================================================================
 *                         SEZIONE: FUNZIONI PUBBLICHE
 * ========================================================================== */

/**
 * @brief Crea o ottiene un identificatore per una coda di messaggi.
 * 
 * @param key Chiave IPC (solitamente generata tramite ftok o IPC_PRIVATE).
 * @param message_flags Permessi e flag di creazione (es. IPC_CREAT | 0666).
 * @return int L'identificatore della coda (msqid) o -1 in caso di errore.
 */
int create_message_queue(key_t key, int message_flags);

/**
 * @brief Invia un messaggio alla coda specificata.
 * 
 * Gestisce internamente l'interruzione da segnali (EINTR) riprovando l'invio.
 * 
 * @param message_queue_id Identificatore della coda di messaggi.
 * @param message_pointer Puntatore alla struttura SimulationMessage da inviare.
 * @param message_size Dimensione effettiva del payload (escluso message_type).
 * @param message_flags Flag di controllo per l'invio (es. IPC_NOWAIT).
 * @return int 0 in caso di successo, -1 in caso di errore critico.
 */
int send_message_to_queue(int message_queue_id, SimulationMessage *message_pointer, size_t message_size, int message_flags);

/**
 * @brief Riceve un messaggio dalla coda specificata.
 * 
 * Gestisce internamente l'interruzione da segnali (EINTR) a meno che non si verifichino errori gravi.
 * 
 * @param message_queue_id Identificatore della coda di messaggi.
 * @param message_pointer Puntatore al buffer SimulationMessage dove salvare il dato.
 * @param maximum_message_size Dimensione massima del payload (mtext) accettabile.
 * @param message_type Selettore del tipo di messaggio (0 per il primo, >0 per tipo specifico).
 * @param message_flags Flag di controllo per la ricezione (es. MSG_NOERROR).
 * @return ssize_t Numero di byte ricevuti nel payload o -1 in caso di errore.
 */
ssize_t receive_message_from_queue(int message_queue_id, SimulationMessage *message_pointer, size_t maximum_message_size, long message_type, int message_flags);

/**
 * @brief Rimuove definitivamente una coda di messaggi dal sistema operativo.
 * 
 * @param message_queue_id Identificatore della coda da eliminare.
 * @return int 0 in caso di successo, -1 in caso di errore.
 */
int remove_message_queue(int message_queue_id);

/**
 * @brief Recupera le informazioni di stato e controllo di una coda di messaggi.
 * 
 * @param message_queue_id Identificatore della coda.
 * @param statistics_buffer Puntatore alla struttura msqid_ds in cui scrivere i dati.
 * @return int 0 in caso di successo, -1 in caso di errore.
 */
int get_message_queue_statistics(int message_queue_id, struct msqid_ds *statistics_buffer);

/**
 * @brief Restituisce il numero di messaggi attualmente presenti nella coda.
 * 
 * @param message_queue_id ID della coda da interrogare.
 * @return int Numero di messaggi (msg_qnum) o -1 in caso di errore.
 */
int get_message_queue_length(int message_queue_id);

#endif /* QUEUE_H */
