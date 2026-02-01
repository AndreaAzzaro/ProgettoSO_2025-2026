/**
 * @file menu.h
 * @brief Definizioni per la gestione del menu della mensa.
 * 
 * Questo modulo definisce le strutture dati per rappresentare il menu
 * della mensa, caricato dal file `config/menu.conf`.
 * 
 * @see load_simulation_menu() per il caricamento dei piatti.
 */

#ifndef MENU_H
#define MENU_H

/* ==========================================================================
 *                              SEZIONE: COSTANTI
 * ========================================================================== */

/** Lunghezza massima del nome di un piatto (incluso terminatore) */
#define MAX_DISH_NAME_LENGTH 32

/** Numero massimo di piatti per ogni categoria del menu */
#define MAX_DISHES_PER_CATEGORY 20

/** Percorso del file di configurazione del menu */
#define MENU_CONFIGURATION_PATH "config/menu.conf"

/* ==========================================================================
 *                           SEZIONE: TIPI E STRUTTURE
 * ========================================================================== */

/**
 * @brief Identificatori per le categorie di piatti nel menu.
 */
typedef enum {
    MENU_DISH_TYPE_FIRST_COURSE = 0,
    MENU_DISH_TYPE_SECOND_COURSE = 1,
    MENU_DISH_TYPE_SIDE_COURSE = 2,
    MENU_DISH_TYPE_DESSERT = 3,
    MENU_DISH_TYPE_BEVERAGE = 4,
    MENU_DISH_TYPE_COUNT
} MenuDishCategory;

/**
 * @brief Rappresentazione di un singolo piatto nel menu.
 */
typedef struct {
    char name[MAX_DISH_NAME_LENGTH];  /**< Nome del piatto (es. "Spaghetti al pomodoro") */
} MenuDish;

/**
 * @brief Struttura completa del menu della mensa.
 * 
 * Contiene tutti i piatti suddivisi per categoria, con contatori
 * per il numero effettivo di piatti caricati in ogni categoria.
 */
typedef struct {
    MenuDish first_courses[MAX_DISHES_PER_CATEGORY];   /**< Array dei primi piatti */
    int number_of_first_courses;                        /**< Numero di primi caricati */

    MenuDish second_courses[MAX_DISHES_PER_CATEGORY];  /**< Array dei secondi piatti */
    int number_of_second_courses;                       /**< Numero di secondi caricati */

    MenuDish side_courses[MAX_DISHES_PER_CATEGORY];    /**< Array dei contorni */
    int number_of_side_courses;                         /**< Numero di contorni caricati */

    MenuDish dessert_courses[MAX_DISHES_PER_CATEGORY]; /**< Array dei dolci */
    int number_of_dessert_courses;                      /**< Numero di dolci caricati */

    MenuDish beverage_courses[MAX_DISHES_PER_CATEGORY];/**< Array delle bevande */
    int number_of_beverage_courses;                     /**< Numero di bevande caricate */
} SimulationMenu;

/* ==========================================================================
 *                         SEZIONE: FUNZIONI PUBBLICHE
 * ========================================================================== */

/**
 * @brief Carica il menu dal file di configurazione.
 * 
 * Legge il file `config/menu.conf` e popola la struttura SimulationMenu.
 * In caso di errore di lettura, termina il processo con EXIT_FAILURE.
 * 
 * @return SimulationMenu Struttura popolata con i piatti letti.
 */
SimulationMenu load_simulation_menu();

/**
 * @brief Recupera il nome di un piatto partendo dalla categoria e dall'indice.
 * 
 * Questa funzione permette di risolvere l'ID scambiato nelle code di messaggi
 * per ottenere il nome leggibile del piatto.
 * 
 * @param menu_ptr Puntatore alla struttura menu in memoria condivisa.
 * @param category Categoria del piatto (MenuDishCategory).
 * @param dish_index Indice del piatto all'interno della categoria (0-based).
 * @return const char* Nome del piatto o "Sconosciuto" in caso di indice invalido.
 */
const char* get_dish_name_by_id(SimulationMenu *menu_ptr, MenuDishCategory category, int dish_index);

#endif /* MENU_H */
