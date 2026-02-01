/**
 * @file config.c
 * @brief Implementazione del parser per il file di configurazione.
 * 
 * Legge il file `config/config.conf` e popola la struttura SimulationConfiguration.
 * Utilizza una tabella di lookup per mappare le chiavi stringa agli enum interni.
 * 
 * @see config.h per la documentazione delle strutture.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Includes del progetto */
#include "config.h"

/* ==========================================================================
 *                          SEZIONE: COSTANTI LOCALI
 * ========================================================================== */

/** Path predefinito per il file di configurazione */
#define CONFIGURATION_FILE_PATH "config/config.conf"

/** Lunghezza massima di una riga nel file di configurazione */
#define MAX_LINE_LENGTH 256

/* ==========================================================================
 *                        SEZIONE: TIPI PRIVATI (ENUM)
 * ========================================================================== */

/**
 * @brief Identificatori univoci per le chiavi di configurazione.
 */
typedef enum {
    KEY_UNKNOWN = 0,
    
    /* Quantities */
    KEY_NUMBER_OF_WORKERS, 
    KEY_NUMBER_OF_USERS, 
    KEY_NUMBER_OF_NEW_USERS_BATCH, 
    KEY_NUMBER_OF_PAUSE, 
    KEY_MAXIMUM_USERS_PER_GROUP,
    
    /* Seats */
    KEY_SEATS_PRIMI, 
    KEY_SEATS_SECONDI, 
    KEY_SEATS_COFFEE, 
    KEY_SEATS_CASSA, 
    KEY_TOTAL_DINING_SEATS,
    
    /* Prices */
    KEY_PRICE_PRIMI, 
    KEY_PRICE_SECONDI, 
    KEY_PRICE_COFFEE,
    
    /* Timings */
    KEY_SIMULATION_DURATION, 
    KEY_MEAL_DURATION, 
    KEY_NANOSECONDS_PER_TICK, 
    KEY_AVERAGE_SERVICE_PRIMI, 
    KEY_AVERAGE_SERVICE_SECONDI, 
    KEY_AVERAGE_SERVICE_COFFEE, 
    KEY_AVERAGE_SERVICE_CASSA, 
    KEY_AVERAGE_SERVICE_TICKET,
    KEY_AVERAGE_REFILL_TIME, 
    KEY_STOP_DURATION,
      
    /* Thresholds */
    KEY_OVERLOAD_THRESHOLD,
    KEY_MAXIMUM_PORTIONS_PRIMI, 
    KEY_MAXIMUM_PORTIONS_SECONDI,
    KEY_REFILL_AMOUNT_PRIMI, 
    KEY_REFILL_AMOUNT_SECONDI,
    KEY_QUEUE_PATIENCE_THRESHOLD
} ConfigurationKey;

/* ==========================================================================
 *                      SEZIONE: TABELLA DI LOOKUP
 * ========================================================================== */

/**
 * @brief Associazione tra stringa nel file e identificatore Enum.
 */
typedef struct {
    const char *string_key;        /**< Chiave come appare nel file .conf */
    ConfigurationKey enum_identifier; /**< Identificatore interno */
} ConfigurationKeyMap;

/** Tabella di lookup per la risoluzione delle chiavi */
static const ConfigurationKeyMap configuration_mapping_table[] = {
    {"NOF_WORKERS", KEY_NUMBER_OF_WORKERS},
    {"NOF_USERS", KEY_NUMBER_OF_USERS},
    {"N_NEW_USERS", KEY_NUMBER_OF_NEW_USERS_BATCH},
    {"NOF_PAUSE", KEY_NUMBER_OF_PAUSE},
    {"MAX_USERS_PER_GROUP", KEY_MAXIMUM_USERS_PER_GROUP},
    
    {"NOF_WK_SEATS_PRIMI", KEY_SEATS_PRIMI},
    {"NOF_WK_SEATS_SECONDI", KEY_SEATS_SECONDI},
    {"NOF_WK_SEATS_COFFEE", KEY_SEATS_COFFEE},
    {"NOF_WK_SEATS_CASSA", KEY_SEATS_CASSA},
    {"NOF_TABLE_SEATS", KEY_TOTAL_DINING_SEATS},
    
    {"PRICE_PRIMI", KEY_PRICE_PRIMI},
    {"PRICE_SECONDI", KEY_PRICE_SECONDI},
    {"PRICE_COFFEE", KEY_PRICE_COFFEE},
    
    {"SIM_DURATION", KEY_SIMULATION_DURATION},
    {"SIM_PASTO_DURATION", KEY_MEAL_DURATION},
    {"NNANOSECS", KEY_NANOSECONDS_PER_TICK},
    {"AVG_SRVC_PRIMI", KEY_AVERAGE_SERVICE_PRIMI},
    {"AVG_SRVC_SECONDI", KEY_AVERAGE_SERVICE_SECONDI},
    {"AVG_SRVC_COFFEE", KEY_AVERAGE_SERVICE_COFFEE},
    {"AVG_SRVC_CASSA", KEY_AVERAGE_SERVICE_CASSA},
    {"AVG_SRVC_TICKET", KEY_AVERAGE_SERVICE_TICKET},
    {"AVG_REFILL_TIME", KEY_AVERAGE_REFILL_TIME},
    {"STOP_DURATION", KEY_STOP_DURATION},
    
    {"OVERLOAD_THRESHOLD", KEY_OVERLOAD_THRESHOLD},
    {"MAX_PORZIONI_PRIMI", KEY_MAXIMUM_PORTIONS_PRIMI},
    {"MAX_PORZIONI_SECONDI", KEY_MAXIMUM_PORTIONS_SECONDI},
    {"AVG_REFILL_PRIMI", KEY_REFILL_AMOUNT_PRIMI},
    {"AVG_REFILL_SECONDI", KEY_REFILL_AMOUNT_SECONDI},
    {"QUEUE_PATIENCE_THRESHOLD", KEY_QUEUE_PATIENCE_THRESHOLD},
    
    {NULL, KEY_UNKNOWN}  /* Terminatore */
};

/* ==========================================================================
 *                       SEZIONE: FUNZIONI PRIVATE
 * ========================================================================== */

/**
 * @brief Risolve una stringa di testo nell'identificatore di configurazione corrispondente.
 */
static ConfigurationKey resolve_configuration_key(const char *key_string) {
    ConfigurationKey found_key = KEY_UNKNOWN;
    int i = 0;
    while (configuration_mapping_table[i].string_key != NULL) {
        if (strcmp(key_string, configuration_mapping_table[i].string_key) == 0) {
            found_key = configuration_mapping_table[i].enum_identifier;
        }
        i++;
    }
    return found_key;
}

/* ==========================================================================
 *                       SEZIONE: FUNZIONI PUBBLICHE
 * ========================================================================== */

/**
 * Carica la configurazione dal file.
 * Fallback: cerca "config.conf" nella directory corrente se il path principale fallisce.
 */
SimulationConfiguration load_simulation_configuration(const char *filepath) {
    SimulationConfiguration configuration;
    memset(&configuration, 0, sizeof(SimulationConfiguration));

    const char *target_path = (filepath != NULL) ? filepath : CONFIGURATION_FILE_PATH;

    FILE *config_file = fopen(target_path, "r");
    if (config_file == NULL) {
        if (filepath == NULL) {
            config_file = fopen("config.conf", "r");
        }
        
        if (config_file == NULL) {
            fprintf(stderr, "ERRORE CRITICO: Impossibile aprire il file di configurazione '%s'\n", target_path);
            exit(EXIT_FAILURE);
        }
    }
    
    if (filepath != NULL) {
        printf("[CONFIG] Caricamento da file personalizzato: %s\n", filepath);
    }

    char line_buffer[MAX_LINE_LENGTH];
    while (fgets(line_buffer, sizeof(line_buffer), config_file)) {
        if (line_buffer[0] != '#' && line_buffer[0] != '\n' && line_buffer[0] != '\r') {
            char *equality_position = strchr(line_buffer, '=');
            if (equality_position != NULL) {
                *equality_position = '\0';
                char *key_part = line_buffer;
                char *value_part = equality_position + 1;

                while (*key_part == ' ' || *key_part == '\t') key_part++;
                int key_length = strlen(key_part);
                while (key_length > 0 && (key_part[key_length - 1] == ' ' || key_part[key_length - 1] == '\t' || key_part[key_length - 1] == '\n' || key_part[key_length - 1] == '\r')) {
                    key_part[--key_length] = '\0';
                }

                long variable_value = atol(value_part);
                ConfigurationKey identified_key = resolve_configuration_key(key_part);

                switch (identified_key) {
                    case KEY_NUMBER_OF_WORKERS: configuration.quantities.number_of_workers = (int)variable_value; break;
                    case KEY_NUMBER_OF_USERS: configuration.quantities.number_of_initial_users = (int)variable_value; break;
                    case KEY_NUMBER_OF_NEW_USERS_BATCH: configuration.quantities.number_of_new_users_batch = (int)variable_value; break;
                    case KEY_NUMBER_OF_PAUSE: configuration.quantities.number_of_allowed_breaks = (int)variable_value; break;
                    case KEY_MAXIMUM_USERS_PER_GROUP: configuration.quantities.maximum_users_per_group = (int)variable_value; break;
                    
                    case KEY_SEATS_PRIMI: configuration.seats.seats_first_course = (int)variable_value; break;
                    case KEY_SEATS_SECONDI: configuration.seats.seats_second_course = (int)variable_value; break;
                    case KEY_SEATS_COFFEE: configuration.seats.seats_coffee_dessert = (int)variable_value; break;
                    case KEY_SEATS_CASSA: configuration.seats.seats_cash_desk = (int)variable_value; break;
                    case KEY_TOTAL_DINING_SEATS: configuration.seats.total_dining_seats = (int)variable_value; break;
                    
                    case KEY_PRICE_PRIMI: configuration.prices.price_first_course = (double)variable_value; break;
                    case KEY_PRICE_SECONDI: configuration.prices.price_second_course = (double)variable_value; break;
                    case KEY_PRICE_COFFEE: configuration.prices.price_coffee_dessert = (double)variable_value; break;
                    
                    case KEY_SIMULATION_DURATION: configuration.timings.simulation_duration_days = (int)variable_value; break;
                    case KEY_MEAL_DURATION: configuration.timings.meal_duration_minutes = (int)variable_value; break;
                    case KEY_NANOSECONDS_PER_TICK: configuration.timings.nanoseconds_per_tick = variable_value; break;
                    case KEY_AVERAGE_SERVICE_PRIMI: configuration.timings.average_service_time_primi = (int)variable_value; break;
                    case KEY_AVERAGE_SERVICE_SECONDI: configuration.timings.average_service_time_secondi = (int)variable_value; break;
                    case KEY_AVERAGE_SERVICE_COFFEE: configuration.timings.average_service_time_coffee = (int)variable_value; break;
                    case KEY_AVERAGE_SERVICE_CASSA: configuration.timings.average_service_time_cassa = (int)variable_value; break;
                    case KEY_AVERAGE_SERVICE_TICKET: configuration.timings.average_service_time_ticket = (int)variable_value; break;
                    case KEY_AVERAGE_REFILL_TIME: configuration.timings.average_refill_time = (int)variable_value; break;
                    case KEY_STOP_DURATION: configuration.timings.stop_duration_minutes = (int)variable_value; break;
                    
                    case KEY_OVERLOAD_THRESHOLD: configuration.thresholds.overload_threshold = (int)variable_value; break;
                    case KEY_MAXIMUM_PORTIONS_PRIMI: configuration.thresholds.maximum_portions_primi = (int)variable_value; break;
                    case KEY_MAXIMUM_PORTIONS_SECONDI: configuration.thresholds.maximum_portions_secondi = (int)variable_value; break;
                    case KEY_REFILL_AMOUNT_PRIMI: configuration.thresholds.refill_amount_primi = (int)variable_value; break;
                    case KEY_REFILL_AMOUNT_SECONDI: configuration.thresholds.refill_amount_secondi = (int)variable_value; break;
                    case KEY_QUEUE_PATIENCE_THRESHOLD: configuration.thresholds.queue_patience_threshold = (int)variable_value; break;
                    
                    default: break;
                }
            }
        }
    }

    fclose(config_file);
    printf("[CONFIG] Parametri caricati correttamente.\n");
    return configuration;
}