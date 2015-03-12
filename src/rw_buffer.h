#ifndef _RW_BUFFER_H_
#define _RW_BUFFER_H_

#include <stddef.h>

typedef struct rw_buffer_decl{
    char *buff;
    size_t buff_size;
    size_t frount;
    size_t wanna;
    size_t done;
} rw_buffer_t;

void rw_buffer_init(rw_buffer_t *buffer);
void rw_buffer_fin(rw_buffer_t *buffer);

char *rw_buffer_frount(rw_buffer_t *buffer);
char *rw_buffer_wanna(rw_buffer_t *buffer);
char *rw_buffer_done(rw_buffer_t *buffer);
void rw_buffer_pop_frount(rw_buffer_t *buffer, size_t size);
void rw_buffer_size_upto(rw_buffer_t *buffer, size_t size);

#endif
