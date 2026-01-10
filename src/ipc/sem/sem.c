#include "../../include/sem.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// --- INIZIALIZZAZIONE ---
void init_sem(sem_t *sem, int value) {
    // sem_init(sem, pshared, value)
    // pshared = 1: Il semaforo è condiviso tra processi (FONDAMENTALE per SHM)
    // pshared = 0: Il semaforo è condiviso solo tra thread dello stesso processo
    if (sem_init(sem, 1, value) == -1) {
        perror("ERRORE FATALE: init_sem fallita");
        exit(EXIT_FAILURE);
    }
}

// --- WAIT (Decrementa/Blocca) ---
void wait_sem(sem_t *sem) {
    // sem_wait decrementa il semaforo. Se è 0, si blocca.
    if (sem_wait(sem) == -1) {
        perror("ERRORE FATALE: wait_sem fallita");
        exit(EXIT_FAILURE);
    }
}

// --- POST (Incrementa/Sveglia) ---
void post_sem(sem_t *sem) {
    // sem_post incrementa il semaforo. Se c'è qualcuno in attesa, lo sveglia.
    if (sem_post(sem) == -1) {
        perror("ERRORE FATALE: post_sem fallita");
        exit(EXIT_FAILURE);
    }
}

// --- DESTROY (Pulizia) ---
void destroy_sem(sem_t *sem) {
    // Rilascia le risorse. Non fa exit in caso di errore perché
    // siamo già in fase di chiusura, basta un warning.
    if (sem_destroy(sem) == -1) {
        perror("Warning: destroy_sem fallita");
    }
}