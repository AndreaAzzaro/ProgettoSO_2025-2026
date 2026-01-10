/**
 * @file menu.h
 * @brief Gestione del Menu della mensa.
 * Definisce le strutture dati per memorizzare i piatti e le funzioni
 * per caricare il menu da file.
 */

#ifndef MENU_H
#define MENU_H

#include <stddef.h> // per size_t

// --- COSTANTI DIMENSIONALI ---
#define MAX_DISHES_PER_COURSE 20  /**< Numero massimo di piatti per categoria. */
#define MAX_NAME_LEN 64           /**< Lunghezza massima del nome di un piatto. */

/**
 * @brief Struttura che contiene l'intero menu giornaliero.
 * Questa struct sarà un membro della SharedMensa.
 */
typedef struct {
    // Array bidimensionali: [Indice ID][Caratteri Stringa]
    
    char primi[MAX_DISHES_PER_COURSE][MAX_NAME_LEN];
    int n_primi;    /**< Contatore: quanti primi sono stati caricati (es. 4). */

    char secondi[MAX_DISHES_PER_COURSE][MAX_NAME_LEN];
    int n_secondi;

    char dessert[MAX_DISHES_PER_COURSE][MAX_NAME_LEN];
    int n_dessert;

} MensaMenu;

/**
 * @brief Legge il file di testo e popola la struttura Menu.
 * * @param path Percorso del file menu.conf.
 * @param dest Puntatore alla struct MensaMenu (solitamente dentro la SHM) dove scrivere i dati.
 * * @pre 'dest' deve puntare a un'area di memoria già allocata (es. in Shared Memory).
 * @post La struct puntata da 'dest' conterrà i nomi dei piatti e i contatori aggiornati.
 */
void load_menu_from_file(const char *path, MensaMenu *dest);

#endif