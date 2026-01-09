#ifndef SEM_H
#define SEM_H

#include <semaphore.h>
#include <sys/types.h>

void init_sem(sem_t *sem, int value);

void wait_sem(sem_t *sem);

void post_sem(sem_t *sem);

void destroy_sem(sem_t *sem);

#endif