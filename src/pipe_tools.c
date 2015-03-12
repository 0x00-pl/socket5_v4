#include "pipe_tools.h"
#include "epoll_loop.h"
#include <stdint.h>
#include <string.h>

void convert_buff_2_size_t(char *buff, int *size, int reverse){
    int copy_of_size;
    if(!reverse){
        *size = (uint8_t)buff[0];
        *size <<= 8;
        *size |= (uint8_t)buff[1];
        *size <<= 8;
        *size |= (uint8_t)buff[2];
        *size <<= 8;
        *size |= (uint8_t)buff[3];
    }
    else{
        copy_of_size = *size;
        buff[3] = (char)(copy_of_size&0xff);
        copy_of_size >>= 8;
        buff[2] = (char)(copy_of_size&0xff);
        copy_of_size >>= 8;
        buff[1] = (char)(copy_of_size&0xff);
        copy_of_size >>= 8;
        buff[0] = (char)(copy_of_size&0xff);
    }
}

size_t msg_size(char *buff){
    int ret;
    convert_buff_2_size_t(buff, &ret, 0);
    return (size_t)ret;
}

size_t msg_header_size(char *buff){
    int ret;
    convert_buff_2_size_t(buff+4, &ret, 0);
    return (size_t)ret;
}

char *msg_header(char *buff){
    return buff+8;
}

size_t msg_data_size(char *buff){
    int ret;
    convert_buff_2_size_t(buff+msg_header_size(buff)+8, &ret, 0);
    return (size_t)ret;
}

char *msg_data(char *buff){
    return buff+msg_header_size(buff)+12;
}

void msg_init(char *buff, char *header, int header_size, char *data, int data_size){
    int msg_size;
    msg_size = header_size+data_size+8;
    convert_buff_2_size_t(buff, &msg_size, 1);
    convert_buff_2_size_t(buff+4, &header_size, 1);
    if(header != NULL){memcpy(msg_header(buff), header, (size_t)header_size);}
    convert_buff_2_size_t(buff+header_size+8, &data_size, 1);
    if(data != NULL){memcpy(msg_data(buff), data, (size_t)data_size);}
}

void msg_pipe_send(struct node_fd_decl *node, int fd, int type, char *data, int data_size){
    char *dst;
    int msg_size;
    if(node == NULL){return;}
    msg_size = data_size + 16;
    rw_buffer_size_upto(&node->write_buffer, node->write_buffer.wanna+(size_t)msg_size+4);
    dst = rw_buffer_wanna(&node->write_buffer);
    msg_init(dst, NULL, 8, NULL, data_size);
    convert_buff_2_size_t(msg_header(dst), &fd, 1);
    convert_buff_2_size_t(msg_header(dst)+4, &type, 1);
    memcpy(msg_data(dst), data, (size_t)data_size);
    node->write_buffer.wanna += (size_t)msg_size+4;
    el_enable_write(node);
}
