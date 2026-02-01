/**
 * @file message.h
 * @brief Definizioni dei payload e dei tipi di messaggio per le code IPC.
 * 
 * Questo modulo definisce le strutture dati utilizzate come payload nelle
 * code di messaggi System V per la comunicazione tra processi della simulazione.
 * 
 * I payload vengono incapsulati nel campo `message_text` di SimulationMessage.
 * @see queue.h per le funzioni di invio/ricezione.
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#include <sys/types.h>
#include <stdbool.h>

/* ==========================================================================
 *                           SEZIONE: TIPI DI MESSAGGIO
 * ========================================================================== */

/** Messaggio inviato dall'Utente all'Operatore (ordine piatto) */
#define MSG_TYPE_ORDER 1

/** Messaggio per la gestione dinamica degli utenti (add_users -> Master) */
#define MSG_TYPE_CONTROL 2

/* ==========================================================================
 *                           SEZIONE: ENUMERAZIONI
 * ========================================================================== */

/**
 * @brief Stato della risposta dell'operatore a un ordine.
 */
typedef enum {
    ORDER_STATUS_SERVED = 1,      /**< Piatto servito con successo */
    ORDER_STATUS_OUT_OF_STOCK = 2 /**< Piatto esaurito durante l'attesa */
} OrderStatus;

/* ==========================================================================
 *                         SEZIONE: STRUTTURE PAYLOAD
 * ========================================================================== */

/**
 * @brief Payload per comunicazione Stazione <-> Utente.
 * 
 * Usato per inviare ordini e ricevere risposte dalle stazioni di distribuzione.
 */
typedef struct {
    pid_t user_pid;               /**< PID utente (usato come mtype per risposta mirata) */
    int dish_index;               /**< Indice del piatto scelto nella categoria */
    int status;                   /**< Esito dell'ordine (OrderStatus) */
} StationPayload;

/**
 * @brief Payload per richieste di aggiunta dinamica utenti.
 * 
 * Inviato dall'utility add_users al processo Master tramite control_queue_id.
 */
typedef struct {
    int users_count;              /**< Numero di utenti da aggiungere alla simulazione */
} ControlPayload;

/**
 * @brief Payload per il pagamento in Cassa.
 * 
 * Inviato dall'Utente al Cassiere con i dettagli del pasto consumato.
 */
typedef struct {
    pid_t user_pid;               /**< PID utente (usato come mtype per lo scontrino) */
    bool had_first;               /**< true se ha consumato un primo piatto */
    bool had_second;              /**< true se ha consumato un secondo piatto */
    bool want_coffee;             /**< true se desidera caffè/dolce */
    bool has_discount;            /**< true se ha presentato un ticket valido (sconto) */
} CashierPayload;

#endif /* MESSAGE_H */
