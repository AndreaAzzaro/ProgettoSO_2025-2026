/**
 * @file communication_disorder.h
 * @brief Header per l'utility communication_disorder.
 * 
 * Modulo standalone che simula un guasto temporaneo alle casse (Communication Disorder),
 * bloccando i pagamenti per un intervallo di tempo definito.
 * 
 * @see communication_disorder.c per l'implementazione.
 */

#ifndef COMMUNICATION_DISORDER_H
#define COMMUNICATION_DISORDER_H

#include "common.h"
#include <sys/types.h>

/* ==========================================================================
 *                       SEZIONE: PROTOTIPI FUNZIONI
 * ========================================================================== */

/**
 * @brief Connette alla memoria condivisa della simulazione tramite ftok.
 * @return 0 successo, -1 errore.
 */
int connect_to_simulation(MainSharedMemory **shm_out, int *shmid_out);

/**
 * @brief Esegue il ciclo di blocco e sblocco delle casse.
 * @param duration_seconds Durata del blocco in secondi reali.
 */
void trigger_disorder(MainSharedMemory *shm, int duration_seconds);

#endif /* COMMUNICATION_DISORDER_H */
