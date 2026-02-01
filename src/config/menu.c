/**
 * @file menu.c
 * @brief Implementazione del parser per il file di configurazione del menu.
 * 
 * Legge il file `config/menu.conf` e popola la struttura SimulationMenu.
 * Formato file: una riga per piatto con "CATEGORIA NOME" (es. "P Spaghetti").
 * 
 * @see menu.h per la documentazione delle strutture.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Includes del progetto */
#include "menu.h"

/* ==========================================================================
 *                          SEZIONE: TIPI E MAPPING
 * ========================================================================== */

/**
 * @brief Identificatori interni per la risoluzione della chiave nel file.
 */
typedef enum {
    CATEGORY_KEY_UNKNOWN = -1,
    CATEGORY_KEY_FIRST,
    CATEGORY_KEY_SECOND,
    CATEGORY_KEY_SIDE,
    CATEGORY_KEY_DESSERT,
    CATEGORY_KEY_BEVERAGE
} MenuCategoryKey;

/**
 * @brief Struttura per il mapping tra flag del file e categoria interna.
 */
typedef struct {
    const char *character_key;
    MenuCategoryKey enum_identifier;
} MenuCategoryKeyMap;

/** Tabella di lookup per i flag del menu (P=Primo, S=Secondo, etc.) */
static const MenuCategoryKeyMap menu_category_mapping_table[] = {
    {"P", CATEGORY_KEY_FIRST},
    {"S", CATEGORY_KEY_SECOND},
    {"C", CATEGORY_KEY_SIDE},
    {"D", CATEGORY_KEY_DESSERT},
    {"B", CATEGORY_KEY_BEVERAGE},
    {NULL, CATEGORY_KEY_UNKNOWN}  /* Terminatore */
};

/* ==========================================================================
 *                          SEZIONE: FUNZIONI PRIVATE
 * ========================================================================== */

/**
 * @brief Risolve il flag testuale del file nella categoria corrispondente.
 */
static MenuCategoryKey resolve_menu_category_key(const char *key_string) {
    MenuCategoryKey identified_category = CATEGORY_KEY_UNKNOWN;
    int i = 0;
    while (menu_category_mapping_table[i].character_key != NULL) {
        if (strcmp(key_string, menu_category_mapping_table[i].character_key) == 0) {
            identified_category = menu_category_mapping_table[i].enum_identifier;
        }
        i++;
    }
    return identified_category;
}

/* ==========================================================================
 *                          SEZIONE: FUNZIONI PUBBLICHE
 * ========================================================================== */

/**
 * Carica il menu dal file di configurazione.
 * Fallback: cerca "config/menu.conf" se MENU_CONFIGURATION_PATH fallisce.
 */
SimulationMenu load_simulation_menu() {
    SimulationMenu menu_data;
    memset(&menu_data, 0, sizeof(SimulationMenu));

    FILE *menu_config_file = fopen(MENU_CONFIGURATION_PATH, "r");
    if (menu_config_file == NULL) {
        menu_config_file = fopen("config/menu.conf", "r");
        if (menu_config_file == NULL) {
            perror("ERRORE CRITICO: Impossibile aprire file menu.conf");
            exit(EXIT_FAILURE);
        }
    }

    char line_buffer[128];
    while (fgets(line_buffer, sizeof(line_buffer), menu_config_file)) {
        /* Gestione righe valide (no commenti, no vuote) - Sostituzione di 'continue' */
        if (line_buffer[0] != '#' && line_buffer[0] != '\n' && line_buffer[0] != '\r') {
            
            char type_identifier[4];
            char dish_name[MAX_DISH_NAME_LENGTH];

            /* Parsing: Identificatore categoria (P, S, C, D, B) e Nome Piatto */
            if (sscanf(line_buffer, "%3s %31s", type_identifier, dish_name) == 2) {
                
                switch (resolve_menu_category_key(type_identifier)) {
                    case CATEGORY_KEY_FIRST:
                        if (menu_data.number_of_first_courses < MAX_DISHES_PER_CATEGORY) {
                            strcpy(menu_data.first_courses[menu_data.number_of_first_courses].name, dish_name);
                            menu_data.number_of_first_courses++;
                        }
                        break;
                    case CATEGORY_KEY_SECOND:
                        if (menu_data.number_of_second_courses < MAX_DISHES_PER_CATEGORY) {
                            strcpy(menu_data.second_courses[menu_data.number_of_second_courses].name, dish_name);
                            menu_data.number_of_second_courses++;
                        }
                        break;
                    case CATEGORY_KEY_SIDE:
                        if (menu_data.number_of_side_courses < MAX_DISHES_PER_CATEGORY) {
                            strcpy(menu_data.side_courses[menu_data.number_of_side_courses].name, dish_name);
                            menu_data.number_of_side_courses++;
                        }
                        break;
                    case CATEGORY_KEY_DESSERT:
                        if (menu_data.number_of_dessert_courses < MAX_DISHES_PER_CATEGORY) {
                            strcpy(menu_data.dessert_courses[menu_data.number_of_dessert_courses].name, dish_name);
                            menu_data.number_of_dessert_courses++;
                        }
                        break;
                    case CATEGORY_KEY_BEVERAGE:
                        if (menu_data.number_of_beverage_courses < MAX_DISHES_PER_CATEGORY) {
                            strcpy(menu_data.beverage_courses[menu_data.number_of_beverage_courses].name, dish_name);
                            menu_data.number_of_beverage_courses++;
                        }
                        break;
                    case CATEGORY_KEY_UNKNOWN:
                    default:
                        fprintf(stderr, "Warning: Categoria piatto '%s' non riconosciuta.\n", type_identifier);
                        break;
                }
            }
        }
    }

    fclose(menu_config_file);
    printf("[MENU] Configurazione menu caricata correttamente.\n");
    return menu_data;
}

/**
 * Risolve l'ID di un piatto nel suo nome leggibile.
 * Ritorna "Sconosciuto" se l'indice non è valido.
 */
const char* get_dish_name_by_id(SimulationMenu *menu_ptr, MenuDishCategory category, int dish_index) {
    const char* result = "Sconosciuto";

    if (menu_ptr != NULL && dish_index >= 0 && dish_index < MAX_DISHES_PER_CATEGORY) {
        switch (category) {
            case MENU_DISH_TYPE_FIRST_COURSE:
                result = menu_ptr->first_courses[dish_index].name;
                break;
            case MENU_DISH_TYPE_SECOND_COURSE:
                result = menu_ptr->second_courses[dish_index].name;
                break;
            case MENU_DISH_TYPE_SIDE_COURSE:
                result = menu_ptr->side_courses[dish_index].name;
                break;
            case MENU_DISH_TYPE_DESSERT:
                result = menu_ptr->dessert_courses[dish_index].name;
                break;
            case MENU_DISH_TYPE_BEVERAGE:
                result = menu_ptr->beverage_courses[dish_index].name;
                break;
            default:
                break;
        }
    }
    
    return result;
}
