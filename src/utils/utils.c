/**
 * @file utils.c
 * @brief Implementazione delle utility generali per la simulazione.
 * 
 * Fornisce:
 * - Gestione errori critici
 * - Generazione numeri casuali e probabilità
 * - Simulazione del passaggio del tempo
 * 
 * @see utils.h per la documentazione delle funzioni pubbliche.
 */

/* Includes di sistema */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

/* Includes del progetto */
#include "utils.h"

/* ==========================================================================
 *                         SEZIONE: GESTIONE ERRORI
 * ========================================================================== */

/** Termina il processo con messaggio di errore. */
void display_critical_error(const char *error_message) {
    perror(error_message);
    exit(EXIT_FAILURE);
}

/* ==========================================================================
 *                       SEZIONE: GENERAZIONE CASUALE
 * ========================================================================== */

/** Genera un intero casuale nell'intervallo [min, max]. */
int generate_random_integer(int minimum_value, int maximum_value) {
    int random_result = minimum_value;
    
    /* Protezione contro parametri invertiti o uguali */
    if (minimum_value < maximum_value) {
        random_result = minimum_value + rand() % (maximum_value - minimum_value + 1);
    }
    
    return random_result;
}

/**
 * Calcola un valore variato casualmente attorno alla media.
 * Implementa il requisito AVG ± variation% della consegna.
 */
int calculate_varied_time(int average_value, int variation_percentage) {
    /* Calcoliamo il delta basato sul valore medio e la percentuale */
    double delta = (average_value * variation_percentage) / 100.0;
    
    int minimum = (int)(average_value - delta);
    int maximum = (int)(average_value + delta);
    
    /* Garantiamo che il minimo non scenda mai sotto zero per tempi fisici */
    if (minimum < 0) minimum = 0;
    
    return generate_random_integer(minimum, maximum);
}

/** Valuta se un evento casuale si verifica data una probabilità %. */
bool evaluate_probability_event(int success_percentage_rate) {
    bool event_occurred = false;
    
    /* Gestione dei casi limite fuori range */
    if (success_percentage_rate >= 100) {
        event_occurred = true;
    } else if (success_percentage_rate > 0) {
        event_occurred = (rand() % 100) < success_percentage_rate;
    }
    
    return event_occurred;
}

/* ==========================================================================
 *                        SEZIONE: GESTIONE TEMPO
 * ========================================================================== */

/**
 * Simula il passaggio del tempo convertendo unità simulate in nanosleep.
 * Gestisce automaticamente le interruzioni da segnale (EINTR).
 */
void simulate_time_passage(int units_to_wait, long nanoseconds_per_tick) {
    if (units_to_wait <= 0) return;

    struct timespec requested_time;
    long total_nanoseconds = (long)units_to_wait * nanoseconds_per_tick;

    /* Conversione in secondi e nanosecondi per struct timespec */
    requested_time.tv_sec = total_nanoseconds / 1000000000L;
    requested_time.tv_nsec = total_nanoseconds % 1000000000L;
    
    /* Loop robusto per nanosleep: riprende se interrotto da EINTR */
    while (nanosleep(&requested_time, &requested_time) == -1) {
        if (errno != EINTR) {
            /* Errore critico non dovuto a segnali */
            break; 
        }
    }
}
