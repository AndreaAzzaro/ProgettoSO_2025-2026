#include "../../include/queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>     // O_CREAT, O_RDWR
#include <sys/stat.h>  // Permessi 0666
#include <errno.h>

mqd_t create_queue(const char *name, long max_msgs, long msg_size) {
    struct mq_attr attr;
    attr.mq_flags = 0;           // 0 = Bloccante
    attr.mq_maxmsg = max_msgs;   // Es. 10
    attr.mq_msgsize = msg_size;  // Es. sizeof(OrderData)
    attr.mq_curmsgs = 0;

    // Crea la coda con permessi di lettura/scrittura per tutti
    mqd_t qd = mq_open(name, O_CREAT | O_RDWR, 0666, &attr);

    if (qd == (mqd_t)-1) {
        perror("ERRORE FATALE: create_queue fallita");
        exit(EXIT_FAILURE);
    }
    return qd;
}

mqd_t open_queue(const char *name) {
    // Apre solo (senza O_CREAT)
    mqd_t qd = mq_open(name, O_RDWR);

    if (qd == (mqd_t)-1) {
        perror("ERRORE FATALE: open_queue fallita (la coda esiste?)");
        exit(EXIT_FAILURE);
    }
    return qd;
}

void send_message(mqd_t qd, OrderData *msg) {
    // Cast a (const char*) necessario per la firma di mq_send
    // Priorità 0 (default)
    if (mq_send(qd, (const char *)msg, sizeof(OrderData), 0) == -1) {
        perror("ERRORE: send_message fallita");
        exit(EXIT_FAILURE);
    }
}

ssize_t receive_message(mqd_t qd, OrderData *msg) {
    // Cast a (char*) per mq_receive
    ssize_t bytes_read = mq_receive(qd, (char *)msg, sizeof(OrderData), NULL);

    if (bytes_read == -1) {
        perror("ERRORE: receive_message fallita");
        exit(EXIT_FAILURE);
    }
    return bytes_read;
}

void close_and_unlink_queue(mqd_t qd, const char *name) {
    if (mq_close(qd) == -1) {
        perror("Warning: mq_close fallita");
    }
    // unlink va fatto solo una volta (di solito dal Responsabile)
    if (mq_unlink(name) == -1) {
        if (errno != ENOENT) { // Ignora errore se già rimossa
            perror("Warning: mq_unlink fallita");
        }
    }
}