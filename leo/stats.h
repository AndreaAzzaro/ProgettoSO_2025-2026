#ifndef STATS_H
#define STATS_H

typedef struct {
    int first_course;
    int second_course;
    int coffee_dessert;  
    int total;          
} PlateCounts;

typedef struct {
    double first_course;
    double second_course;
    double coffee_dessert;
    double total;
} PlateAverages;

typedef struct {
    double first_course;
    double second_course;
    double coffee;
    double cassa;
    double global;      
} WaitTimes;

typedef struct {
    int total_served;
    int total_not_served;
    double avg_daily_served;     
    double avg_daily_not_served; 
} ClientStats;

typedef struct {
    int total_active_sim;    
    int daily_active;        
    int total_breaks;        
    double avg_daily_breaks; 
} OperatorStats;

typedef struct {
    double total_income;
    double daily_income;
    double avg_daily_income;
} IncomeStats;

typedef struct {
    PlateCounts total_served_plates;      
    PlateCounts total_leftover_plates;   
    PlateAverages avg_daily_served_plates;   
    PlateAverages avg_daily_leftover_plates; 
    WaitTimes total_avg_wait;  
    WaitTimes daily_avg_wait;  
    
    ClientStats clients;
    OperatorStats operators;
    IncomeStats income;
    
} SimulationStats;

void readStats(SharedMensa *shm);
void printStats(SharedMensa *shm);
#endif