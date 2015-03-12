#ifndef _S5_REMOTE_H_
#define _S5_REMOTE_H_

#include "epoll_loop.h"
#include <stdint.h>

typedef struct node_web_decl{
    node_fd_t base;
    int pipe_fd; // close self when pipe close
    int id; // id of peer node
    uint16_t port__net_order;
} node_web_t;

// // node is listener
void remote_create_listener(loop_manager_t *manager, uint16_t port);
// void remote_accept_connection(node_fd_t *node);
// // node is pipe
// void remote_link_to_web(char *s5_request, node_fd_t *pipe_node, int id);
// void remote_pipe_read_msg_size(node_fd_t *node);
// void remote_pipe_read_msg(node_fd_t *node);
// void remote_pipe_to_web(node_fd_t *node, char *data, size_t size);
// // node is web_node
// void remote_web_to_pipe(node_fd_t *node);

#endif
