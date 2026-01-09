#ifndef SHM_H
#define SHM_H

#include <sys/type.h> // per size_t

#define SHM_NAME "/mensa_shm_project"

void* alloc_shared_memory(const char *name, size_t size);

void free_shared_memory(void *ptr, size_t size, const char *name, int unlink);

#endif