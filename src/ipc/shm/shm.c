#include "../../include/shm.h"
#include <sys/mman.h>  // Per shm_open, mmap, munmap, shm_unlink
#include <sys/stat.h>  // Per le costanti dei permessi (es. S_IRUSR)
#include <fcntl.h>     // Per le costanti O_CREAT, O_RDWR
#include <unistd.h>    // Per ftruncate, close
#include <stdio.h>     // Per perror
#include <stdlib.h>    // Per exit

void* alloc_shared_memory(const char *name, size_t size) {

    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    
    if (fd == -1) {
        perror("ERRORE FATALE: shm_open fallita");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(fd, size) == -1) {
        perror("ERRORE FATALE: ftruncate fallita");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    void *ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    if (ptr == MAP_FAILED) {
        perror("ERRORE FATALE: mmap fallita");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    return ptr; 
}

void free_shared_memory(void *ptr, size_t size, const char *name, int unlink) {
    if (munmap(ptr, size) == -1) {
        perror("Warning: munmap fallita");
    }
    if (unlink) {
        if (shm_unlink(name) == -1) {
            perror("Warning: shm_unlink fallita");
        } else {
            printf("Memoria condivisa %s rimossa correttamente.\n", name);
            
        }
    }
}