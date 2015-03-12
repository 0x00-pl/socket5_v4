#ifndef _PIPE_TOOLS_H_
#define _PIPE_TOOLS_H_

#include <stddef.h>


enum{
    msg_type_connect,
    msg_type_pipe,
    msg_type_close,
    msg_type_dns,
    msg_connection,
    msg_local,
};


void convert_buff_2_size_t(char *buff, int *size, int reverse);
size_t msg_size(char *buff);
size_t msg_header_size(char *buff);
char *msg_header(char *buff);
size_t msg_data_size(char *buff);
char *msg_data(char *buff);
void msg_init(char *buff, char *header, int header_size, char *data, int data_size);
struct node_fd_decl;
void msg_pipe_send(struct node_fd_decl *node, int fd, int type, char *data, int data_size);

#endif
