#ifndef _S5_LOCAL_H_
#define _S5_LOCAL_H_
#include <stdint.h>
#include "epoll_loop.h"

typedef struct node_bind_pipe_decl{
    node_fd_t base;
    node_fd_t *pipe;
} node_bind_pipe_t;

// // node is pipe
node_fd_t *local_create_pipe(loop_manager_t *manager, const char *remote_ip, uint16_t port);
// void pipe_read_message_size(node_fd_t *node);
// void pipe_read_message(node_fd_t *node);
// // node is listener
// void listener_close(node_fd_t *node);
void local_create_listener(loop_manager_t *manager, node_fd_t *pipe, uint16_t port);
// // node is connection
// void connection_close(node_fd_t *node);
// void local_accept_connection(node_fd_t *node);
// void s5_mothed_c(node_fd_t *node);
// void s5_mothed_c2(node_fd_t *node);
// void s5_request_c(node_fd_t *node);
// void s5_request_c2(node_fd_t *node);
// void s5_reply_r(node_fd_t *node, char rep);
// void s5_connection_to_pipe(node_fd_t *node);






#endif
