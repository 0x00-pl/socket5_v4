#include "s5_local.h"
#include "pipe_tools.h"
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>

static void pipe_read_message_size(node_fd_t *node);
static void pipe_read_message(node_fd_t *node);
static void listener_close(node_fd_t *node);
static void connection_close(node_fd_t *node);
static void local_accept_connection(node_fd_t *node);
static void s5_mothed_c(node_fd_t *node);
static void s5_mothed_c2(node_fd_t *node);
static void s5_request_c(node_fd_t *node);
static void s5_request_c2(node_fd_t *node);
static void s5_reply_r(node_fd_t *node, char rep);
static void s5_connection_to_pipe(node_fd_t *node);


static void pipe_on_close(node_fd_t *node){
    (void)node;
    printf("pipe close.\n");
    exit(-1);
}

node_fd_t *local_create_pipe(loop_manager_t *manager, const char *remote_ip, uint16_t port){
    struct sockaddr_in remote_addr;
    node_fd_t *pipe_node;
    int fd;
    pipe_node = (node_fd_t*)malloc(sizeof(node_fd_t));
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        printf("%s\n", "create socket fail.");
        exit(-1);
    }
    node_fd_init(pipe_node, fd, EPOLLIN, manager);
    pipe_node->close = &pipe_on_close;
    pipe_node->speical = &el_default_connect_callback;
    loop_manager_add_node(manager, pipe_node);
    el_call_after_read_node(pipe_node, 4, pipe_read_message_size);
    
    bzero(&remote_addr, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    inet_pton(AF_INET, remote_ip, &(remote_addr.sin_addr));
    remote_addr.sin_port = htons(port);
    if(connect(fd, (struct sockaddr*)&remote_addr, sizeof(remote_addr)) < 0){
        if(errno != EINPROGRESS){
            printf("%s\n", "connect remote fail.");
            exit(-1);
        }
    }
    return pipe_node;
}

// msg pipe
static void pipe_read_message_size(node_fd_t *node){
    int size;
    convert_buff_2_size_t(rw_buffer_frount(&node->read_buffer), &size, 0);
    el_call_after_read_node(node, (size_t)size, pipe_read_message);
}

static void pipe_read_message(node_fd_t *node){
    int connection_fd;
    int type;
    node_fd_t *connection_node;
    char *src;

    src = rw_buffer_frount(&node->read_buffer);
    convert_buff_2_size_t(msg_header(src), &connection_fd, 0);
    convert_buff_2_size_t(msg_header(src)+4, &type, 0);
//     printf("[local] read data size: %d\n", (int)msg_data_size(src));
    if(msg_data_size(src) > 10000){
        printf("[waring] read a big big msg.\n");
    }
    connection_node = loop_manager_fd_2_node(node->manager, connection_fd);
    if(connection_node != NULL){
        if(type == msg_type_connect){
            s5_reply_r(connection_node, msg_data(src)[1]);
        }
        else if(type == msg_type_pipe){
//             printf("[message] recv size: %d\n", (int)msg_data_size(src));
            el_write_to_node(connection_node, msg_data(src), msg_data_size(src));
        }
        else if(type == msg_type_close){
            printf("closeing fd: %d\n", connection_fd);
            el_close_node(connection_node);
        }
        else{
            printf("[waring] unknow type: %d ignore\n", type);
        }
    }
    else{
        printf("[waring] unknow id: %d ignore\n", connection_fd);
    }
    rw_buffer_pop_frount(&node->read_buffer, msg_size(src)+4);
    el_call_after_read_node(node, 4, pipe_read_message_size);
}

static void listener_close(node_fd_t *node){
    int idata;
    char data[4];
    
    idata = msg_local;
    convert_buff_2_size_t(data, &idata, 1);
    msg_pipe_send(node, node->fd, msg_type_close, data, 4);
    printf("listener closed.\n");
    exit(-1);
}

void local_create_listener(loop_manager_t *manager, node_fd_t *pipe, uint16_t port){
    struct sockaddr_in addr;
    int fd;
    node_bind_pipe_t *node;
    node = (node_bind_pipe_t*)malloc(sizeof(node_bind_pipe_t));
    fd = socket(AF_INET, SOCK_STREAM, 0);
    socket_allow_reuse(fd);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        printf("%s\n", "bind listener fail.");
        exit(-1);
    }
    listen(fd, 10);
    node_fd_init((node_fd_t*)node, fd, EPOLLIN, manager);
    node->pipe = pipe;
    node->base.close = &listener_close;
    node->base.speical = &local_accept_connection;
    loop_manager_add_node(manager, (node_fd_t*)node);
    el_enable_read((node_fd_t*)node);
}

static void connection_close(node_fd_t *node){
    int idata;
    char data[4];
    node_bind_pipe_t *nodeex;
    
    if(node == NULL){return;}
    nodeex = (node_bind_pipe_t*)node;
    
    idata = msg_connection;
    convert_buff_2_size_t(data, &idata, 1);
    msg_pipe_send(nodeex->pipe, node->fd, msg_type_close, data, 4);
}

// listener accept
static void local_accept_connection(node_fd_t *node){
    struct sockaddr connection_addr;
    socklen_t connection_addr_len;
    int connection_fd;
    node_bind_pipe_t *nodeex;
    node_bind_pipe_t *connection_node;
    
    connection_addr_len = sizeof(connection_addr); 
    nodeex = (node_bind_pipe_t*)node;
    for(;;){
        connection_fd = accept(node->fd, &connection_addr, &connection_addr_len);
        if(connection_fd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }
            else{
                // unknow error stop listening
                printf("[error] accept error: %d\n", errno);
                el_close_node(node);
                break;
            }
        }
        connection_node = (node_bind_pipe_t*)malloc(sizeof(node_bind_pipe_t));
        node_fd_init(&connection_node->base, connection_fd, EPOLLIN, node->manager);
        connection_node->base.close = connection_close;
        connection_node->pipe = nodeex->pipe;
        loop_manager_add_node(node->manager, (node_fd_t*)connection_node);
        el_call_after_read_node((node_fd_t*)connection_node, 2, s5_mothed_c);
    }
}

// local read header
static void s5_mothed_c(node_fd_t *node){
    uint8_t n_mothed = (uint8_t)rw_buffer_frount(&node->read_buffer)[1];
    if(rw_buffer_frount(&node->read_buffer)[0] != 0x05){
        printf("[error] mothed version check fail. cmd: %x\n", (uint8_t)rw_buffer_frount(&node->read_buffer)[0]);
        el_close_node(node);
        return;
    }
    el_call_after_read_node(node, (size_t)n_mothed, s5_mothed_c2);
}

static void s5_mothed_c2(node_fd_t *node){
    static char mothed_no_auth[2] = {0x05, 0x00};
    char n_mothed = rw_buffer_frount(&node->read_buffer)[1];
    rw_buffer_pop_frount(&node->read_buffer, 2+(size_t)n_mothed);
    
    printf("cmd reply.\n");
    el_write_to_node(node, mothed_no_auth, 2);
    
    el_call_after_read_node(node, 5, s5_request_c);
}

static void s5_request_c(node_fd_t *node){
    char atyp = rw_buffer_frount(&node->read_buffer)[3];
    char addr0 = rw_buffer_frount(&node->read_buffer)[4];
    size_t wanna;
    
    if(atyp == 0x01){
        wanna = 5;
    }
    else if(atyp == 0x03){
        wanna = 2 + (size_t)addr0;
    }
    else if(atyp == 0x04){
        wanna = 17;
    }
    else{
        printf("[waring] unknow atyp: %d\n", atyp);
    }
    el_call_after_read_node(node, wanna, s5_request_c2);
}

static void s5_request_c2(node_fd_t *node){
    node_bind_pipe_t *nodeex = (node_bind_pipe_t*)node;
    char atyp = rw_buffer_frount(&node->read_buffer)[3];
    char addr0 = rw_buffer_frount(&node->read_buffer)[4];
    int data_size;
    
    if(atyp == 0x01){
        data_size = 10;
    }
    else if(atyp == 0x03){
        data_size = 7 + (int)addr0;
    }
    else if(atyp == 0x04){
        data_size = 22;
    }
    
//     printf("send requet. size: %d\n", data_size);
    msg_pipe_send(nodeex->pipe, node->fd, msg_type_connect, rw_buffer_frount(&node->read_buffer), data_size);
    rw_buffer_pop_frount(&node->read_buffer, (size_t)data_size);
}

static void s5_reply_r(node_fd_t *node, char rep){
    static char rep_ok[] = {0x05,0x00,0x00,0x01,0,0,0,0,0,0};
    
    if(rep == 0x00){
        printf("%s\n", "remote say ok.");
        el_write_to_node(node, rep_ok, 10);
        rw_buffer_pop_frount(&node->read_buffer, node->read_buffer.wanna);
        el_call_after_read_node(node, 0, &s5_connection_to_pipe);
    }
    else{
        printf("%s\n", "remote say connect fail.");
        el_close_node(node);
    }
}

static void s5_connection_to_pipe(node_fd_t *node){
    node_bind_pipe_t *nodeex = (node_bind_pipe_t*)node;
    int data_size = (int)node->read_buffer.done;
    
//     printf("[local] read data size: %d\n", (int)data_size);
    
    msg_pipe_send(nodeex->pipe, node->fd, msg_type_pipe, rw_buffer_frount(&node->read_buffer), data_size);
    node->read_buffer.wanna += node->read_buffer.done;
    rw_buffer_pop_frount(&node->read_buffer, node->read_buffer.done);
    el_call_after_read_node(node, 0, &s5_connection_to_pipe);
}


