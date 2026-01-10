/**
 * @file queue.h
 * @brief Gestione delle Code di Messaggi POSIX per gli ordini.
 * * Definisce la struttura del messaggio "ordine" che viaggia dall'Utente
 * verso le stazioni (Primi, Secondi, Dessert).
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <mqueue.h>
#include <sys/types.h> // Per pid_t

/**
 * @brief Payload del messaggio (L'ordine).
 * Questo è ciò che viaggia fisicamente nella coda.
 * NON contiene stringhe, ma solo l'ID numerico che fa riferimento
 * all'indice dell'array Menu nella Shared Memory.
 */
typedef struct {
    pid_t id_utente; /**< PID del processo Utente (Mittente). Serve per i log. */
    int id_piatto;   /**< Indice del piatto nel menu (0, 1, 2...). */
} OrderData;

/**
 * @brief Crea una nuova coda di messaggi (Server side).
 * * @param name      Nome della coda (es. "/coda_primi").
 * @param max_msgs  Numero massimo di ordini in attesa.
 * @param msg_size  Dimensione della struct (sizeof(OrderData)).
 * * @return mqd_t Descrittore della coda creata.
 */
mqd_t create_queue(const char *name, long max_msgs, long msg_size);

/**
 * @brief Apre una coda esistente (Client side / Utente).
 * * @param name Nome della coda da aprire.
 * * @return mqd_t Descrittore della coda.
 */
mqd_t open_queue(const char *name);

/**
 * @brief Invia un ordine alla stazione (Bloccante).
 * * @param qd  Descrittore della coda.
 * @param msg Puntatore alla struct OrderData contenente PID e ID Piatto.
 */
void send_message(mqd_t qd, OrderData *msg);

/**
 * @brief Riceve un ordine (Bloccante).
 * * @param qd  Descrittore della coda.
 * @param msg Puntatore a una struct OrderData vuota dove salvare i dati.
 * * @return ssize_t Byte letti o -1 in caso di errore.
 */
ssize_t receive_message(mqd_t qd, OrderData *msg);

/**
 * @brief Chiude e rimuove la coda (Pulizia).
 */
void close_and_unlink_queue(mqd_t qd, const char *name);

#endif