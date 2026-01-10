#include "../../include/conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_PATH "config/config.conf"
#define MAX_LINE_LEN 256

/**
 * @brief Funzione helper per rimuovere newline e spazi (opzionale, ma utile)
 * Qui ci fidiamo di sscanf che salta gli spazi bianchi automaticamente.
 */

SimConfig loadConfig() {
    SimConfig cfg;
    // Inizializziamo tutto a 0 per sicurezza
    memset(&cfg, 0, sizeof(SimConfig));

    FILE *file = fopen(CONFIG_PATH, "r");
    if (file == NULL) {
        perror("ERRORE CRITICO: Impossibile aprire config/config.conf");
        fprintf(stderr, "Assicurati di lanciare il programma dalla root del progetto!\n");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LEN];
    char key[100];
    int int_val;
    long long_val; // Per n_nano_secs

    // Leggiamo riga per riga
    while (fgets(line, sizeof(line), file)) {
        
        // 1. Salta commenti (#) e righe vuote
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // 2. Parsing: Cerchiamo pattern "CHIAVE = VALORE"
        // Cerchiamo prima come long (per n_nano_secs)
        if (sscanf(line, "%s = %ld", key, &long_val) == 2) {
            // Nota: usiamo long_val anche per gli int, poi castiamo. 
            // È sicuro se i numeri rientrano nell'int.

            
            // QUANTITIES
            if (strcmp(key, "N_WORKERS") == 0)        cfg.quantities.n_workers = (int)long_val;
            else if (strcmp(key, "N_USERS") == 0)     cfg.quantities.n_users = (int)long_val;
            else if (strcmp(key, "N_NEW_USERS") == 0) cfg.quantities.n_new_users = (int)long_val;
            else if (strcmp(key, "N_PAUSE") == 0)     cfg.quantities.n_pause = (int)long_val;

            // SEATS
            else if (strcmp(key, "SEATS_PRIMI") == 0)   cfg.seat.primi = (int)long_val;
            else if (strcmp(key, "SEATS_SECONDI") == 0) cfg.seat.secondi = (int)long_val;
            else if (strcmp(key, "SEATS_COFFEE") == 0)  cfg.seat.coffee = (int)long_val;
            else if (strcmp(key, "SEATS_CASSA") == 0)   cfg.seat.cassa = (int)long_val;
            else if (strcmp(key, "TOTAL_SEATS") == 0)   cfg.seat.seats = (int)long_val;

            // PRICES
            else if (strcmp(key, "PRICE_PRIMI") == 0)   cfg.price.primi = (int)long_val;
            else if (strcmp(key, "PRICE_SECONDI") == 0) cfg.price.secondi = (int)long_val;
            else if (strcmp(key, "PRICE_COFFEE") == 0)  cfg.price.coffee = (int)long_val;

            // TIMINGS
            else if (strcmp(key, "SIM_DURATION") == 0)       cfg.timing.sim_duration = (int)long_val;
            else if (strcmp(key, "N_NANO_SECS") == 0)        cfg.timing.n_nano_secs = long_val; // Qui niente cast!
            else if (strcmp(key, "AVG_SERVICE_PRIMI") == 0)  cfg.timing.avg_service_primi = (int)long_val;
            else if (strcmp(key, "AVG_SERVICE_MAIN") == 0)   cfg.timing.avg_service_main = (int)long_val;
            else if (strcmp(key, "AVG_SERVICE_COFFEE") == 0) cfg.timing.avg_service_coffee = (int)long_val;
            else if (strcmp(key, "AVG_SERVICE_CASSA") == 0)  cfg.timing.avg_service_cassa = (int)long_val;
            else if (strcmp(key, "AVG_REFILL_TIME") == 0)    cfg.timing.avg_refill_time = (int)long_val;
            else if (strcmp(key, "STOP_DURATION") == 0)      cfg.timing.stop_duration = (int)long_val;

            // THRESHOLD
            else if (strcmp(key, "OVERLOAD_THRESHOLD") == 0) cfg.threshold.overload_threshold = (int)long_val;

            else {
                // Opzionale: avvisa se c'è una chiave sconosciuta
                printf("Warning: Chiave sconosciuta nel config: %s\n", key);
            }
        }
    }

    fclose(file);
    printf("Configurazione caricata con successo da %s\n", CONFIG_PATH);
    return cfg;
}