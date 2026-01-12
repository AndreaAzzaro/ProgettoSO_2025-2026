#ifndef RESPONSABILE_MENSA_H
#ifndef RESPONSABILE_MENSA_H
/*
stazione.h


int array operators_id []

int AVG_REFILL

int REFILL_TIMER

int MAX_PLATES

int AVG_SRVC

string STATION_NAME

sem_t stationSem

int service_time

*/

// Dimensione sovrastimata per sicurezza (il minimo richiesto è circa 10)
#define MAX_PIATTI 8  

// Lunghezza massima del nome di un piatto
#define LEN_NOME_PIATTO 50

typedef enum {
    PRIMO,
    SECONDO,
    CONTORNO,
    DOLCE_CAFFE
} TipoPiatto;

typedef struct {
    char nome[LEN_NOME_PIATTO];
    TipoPiatto tipo;
    int quantita_disponibile; 
    int prezzo;
} Piatto;

typedef struct {
    Piatto piatti[MAX_PIATTI]; // Qui usi la costante
    int numero_piatti_totali;
} MenuGiornaliero;
/*
@brief: apre la memoria condivisa
@param un cazzo
pre_condizione un cazzo
post condizione mi ritorna il puntatore alla shared memory
*/
SharedMensa openSHM();//director
/*
@brief: inizializza le variabile della shm
@param puntatore alla shm
pre_condizione shm != null
post condizione deve aver inizializzato la shared memory
*/
void initStruct(SharedMensa shm);//director 
void updateStats(SharedMensa shm);//director
void createOperator(int max_operator);//director mio

int* divOperator(SharedMensa shm);
#endif
