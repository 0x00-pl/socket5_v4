#ifndef _EPOLL_LOOP_H_
#define _EPOLL_LOOP_H_

#include "rb_tree.h"
#include "rw_buffer.h"
#include "pipe_tools.h"
#include <stdint.h>


void socket_non_blocking(int fd);
int socket_allow_reuse(int fd);

struct node_fd_decl;
typedef void el_callback_f(struct node_fd_decl *node);
struct loop_manager_decl;

typedef struct node_fd_decl{
    rb_node base;
    int fd;
    int e_event;
    rw_buffer_t read_buffer;
    rw_buffer_t write_buffer;
    el_callback_f *speical;
    el_callback_f *close;
    el_callback_f *after_read;
    struct loop_manager_decl* manager;
    int i_am_zombe;
} node_fd_t;

void node_fd_init(node_fd_t *node, int fd, int epoll_event, struct loop_manager_decl *manager);
void node_fd_fin(node_fd_t *node);

typedef struct node_id_decl{
    rb_node base;
    int id;
    int fd;
} node_id_t;

void node_id_init(node_id_t *nodeid, int id, int fd);
void node_id_fin(node_id_t *nodeid);
void node_id_insert(node_id_t **proot, node_id_t *node);
node_id_t *node_id_find(node_id_t *root, int id);

struct dns_pool_decl;
typedef struct loop_manager_decl{
    node_fd_t *root;
    node_id_t *idroot;
    int epoll_fd;
    int dns_fd;
    struct dns_pool_decl *dns_pool;
    uint32_t *dns_servers;
    size_t dns_servers_size;
} loop_manager_t;


void loop_manager_init(loop_manager_t *lm);
void loop_manager_close(loop_manager_t *lm);
void loop_manager_fin(loop_manager_t *lm);

node_fd_t *loop_manager_fd_2_node(loop_manager_t *manager, int fd);
node_fd_t *loop_manager_id_2_node(loop_manager_t *manager, int id);

void loop_manager_add_id(loop_manager_t *lm, int id, int fd);
void loop_manager_remove_id(loop_manager_t* manager, int id);

void loop_manager_add_node(loop_manager_t *lm, node_fd_t *node);
void loop_manager_force_remove_node(loop_manager_t *manager, node_fd_t *node);
void loop_manager_modify_node(loop_manager_t *manager, node_fd_t *node);
void loop_manager_poll(loop_manager_t *manager, int timeout);

void el_enable_write(node_fd_t *node);
void el_enable_read(node_fd_t *node);
void el_close_node(node_fd_t *node);
void el_write_to_node(node_fd_t *node, const char *buff, size_t size);
void el_call_after_read_node(node_fd_t *node, size_t size, el_callback_f *callback);
void el_default_connect_callback(node_fd_t *node);

#endif
