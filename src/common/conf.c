#include "../../include/conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_PATH "config/config.conf"
#define MAX_LINE_LEN 256

// 1. DEFINIAMO UN ENUM PER IDENTIFICARE LE CHIAVI
// Questo trasforma le stringhe in numeri gestibili dallo switch
typedef enum {
    KEY_UNKNOWN = 0,
    
    // Quantities
    KEY_N_WORKERS, KEY_N_USERS, KEY_N_NEW_USERS, KEY_N_PAUSE,
    
    // Seats
    KEY_SEATS_PRIMI, KEY_SEATS_SECONDI, KEY_SEATS_COFFEE, KEY_SEATS_CASSA, KEY_TOTAL_SEATS,
    
    // Prices
    KEY_PRICE_PRIMI, KEY_PRICE_SECONDI, KEY_PRICE_COFFEE,
    
    // Timings
    KEY_SIM_DURATION, KEY_N_NANO_SECS, 
    KEY_AVG_SERVICE_PRIMI, KEY_AVG_SERVICE_MAIN, KEY_AVG_SERVICE_COFFEE, 
    KEY_AVG_SERVICE_CASSA, KEY_AVG_REFILL_TIME, KEY_STOP_DURATION,
    
    // Threshold
    KEY_OVERLOAD_THRESHOLD
} ConfigKey;

// 2. CREIAMO UNA TABELLA DI MAPPING (LOOKUP TABLE)
// Associa la stringa nel file al valore Enum corrispondente
typedef struct {
    const char *string_key;
    ConfigKey enum_key;
} KeyMap;

static const KeyMap mapping_table[] = {
    {"N_WORKERS", KEY_N_WORKERS},
    {"N_USERS", KEY_N_USERS},
    {"N_NEW_USERS", KEY_N_NEW_USERS},
    {"N_PAUSE", KEY_N_PAUSE},
    
    {"SEATS_PRIMI", KEY_SEATS_PRIMI},
    {"SEATS_SECONDI", KEY_SEATS_SECONDI},
    {"SEATS_COFFEE", KEY_SEATS_COFFEE},
    {"SEATS_CASSA", KEY_SEATS_CASSA},
    {"TOTAL_SEATS", KEY_TOTAL_SEATS},
    
    {"PRICE_PRIMI", KEY_PRICE_PRIMI},
    {"PRICE_SECONDI", KEY_PRICE_SECONDI},
    {"PRICE_COFFEE", KEY_PRICE_COFFEE},
    
    {"SIM_DURATION", KEY_SIM_DURATION},
    {"N_NANO_SECS", KEY_N_NANO_SECS},
    {"AVG_SERVICE_PRIMI", KEY_AVG_SERVICE_PRIMI},
    {"AVG_SERVICE_MAIN", KEY_AVG_SERVICE_MAIN},
    {"AVG_SERVICE_COFFEE", KEY_AVG_SERVICE_COFFEE},
    {"AVG_SERVICE_CASSA", KEY_AVG_SERVICE_CASSA},
    {"AVG_REFILL_TIME", KEY_AVG_REFILL_TIME},
    {"STOP_DURATION", KEY_STOP_DURATION},
    
    {"OVERLOAD_THRESHOLD", KEY_OVERLOAD_THRESHOLD},
    
    {NULL, KEY_UNKNOWN} // Sentinella di fine array
};

// 3. FUNZIONE DI RISOLUZIONE
// Converte stringa -> Enum scorrendo la tabella
ConfigKey resolve_key(const char *key) {
    for (int i = 0; mapping_table[i].string_key != NULL; i++) {
        if (strcmp(key, mapping_table[i].string_key) == 0) {
            return mapping_table[i].enum_key;
        }
    }
    return KEY_UNKNOWN;
}

// 4. IMPLEMENTAZIONE LOAD CONFIG CON SWITCH
SimConfig loadConfig() {
    SimConfig cfg;
    memset(&cfg, 0, sizeof(SimConfig));

    FILE *file = fopen(CONFIG_PATH, "r");
    if (file == NULL) {
        perror("ERRORE CRITICO: config/config.conf non trovato");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LEN];
    char key_str[100];
    long val; // Usiamo long per tutto, poi castiamo

    while (fgets(line, sizeof(line), file)) {
        // Salta commenti e righe vuote
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        if (sscanf(line, "%s = %ld", key_str, &val) == 2) {
            
            // --- QUI AVVIENE LA MAGIA ---
            switch (resolve_key(key_str)) {
                
                // Quantities
                case KEY_N_WORKERS:     cfg.quantities.n_workers = (int)val; break;
                case KEY_N_USERS:       cfg.quantities.n_users = (int)val; break;
                case KEY_N_NEW_USERS:   cfg.quantities.n_new_users = (int)val; break;
                case KEY_N_PAUSE:       cfg.quantities.n_pause = (int)val; break;
                
                // Seats
                case KEY_SEATS_PRIMI:   cfg.seat.primi = (int)val; break;
                case KEY_SEATS_SECONDI: cfg.seat.secondi = (int)val; break;
                case KEY_SEATS_COFFEE:  cfg.seat.coffee = (int)val; break;
                case KEY_SEATS_CASSA:   cfg.seat.cassa = (int)val; break;
                case KEY_TOTAL_SEATS:   cfg.seat.seats = (int)val; break;
                
                // Prices
                case KEY_PRICE_PRIMI:   cfg.price.primi = (int)val; break;
                case KEY_PRICE_SECONDI: cfg.price.secondi = (int)val; break;
                case KEY_PRICE_COFFEE:  cfg.price.coffee = (int)val; break;
                
                // Timings
                case KEY_SIM_DURATION:       cfg.timing.sim_duration = (int)val; break;
                case KEY_N_NANO_SECS:        cfg.timing.n_nano_secs = val; break; // No cast!
                case KEY_AVG_SERVICE_PRIMI:  cfg.timing.avg_service_primi = (int)val; break;
                case KEY_AVG_SERVICE_MAIN:   cfg.timing.avg_service_main = (int)val; break;
                case KEY_AVG_SERVICE_COFFEE: cfg.timing.avg_service_coffee = (int)val; break;
                case KEY_AVG_SERVICE_CASSA:  cfg.timing.avg_service_cassa = (int)val; break;
                case KEY_AVG_REFILL_TIME:    cfg.timing.avg_refill_time = (int)val; break;
                case KEY_STOP_DURATION:      cfg.timing.stop_duration = (int)val; break;
                
                // Threshold
                case KEY_OVERLOAD_THRESHOLD: cfg.threshold.overload_threshold = (int)val; break;

                case KEY_UNKNOWN:
                default:
                    printf("Warning: Chiave ignorata nel config: %s\n", key_str);
                    break;
            }
        }
    }

    fclose(file);
    printf("Configurazione caricata correttamente.\n");
    return cfg;
}