#include "../../include/menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void load_menu_from_file(const char *path, MensaMenu *dest) {
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("ERRORE: Impossibile aprire il file menu");
        exit(EXIT_FAILURE);
    }

    // Inizializziamo i contatori a 0 per sicurezza
    dest->n_primi = 0;
    dest->n_secondi = 0;
    dest->n_dessert = 0;

    char line[128];
    int current_section = 0; // 0=None, 1=Primi, 2=Secondi, 3=Dessert

    while (fgets(line, sizeof(line), file)) {
        // 1. Pulizia stringa: Rimuovi newline finale
        line[strcspn(line, "\n")] = 0;
        line[strcspn(line, "\r")] = 0; // Per compatibilità Windows/Linux

        // 2. Salta righe vuote o commenti
        if (strlen(line) == 0 || line[0] == '#') continue;

        // 3. Rilevamento Sezione
        if (strcmp(line, "[PRIMI]") == 0) {
            current_section = 1;
            continue;
        } else if (strcmp(line, "[SECONDI]") == 0) {
            current_section = 2;
            continue;
        } else if (strcmp(line, "[DESSERT]") == 0) {
            current_section = 3;
            continue;
        }

        // 4. Inserimento Dati (in base alla sezione attiva)
        if (current_section == 1 && dest->n_primi < MAX_DISHES_PER_COURSE) {
            strncpy(dest->primi[dest->n_primi], line, MAX_NAME_LEN - 1);
            dest->n_primi++;
        } 
        else if (current_section == 2 && dest->n_secondi < MAX_DISHES_PER_COURSE) {
            strncpy(dest->secondi[dest->n_secondi], line, MAX_NAME_LEN - 1);
            dest->n_secondi++;
        } 
        else if (current_section == 3 && dest->n_dessert < MAX_DISHES_PER_COURSE) {
            strncpy(dest->dessert[dest->n_dessert], line, MAX_NAME_LEN - 1);
            dest->n_dessert++;
        }
    }

    fclose(file);
    printf("Menu caricato: %d Primi, %d Secondi, %d Dessert.\n", 
           dest->n_primi, dest->n_secondi, dest->n_dessert);
}