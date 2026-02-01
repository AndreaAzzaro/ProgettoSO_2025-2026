/**
 * @file communication_disorder.c
 * @brief Utility esterna per bloccare temporaneamente le casse (Communication Disorder).
 * 
 * Protocollo:
 * 1. Connessione alla SHM.
 * 2. Attivazione semaforo di stop (STATION_SEM_STOP_GATE) sulla cassa.
 * 3. Sleep per durata configurata.
 * 4. Rimozione semaforo di stop.
 * 
 * @see communication_disorder.h per i prototipi.
 */

/* Includes di sistema */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

/* Includes del progetto */
#include "common.h"
#include "shm.h"
#include "sem.h"
#include "communication_disorder.h"

/* ==========================================================================
 *                             SEZIONE: MAIN
 * ========================================================================== */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("[DISORDER] Communication Disorder in avvio...\n");

    /* 1. Connessione alla simulazione */
    MainSharedMemory *shm;
    int shmid;
    if (connect_to_simulation(&shm, &shmid) != 0) {
        return EXIT_FAILURE;
    }

    /* 2. Lettura configurazione */
    int stop_duration = shm->configuration.timings.stop_duration_minutes;
    printf("[DISORDER] Durata blocco casse: %d secondi.\n", stop_duration);

    /* 3. Esecuzione Disorder */
    trigger_disorder(shm, stop_duration);

    /* 4. Cleanup locale (detach) */
    detach_shared_memory_segment(shm);
    
    return EXIT_SUCCESS;
}

/* ==========================================================================
 *                    SEZIONE: IMPLEMENTAZIONE FUNZIONI
 * ========================================================================== */

int connect_to_simulation(MainSharedMemory **shm_out, int *shmid_out) {
    key_t key = ftok(IPC_KEY_PATH, IPC_PROJECT_ID);
    if (key == -1) {
        perror("[ERROR] ftok fallita. Assicurati che config/config.conf esista");
        return -1;
    }

    int shmid = shmget(key, 0, 0);
    if (shmid == -1) {
        fprintf(stderr, "[ERROR] Impossibile trovare la memoria condivisa.\n");
        fprintf(stderr, "La simulazione è stata avviata?\n");
        return -1;
    }

    *shm_out = attach_to_simulation_shared_memory(shmid);
    *shmid_out = shmid;
    
    return 0;
}

void trigger_disorder(MainSharedMemory *shm, int duration_seconds) {
    /* 1. Blocco casse: V() sul semaforo stop */
    printf("[DISORDER] ATTIVAZIONE BLOCCO CASSE...\n");
    if (release_sem(shm->register_station.semaphore_set_id, STATION_SEM_STOP_GATE) == -1) {
        perror("[ERROR] Impossibile attivare blocco casse");
        return;
    }
    
    printf("[DISORDER] Casse BLOCCATE. Attesa di %d secondi...\n", duration_seconds);

    /* 2. Attesa (simulazione durata guasto) */
    sleep(duration_seconds);

    /* 3. Ripristino casse: P() sul semaforo stop */
    printf("[DISORDER] RIPRISTINO CASSE...\n");
    if (reserve_sem(shm->register_station.semaphore_set_id, STATION_SEM_STOP_GATE) == -1) {
        perror("[ERROR] Impossibile rimuovere blocco casse");
    } else {
        printf("[DISORDER] Casse RIPRISTINATE. Operatività normale.\n");
    }
}
