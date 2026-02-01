/**
 * @file statistics.c
 * @brief Implementazione della raccolta e visualizzazione statistiche.
 * 
 * Fornisce funzioni per:
 * - Raccolta thread-safe delle statistiche dalla SHM
 * - Calcolo medie tempi di attesa
 * - Report a terminale e salvataggio su file
 * 
 * @see statistics.h per la documentazione delle strutture.
 */

/* Includes di sistema */
#include <stdio.h>
#include <string.h>

/* Includes del progetto */
#include "statistics.h"
#include "common.h"
#include "sem.h"

/* ==========================================================================
 *                         SEZIONE: RACCOLTA DATI
 * ========================================================================== */

/**
 * Raccoglie le statistiche dalla SHM con accesso protetto da mutex.
 * Calcola le medie dai dati accumulatori prima di restituire.
 */
SimulationStatistics collect_simulation_statistics(struct MainSharedMemory *shared_memory_ptr) {
    SimulationStatistics stats;
    int num_days = shared_memory_ptr->current_simulation_day + 1;

    /* 1. Protocollo di Accesso Sicuro (Sez 5.1 Consegna) */
    reserve_sem(shared_memory_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    memcpy(&stats, &shared_memory_ptr->statistics, sizeof(SimulationStatistics));
    release_sem(shared_memory_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);

    /* 2. Calcolo Medie Giornaliere (Utenti) */
    stats.clients_statistics.average_daily_clients_served = (double)stats.clients_statistics.total_clients_served / num_days;
    stats.clients_statistics.average_daily_clients_not_served = (double)stats.clients_statistics.total_clients_not_served / num_days;

    /* 3. Calcolo Medie Giornaliere (Piatti) */
    stats.average_daily_served_plates.average_daily_first_courses = (double)stats.total_served_plates.first_course_count / num_days;
    stats.average_daily_served_plates.average_daily_second_courses = (double)stats.total_served_plates.second_course_count / num_days;
    stats.average_daily_served_plates.average_daily_coffee_dessert = (double)stats.total_served_plates.coffee_dessert_count / num_days;
    stats.average_daily_served_plates.average_daily_total = (double)stats.total_served_plates.total_plates_count / num_days;

    stats.average_daily_leftover_plates.average_daily_first_courses = (double)stats.total_leftover_plates.first_course_count / num_days;
    stats.average_daily_leftover_plates.average_daily_second_courses = (double)stats.total_leftover_plates.second_course_count / num_days;
    stats.average_daily_leftover_plates.average_daily_coffee_dessert = (double)stats.total_leftover_plates.coffee_dessert_count / num_days;
    stats.average_daily_leftover_plates.average_daily_total = (double)stats.total_leftover_plates.total_plates_count / num_days;

    /* 4. Calcolo Medie Tempi (Giornalieri) */
    if (stats.daily_wait_accumulators.count_first > 0)
        stats.daily_average_wait_times.average_wait_first_course = stats.daily_wait_accumulators.sum_wait_first / stats.daily_wait_accumulators.count_first;
    if (stats.daily_wait_accumulators.count_second > 0)
        stats.daily_average_wait_times.average_wait_second_course = stats.daily_wait_accumulators.sum_wait_second / stats.daily_wait_accumulators.count_second;
    if (stats.daily_wait_accumulators.count_cashier > 0)
        stats.daily_average_wait_times.average_wait_cash_desk = stats.daily_wait_accumulators.sum_wait_cashier / stats.daily_wait_accumulators.count_cashier;
    if (stats.daily_wait_accumulators.count_coffee > 0)
        stats.daily_average_wait_times.average_wait_coffee_dessert = stats.daily_wait_accumulators.sum_wait_coffee / stats.daily_wait_accumulators.count_coffee;

    /* 5. Calcolo Medie Tempi (Globali) */
    if (stats.total_wait_accumulators.count_first > 0)
        stats.total_average_wait_times.average_wait_first_course = stats.total_wait_accumulators.sum_wait_first / stats.total_wait_accumulators.count_first;
    if (stats.total_wait_accumulators.count_second > 0)
        stats.total_average_wait_times.average_wait_second_course = stats.total_wait_accumulators.sum_wait_second / stats.total_wait_accumulators.count_second;
    if (stats.total_wait_accumulators.count_cashier > 0)
        stats.total_average_wait_times.average_wait_cash_desk = stats.total_wait_accumulators.sum_wait_cashier / stats.total_wait_accumulators.count_cashier;
    if (stats.total_wait_accumulators.count_coffee > 0)
        stats.total_average_wait_times.average_wait_coffee_dessert = stats.total_wait_accumulators.sum_wait_coffee / stats.total_wait_accumulators.count_coffee;

    /* 6. Calcolo Medie (Incassi e Pause) */
    stats.income_statistics.average_daily_income = stats.income_statistics.accumulated_total_income / num_days;
    stats.operators_statistics.average_daily_breaks = (double)stats.operators_statistics.total_breaks_taken / num_days;

    return stats;
}

/* ==========================================================================
 *                       SEZIONE: OUTPUT E REPORT
 * ========================================================================== */

/**
 * Stampa a terminale il report giornaliero formattato.
 */
void display_daily_statistics_report(SimulationStatistics s, int simulation_day) {
    printf("\n======================================================================\n");
    printf("        REPORT SIMULAZIONE - GIORNO %d (Fine Giornata)\n", simulation_day + 1);
    printf("======================================================================\n");

    printf("[UTENTI]\n");
    printf("  Oggi:   Serviti: %d (TK: %d, No-TK: %d) | Rinunciatari: %d\n", 
           s.clients_statistics.daily_clients_served, s.clients_statistics.daily_clients_with_ticket, 
           s.clients_statistics.daily_clients_without_ticket, s.clients_statistics.daily_clients_not_served);
    printf("  Totali: Serviti: %d | Rinunciatari: %d\n", 
           s.clients_statistics.total_clients_served, s.clients_statistics.total_clients_not_served);
    printf("  Media:  Serviti/gg: %.2f | Rinunciatari/gg: %.2f\n", 
           s.clients_statistics.average_daily_clients_served, s.clients_statistics.average_daily_clients_not_served);

    printf("\n[PIATTI DISTRIBUITI]\n");
    printf("  Oggi:   Primi: %d | Secondi: %d | Caffè/Dolci: %d\n", 
           s.daily_served_plates.first_course_count, s.daily_served_plates.second_course_count, s.daily_served_plates.coffee_dessert_count);
    printf("  Totali: Primi: %d | Secondi: %d | Caffè/Dolci: %d\n", 
           s.total_served_plates.first_course_count, s.total_served_plates.second_course_count, s.total_served_plates.coffee_dessert_count);
    printf("  Media:  Primi/gg: %.2f | Secondi/gg: %.2f | Dolci/gg: %.2f\n", 
           s.average_daily_served_plates.average_daily_first_courses, s.average_daily_served_plates.average_daily_second_courses, 
           s.average_daily_served_plates.average_daily_coffee_dessert);

    printf("\n[PIATTI AVANZATI (WASTE)]\n");
    printf("  Oggi:   Primi: %d | Secondi: %d\n", s.daily_leftover_plates.first_course_count, s.daily_leftover_plates.second_course_count);
    printf("  Media:  Primi/gg: %.2f | Secondi/gg: %.2f\n", 
           s.average_daily_leftover_plates.average_daily_first_courses, s.average_daily_leftover_plates.average_daily_second_courses);

    printf("\n[TEMPI MEDI DI ATTESA (Minuti)]\n");
    printf("  Stazione:  |  Oggi   | Totale  \n");
    printf("  -----------|---------|---------\n");
    printf("  Primi:     | %7.2f | %7.2f\n", s.daily_average_wait_times.average_wait_first_course, s.total_average_wait_times.average_wait_first_course);
    printf("  Secondi:   | %7.2f | %7.2f\n", s.daily_average_wait_times.average_wait_second_course, s.total_average_wait_times.average_wait_second_course);
    printf("  Cassa:     | %7.2f | %7.2f\n", s.daily_average_wait_times.average_wait_cash_desk, s.total_average_wait_times.average_wait_cash_desk);
    printf("  Caffè/D:   | %7.2f | %7.2f\n", s.daily_average_wait_times.average_wait_coffee_dessert, s.total_average_wait_times.average_wait_coffee_dessert);

    printf("\n[OPERATORI E INCASSI]\n");
    printf("  Operatori: Attivi oggi: %d | Attivi Tot: %d | Pause: %d (Media/gg: %.2f)\n", 
           s.operators_statistics.daily_active_operators, s.operators_statistics.total_active_operators_all_time, 
           s.operators_statistics.total_breaks_taken, s.operators_statistics.average_daily_breaks);
    printf("  Incassi:   Oggi: %.2f EUR | Totale: %.2f EUR | Media/gg: %.2f EUR\n", 
           s.income_statistics.current_daily_income, s.income_statistics.accumulated_total_income, s.income_statistics.average_daily_income);

    printf("======================================================================\n\n");
}

void display_final_simulation_report(SimulationStatistics s, int total_days) {
    const char *reasons[] = {
        "NON SPECIFICATA", 
        "TIMEOUT (DURATA MASSIMA RAGGIUNTA)", 
        "OVERLOAD (TROPPI UTENTI NON SERVITI)", 
        "SEGNALE ESTERNO (SIGINT/SIGTERM)"
    };

    printf("\n\n");
    printf("######################################################################\n");
    printf("      REPORT FINALE COMPLESSIVO DELLA SIMULAZIONE (%d GIORNI)\n", total_days);
    printf("######################################################################\n\n");

    printf("--- CAUSA DI TERMINAZIONE: %s ---\n\n", (s.reason_for_termination < 4) ? reasons[s.reason_for_termination] : "SCONOSCIUTA");

    printf("[UTENTI TOTALI]\n");
    printf("  Serviti:     %d (Media: %.2f/gg)\n", s.clients_statistics.total_clients_served, s.clients_statistics.average_daily_clients_served);
    printf("  Rinunciatari: %d (Media: %.2f/gg)\n", s.clients_statistics.total_clients_not_served, s.clients_statistics.average_daily_clients_not_served);
    printf("  Con Ticket:   %d (%.1f%% del totale serviti)\n", 
           s.clients_statistics.total_clients_with_ticket, 
           (s.clients_statistics.total_clients_served > 0) ? (100.0 * s.clients_statistics.total_clients_with_ticket / s.clients_statistics.total_clients_served) : 0);

    printf("\n[CONSUMI E AVANZI PIATTI (TOTALI E MEDIE)]\n");
    printf("  TIPO               DISTRIBUITI (MEDIA/gg)    AVANZATI (MEDIA/gg)\n");
    printf("  ------------------------------------------------------------------\n");
    printf("  Primi Piatti:      %d (%.2f)               %d (%.2f)\n", 
           s.total_served_plates.first_course_count, s.average_daily_served_plates.average_daily_first_courses,
           s.total_leftover_plates.first_course_count, s.average_daily_leftover_plates.average_daily_first_courses);
    printf("  Secondi Piatti:    %d (%.2f)               %d (%.2f)\n", 
           s.total_served_plates.second_course_count, s.average_daily_served_plates.average_daily_second_courses,
           s.total_leftover_plates.second_course_count, s.average_daily_leftover_plates.average_daily_second_courses);
    printf("  Caffè e Dolci:     %d (%.2f)               %d (%.2f)\n", 
           s.total_served_plates.coffee_dessert_count, s.average_daily_served_plates.average_daily_coffee_dessert,
           s.total_leftover_plates.coffee_dessert_count, s.average_daily_leftover_plates.average_daily_coffee_dessert);
    printf("  ------------------------------------------------------------------\n");
    printf("  TOTALI:            %d (%.2f)               %d (%.2f)\n", 
           s.total_served_plates.total_plates_count, s.average_daily_served_plates.average_daily_total,
           s.total_leftover_plates.total_plates_count, s.average_daily_leftover_plates.average_daily_total);

    printf("\n[EFFICIENZA E TEMPI MEDI GLOBALI]\n");
    printf("  Attesa Primi:    %.2f min\n", s.total_average_wait_times.average_wait_first_course);
    printf("  Attesa Secondi:  %.2f min\n", s.total_average_wait_times.average_wait_second_course);
    printf("  Attesa Cassa:    %.2f min\n", s.total_average_wait_times.average_wait_cash_desk);
    printf("  Attesa Caffè:    %.2f min\n", s.total_average_wait_times.average_wait_coffee_dessert);

    printf("\n[ECONOMIA E PERSONALE]\n");
    printf("  Incasso Totale:  %.2f EUR (Media: %.2f EUR/gg)\n", 
           s.income_statistics.accumulated_total_income, s.income_statistics.average_daily_income);
    printf("  Operatori Attivi: %d (Totale simulazione)\n", s.operators_statistics.total_active_operators_all_time);
    printf("  Totale Pause:    %d (Media: %.2f/gg)\n", 
           s.operators_statistics.total_breaks_taken, s.operators_statistics.average_daily_breaks);

    printf("\n######################################################################\n");
    printf("                  FINE REPORT - PROGETTO SO 2026\n");
    printf("######################################################################\n\n");
}

/**
 * Salva le statistiche giornaliere su file.
 */
int save_statistics_to_csv(SimulationStatistics s, int simulation_day, const char *filepath) {
    FILE *check_file = fopen(filepath, "r");
    int file_exists = (check_file != NULL);
    if (check_file) fclose(check_file);

    FILE *file = fopen(filepath, "a");
    if (file == NULL) {
        perror("[STATISTICS] Errore apertura file statistiche");
        return -1;
    }

    if (!file_exists) {
        fprintf(file, "day,daily_srv,daily_not_srv,daily_tk,daily_notk,total_srv,total_not_srv,"
                      "daily_plate_1,daily_plate_2,daily_plate_c,total_plate_1,total_plate_2,total_plate_c,"
                      "waste_1,waste_2,avg_wait_1_day,avg_wait_1_tot,avg_wait_c_day,avg_wait_c_tot,"
                      "ops_active,ops_breaks_tot,income_day,income_tot\n");
    }

    fprintf(file, "%d,%d,%d,%d,%d,%d,%d,"
                  "%d,%d,%d,%d,%d,%d,"
                  "%d,%d,%.2f,%.2f,%.2f,%.2f,"
                  "%d,%d,%.2f,%.2f\n",
            simulation_day + 1,
            s.clients_statistics.daily_clients_served,
            s.clients_statistics.daily_clients_not_served,
            s.clients_statistics.daily_clients_with_ticket,
            s.clients_statistics.daily_clients_without_ticket,
            s.clients_statistics.total_clients_served,
            s.clients_statistics.total_clients_not_served,
            s.daily_served_plates.first_course_count,
            s.daily_served_plates.second_course_count,
            s.daily_served_plates.coffee_dessert_count,
            s.total_served_plates.first_course_count,
            s.total_served_plates.second_course_count,
            s.total_served_plates.coffee_dessert_count,
            s.daily_leftover_plates.first_course_count,
            s.daily_leftover_plates.second_course_count,
            s.daily_average_wait_times.average_wait_first_course,
            s.total_average_wait_times.average_wait_first_course,
            s.daily_average_wait_times.average_wait_cash_desk,
            s.total_average_wait_times.average_wait_cash_desk,
            s.operators_statistics.daily_active_operators,
            s.operators_statistics.total_breaks_taken,
            s.income_statistics.current_daily_income,
            s.income_statistics.accumulated_total_income);

    fclose(file);
    printf("[STATISTICS] Log CSV aggiornato per giorno %d.\n", simulation_day + 1);
    return 0;
}
