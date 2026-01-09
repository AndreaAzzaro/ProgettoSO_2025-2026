#ifndef QUEUE_H
#define QUEUE_H

#include <mqueue.h>
#include <sys/type.h>

typedef struct {
    int id_utente;
    int azione;      // Es: 1=Paga, 2=Reclamo
    double importo;
} MsgData;

mqd_t create_queue(const char *name, long max_msgs, long msg_size);

mqd_t open_queue(const char *name);

void send_message(mqd_t qd, MsgData *msg);

ssize_t receive_message(mqd_t qd, MsgData *msg);

void close_and_unlink_queue(mqd_t qd, const char *name);

#endif