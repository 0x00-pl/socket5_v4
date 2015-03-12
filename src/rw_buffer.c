#include "rw_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void rw_buffer_init(rw_buffer_t *buffer){
    buffer->buff = (char*)malloc(1024);
    buffer->buff_size = 1024;
    buffer->frount = 0;
    buffer->wanna = 0;
    buffer->done = 0;
}

void rw_buffer_fin(rw_buffer_t *buffer){
    free(buffer->buff);
}

char *rw_buffer_frount(rw_buffer_t *buffer){
    return buffer->buff + buffer->frount;
}

char *rw_buffer_wanna(rw_buffer_t *buffer){
    return buffer->buff + buffer->frount + buffer->wanna;
}

char *rw_buffer_done(rw_buffer_t *buffer){
    return buffer->buff + buffer->frount + buffer->done;
}

void rw_buffer_pop_frount(rw_buffer_t *buffer, size_t size){
    buffer->frount += size;
    buffer->wanna = buffer->wanna<size? 0: buffer->wanna-size;
    buffer->done = buffer->done<size? 0: buffer->done-size;
    if(buffer->wanna==0 && buffer->done==0){
        buffer->frount = 0;
    }
}

void rw_buffer_size_upto(rw_buffer_t *buffer, size_t size){
    char *new_buff;
    if(buffer->buff_size - buffer->frount >= size){
        return;
    }
    if(buffer->buff_size < size){
//         printf("realloc buffer to: %zu\n", size);
        new_buff = (char*)malloc(size);
        memcpy(new_buff, buffer->buff + buffer->frount, buffer->buff_size - buffer->frount);
        free(buffer->buff);
        buffer->buff = new_buff;
        buffer->buff_size = size;
        buffer->frount = 0;
    }
    else{
//         printf("move buffer.\n");
        memmove(buffer->buff, buffer->buff + buffer->frount, buffer->buff_size - buffer->frount);
        buffer->frount = 0;
    }
}




