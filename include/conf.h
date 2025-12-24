#ifndef CONF_H
#define CONF_H

typedef struct {
    int n_workers;
    int n_users;
    int n_new_users;
    int n_pause;
}ConfigQuantities;

typedef struct{
    int primi;
    int secondi;
    int coffee;
    int cassa;
    int seats;
}ConfigSeats;

typedef struct{
    int primi;
    int secondi;
    int coffee;
}ConfigPrice;

typedef struct{
    int sim_duration;         
    long n_nano_secs;         // fattore con cui moltiplicheremo sim_duration
    int avg_service_primi;
    int avg_service_main;
    int avg_service_coffee;
    int avg_service_cassa;
    int avg_refill_time;      
    int stop_duration;        
} ConfigTimings;

typedef struct{
    int overload_threshold;
}ConfigThreshold;

typedef struct{
    ConfigQuantities quantities;
    ConfigSeats seat;
    ConfigPrice price;
    ConfigThreshold threshold;
    ConfigTimings timing;
}SimConfig;

#endif