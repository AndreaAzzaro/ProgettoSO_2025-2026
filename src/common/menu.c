#include "../../include/menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 1. ENUM PER GLI STATI (LE SEZIONI)
typedef enum {
    SECTION_NONE = 0,
    SECTION_PRIMI,
    SECTION_SECONDI,
    SECTION_DESSERT,
    SECTION_UNKNOWN // Usato quando la riga non è un tag [SEZIONE]
} MenuSection;

// 2. FUNZIONE HELPER: CAPISCE SE LA RIGA È UN TAG
// Se la riga è "[PRIMI]" ritorna SECTION_PRIMI.
// Se la riga è "Pasta al sugo", ritorna SECTION_UNKNOWN.
MenuSection resolve_section_tag(const char *line) {
    if (strcmp(line, "[PRIMI]") == 0)   return SECTION_PRIMI;
    if (strcmp(line, "[SECONDI]") == 0) return SECTION_SECONDI;
    if (strcmp(line, "[DESSERT]") == 0) return SECTION_DESSERT;
    return SECTION_UNKNOWN;
}

// 3. IMPLEMENTAZIONE DEL CARICAMENTO
void load_menu_from_file(const char *path, MensaMenu *dest) {
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("ERRORE: Impossibile aprire il file menu");
        exit(EXIT_FAILURE);
    }

    // Reset contatori
    dest->n_primi = 0;
    dest->n_secondi = 0;
    dest->n_dessert = 0;

    char line[128];
    MenuSection current_state = SECTION_NONE; // Stato iniziale

    while (fgets(line, sizeof(line), file)) {
        // Pulizia stringa
        line[strcspn(line, "\n")] = 0;
        line[strcspn(line, "\r")] = 0;

        // Salta righe vuote o commenti
        if (strlen(line) == 0 || line[0] == '#') continue;

        // --- FASE 1: CONTROLLO SE È UN TAG DI SEZIONE ---
        MenuSection new_tag = resolve_section_tag(line);
        
        if (new_tag != SECTION_UNKNOWN) {
            // Se la riga era un tag (es. [PRIMI]), cambio lo stato e passo oltre
            current_state = new_tag;
            continue; 
        }

        // --- FASE 2: INSERIMENTO PIATTO (SWITCH SULLO STATO) ---
        // Se arrivo qui, la riga è un piatto (es. "Carbonara").
        // Decido dove metterlo in base allo stato attuale.
        
        switch (current_state) {
            case SECTION_PRIMI:
                if (dest->n_primi < MAX_DISHES_PER_COURSE) {
                    strncpy(dest->primi[dest->n_primi], line, MAX_NAME_LEN - 1);
                    dest->n_primi++;
                }
                break;

            case SECTION_SECONDI:
                if (dest->n_secondi < MAX_DISHES_PER_COURSE) {
                    strncpy(dest->secondi[dest->n_secondi], line, MAX_NAME_LEN - 1);
                    dest->n_secondi++;
                }
                break;

            case SECTION_DESSERT:
                if (dest->n_dessert < MAX_DISHES_PER_COURSE) {
                    strncpy(dest->dessert[dest->n_dessert], line, MAX_NAME_LEN - 1);
                    dest->n_dessert++;
                }
                break;

            case SECTION_NONE:
            default:
                // Se trovo testo prima di qualsiasi tag [SEZIONE]
                // fprintf(stderr, "Warning: Piatto orfano ignorato: %s\n", line);
                break;
        }
    }

    fclose(file);
    printf("Menu caricato: %d Primi, %d Secondi, %d Dessert.\n", 
           dest->n_primi, dest->n_secondi, dest->n_dessert);
}