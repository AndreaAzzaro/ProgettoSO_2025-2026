#ifndef 


typedef struct {
    int first_course;
    int second_course;
    int coffee_dessert;  
    int total;          
} DailyPlateCounts;

void updateStats();
//manda l'operatore in pausa, doppio while quello piu esterno fa sem wait e sem op, quello piu interno simula il lavoro

//! booleano working, condizione del ciclo di lavoro

void pausa();
//controlla se il piatto da preparare sia disponibile
bool checkPlate(SharedMemory *shm, int idPiatto);

void servedPlate(mqd_t qd, OrderData *msg);

double calculateTotalPrice(mqd_t qd, OrderData *msg);

void disorderSignal(Signal *a);