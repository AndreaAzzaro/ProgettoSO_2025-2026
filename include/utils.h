/**
 * @file utils.h
 * @brief Funzioni di utilità generale per la simulazione.
 * 
 * Questo modulo fornisce:
 * - Gestione errori critici
 * - Generazione numeri casuali
 * - Simulazione del passaggio del tempo
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

/* ==========================================================================
 *                         SEZIONE: GESTIONE ERRORI
 * ========================================================================== */

/**
 * @brief Gestisce un errore critico stampando un messaggio e terminando il processo.
 * 
 * @param error_message Messaggio descrittivo dell'errore (solitamente passato a perror).
 */
void display_critical_error(const char *error_message);

/* ==========================================================================
 *                       SEZIONE: GENERAZIONE CASUALE
 * ========================================================================== */

/**
 * @brief Genera un numero intero casuale all'interno di un intervallo.
 * 
 * @param minimum_value Valore minimo (incluso).
 * @param maximum_value Valore massimo (incluso).
 * @return int Il numero casuale generato nell'intervallo [min, max].
 */
int generate_random_integer(int minimum_value, int maximum_value);

/**
 * @brief Calcola un valore casuale basato su un valore medio e una variazione percentuale.
 * 
 * Implementa i requisiti della consegna (es. AVG ± 50%).
 * 
 * @param average_value Il valore medio di riferimento.
 * @param variation_percentage La percentuale di scostamento (es. 50 per scostamento 50%).
 * @return int Il valore calcolato casualmente nell'intorno specificato.
 */
int calculate_varied_time(int average_value, int variation_percentage);

/**
 * @brief Verifica il verificarsi di un evento basandosi su una probabilità percentuale.
 * 
 * @param success_percentage_rate Percentuale di probabilità di successo (0-100).
 * @return true Se l'evento è stato estratto con successo.
 * @return false Se l'evento non si è verificato.
 */
bool evaluate_probability_event(int success_percentage_rate);

/* ==========================================================================
 *                        SEZIONE: GESTIONE TEMPO
 * ========================================================================== */

/**
 * @brief Simula il trascorrere del tempo in base ai parametri della simulazione.
 * 
 * Trasforma i minuti/unità della simulazione in nanosecondi reali utilizzando nanosleep.
 * Gestisce automaticamente le interruzioni dovute ai segnali (EINTR).
 * 
 * @param units_to_wait Quantità di unità temporali da attendere.
 * @param nanoseconds_per_tick Fattore di conversione (NNANOSECS).
 */
void simulate_time_passage(int units_to_wait, long nanoseconds_per_tick);

#endif /* UTILS_H */
