#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <semaphore.h>
#include <pthread.h>

#include "config.h"
#include "stats.h"
#include "menu.h"

#define SHM_NAME "/progetto_mensa_shm"

typedef enum{
    GRP_CASSIERI,
    GRP_CUCINA,
    GRP_BAR,
    GRP_UTENTI,
    MAX_GROUPS,
}ProcessGroupIndex;

typedef struct{
    sem_t sem_colonna_utenti; // blocca gli utenti se la fila è lunga
    sem_t sem_operatori_liberi;// conta quanti operatori stanno lavorando (nof_worker - nof_seats)
    sem_t sem_servizio_finito;// avviso di fine lavoro

    int porzioni_rimanenti;
    pthread_mutex_t mutex_dati;
}StazioneDistribuzione;

typedef struct{
    sem_t sem_colonna_utenti; 
    sem_t sem_operatori_liberi;
    sem_t sem_servizio_finito;

    double incasso_totale;
    pthread_mutex_t mutex_dati;
}StazioneCassa;

typedef struct{
    sem_t sem_tavoli_liberi;
}StazioneRefezione;

typedef struct{
    Config config;
    SimulazioneStats stats;
    MensaMenu menu;

    pthread_mutex_t mutex_stats;

    //Gestione SimStart con mutex
    int simulation_started;
    pthread_mutex_t mutex_start;
    pthread_cond_t cond_start;
    sem_t sem_process_ready;

    pid_t gruppi_processi[MAX_GROUPS];

    StazioneDistribuzione first_course_station;
    StazioneDistribuzione second_course_station;
    StazioneDistribuzione coffee_dessert_station;

    StazioneCassa register_station;
    StazioneRefezione seat_area;
}SharedMensa;