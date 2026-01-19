#ifndef 


typedef struct{

}OperatorStats;

void updateStats();

void pausa();

bool checkPlate(SharedMemory *shm, int idPiatto);

void servedPlate(mqd_t qd, OrderData *msg);

void calculateTotalPrice(SharedMemory shm);