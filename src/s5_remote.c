#include "s5_remote.h"
#include "pipe_tools.h"
#include "dns_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

// listener 1; pipe n; web n*m[n];

static void remote_accept_connection(node_fd_t *node);
static void remote_link_to_web(char *s5_request, node_fd_t *pipe_node, int id);
static void remote_pipe_read_msg_size(node_fd_t *node);
static void remote_pipe_read_msg(node_fd_t *node);
static void remote_pipe_to_web(node_fd_t *node, char *data, size_t size);
static void remote_web_to_pipe(node_fd_t *node);
static void web_on_close(node_fd_t *node);
static void pipe_on_close(node_fd_t *node);
static void remote_link_to_web2(node_fd_t *node);
static int remote_create_web_link_fd_ip4(char *addr);
static void remote_dns_resolved_callback(node_fd_t *node, const char *addr_name, uint32_t ip__net_order);

// node is listener
static void listener_on_close(node_fd_t *node){
    (void)node;
    printf("listener error.\n");
    exit(-1);
}

void remote_create_listener(loop_manager_t *manager, uint16_t port){
    struct sockaddr_in addr;
    int fd;
    node_fd_t *node;
    node = (node_fd_t*)malloc(sizeof(node_fd_t));
    fd = socket(AF_INET, SOCK_STREAM, 0);
    socket_allow_reuse(fd);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        printf("bind listener fail.\n");
        exit(-1);
    }
    if(listen(fd, 10) < 0){
        printf("listener listen fail.\n");
        exit(-1);
    }
    node_fd_init((node_fd_t*)node, fd, EPOLLIN, manager);
    node->close = &listener_on_close;
    node->speical = &remote_accept_connection;
    loop_manager_add_node(manager, (node_fd_t*)node);
    printf("listener created.\n");
    el_enable_read((node_fd_t*)node);
}

// listener accept
static void remote_accept_connection(node_fd_t *node){
    int pipe_fd;
    node_fd_t *pipe_node;
    
    printf("accept connection.\n");
    for(;;){
        pipe_fd = accept(node->fd, NULL, NULL);
        if(pipe_fd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }
            else{
                // unknow error stop listening
                el_close_node(node);
                break;
            }
        }
        pipe_node = (node_fd_t*)malloc(sizeof(node_fd_t));
        node_fd_init(pipe_node, pipe_fd, EPOLLIN, node->manager);
        pipe_node->close = &pipe_on_close;
        loop_manager_add_node(node->manager, pipe_node);
        el_call_after_read_node(pipe_node, 4, &remote_pipe_read_msg_size);
    }
}




// node is pipe
static void pipe_on_close(node_fd_t *node){
    (void)node;
    node->i_am_zombe = 1;
    printf("pipe error.\n");
}

static void send_close_msg(loop_manager_t *manager, int pipe_fd, int id){
    node_fd_t *node = loop_manager_fd_2_node(manager, pipe_fd);
    msg_pipe_send(node, id, msg_type_close, NULL, 0);
}

static void remote_link_to_web(char *s5_request, node_fd_t *pipe_node, int id){
    node_web_t *web_node;
    char atyp;
    uint8_t domain_name_size;
    int fd;
    
    atyp = s5_request[3];
    if(atyp == 0x01){
        web_node = (node_web_t*)malloc(sizeof(node_web_t));
        fd = remote_create_web_link_fd_ip4(s5_request+4);
        if(fd < 0){
            send_close_msg(pipe_node->manager, pipe_node->fd, id);
            return;
        }
        
        node_fd_init((node_fd_t*)web_node, fd, EPOLLIN|EPOLLOUT, pipe_node->manager);
        web_node->base.close = &web_on_close;
        web_node->base.speical = &remote_link_to_web2;
        web_node->pipe_fd = pipe_node->fd;
        web_node->id = id;
        loop_manager_add_node(pipe_node->manager, (node_fd_t*)web_node);
        loop_manager_add_id(pipe_node->manager, id, fd);
        
        el_enable_read((node_fd_t*)web_node);
    }
    else if(atyp == 0x03){
        // domainname, need dns
        domain_name_size = (uint8_t)s5_request[4];
        
        web_node = (node_web_t*)malloc(sizeof(node_web_t));
        node_fd_init((node_fd_t*)web_node, 0, EPOLLIN|EPOLLOUT, pipe_node->manager);
        web_node->base.close = &web_on_close;
        web_node->base.speical = &remote_link_to_web2;
        web_node->pipe_fd = pipe_node->fd;
        web_node->base.fd = -1;
        web_node->id = id;
        web_node->port__net_order = (uint16_t)(((uint8_t)s5_request[domain_name_size+5]) 
                                              |((uint8_t)s5_request[domain_name_size+6]<<8));
        
        s5_request[domain_name_size + 5] = '\0'; // set 0x00 end str after read port
        dns_request(pipe_node->manager->dns_fd, pipe_node->manager->dns_servers, pipe_node->manager->dns_servers_size,
            pipe_node->manager->dns_pool, s5_request + 5,
            remote_dns_resolved_callback, (node_fd_t*)web_node
        );
    }
    else{
        printf("unsupport atyp: %x\n", atyp);
        send_close_msg(pipe_node->manager, pipe_node->fd, id);
    }
}

static void remote_dns_resolved_callback(node_fd_t *node, const char *addr_name, uint32_t ip__net_order){
    struct sockaddr_in saddr;
    int fd;
    node_web_t *web_node = (node_web_t*)node;
    
    if(ip__net_order == DNS_NOT_FOUND){
        send_close_msg(node->manager, web_node->pipe_fd, web_node->id);
        return;
    }
    
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        send_close_msg(node->manager, web_node->pipe_fd, web_node->id);
        return;
    }
    socket_non_blocking(fd);
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = web_node->port__net_order;
    saddr.sin_addr.s_addr = ip__net_order;
    connect(fd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));
    printf("[debug] connecting %s (%s): %d\n", addr_name, inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
    
    node->fd = fd;
    loop_manager_add_node(node->manager, (node_fd_t*)web_node);
    loop_manager_add_id(node->manager, web_node->id, fd);
    
    el_enable_read((node_fd_t*)web_node);
}

static void remote_pipe_read_msg_size(node_fd_t *node){
    int msg_size;
    convert_buff_2_size_t(rw_buffer_frount(&node->read_buffer), &msg_size, 0);
    el_call_after_read_node(node, (size_t)msg_size, &remote_pipe_read_msg);
}

static void remote_pipe_read_msg(node_fd_t *node){
    node_web_t *web_node;
    int type;
    int id;
    char *src;
    
    src = rw_buffer_frount(&node->read_buffer);
    convert_buff_2_size_t(msg_header(src), &id, 0);
    convert_buff_2_size_t(msg_header(src)+4, &type, 0);
//     printf("[remote] read data size: %d\n", (int)msg_data_size(src));
    web_node = (node_web_t*)loop_manager_id_2_node(node->manager, id);

    if(type == msg_type_connect){
//         printf("remote recv connection request.\n");
        remote_link_to_web(msg_data(src), node, id);
    }
    else if(type == msg_type_pipe){
//         printf("remote recv pipe.\n");
        if(web_node == NULL){
            printf("[waring] cannot find web_node. id: %d\n", id);
            send_close_msg(node->manager, node->fd, id);
        }
        else{
            remote_pipe_to_web((node_fd_t*)web_node, msg_data(src), msg_data_size(src));
        }
    }
    else if(type == msg_type_close){
        printf("remote recv close.\n");
        loop_manager_remove_id(node->manager, id);
        el_close_node((node_fd_t*)web_node);
    }
    else{
        printf("[waring] unknow msg type.\n");
        loop_manager_remove_id(node->manager, id);
        el_close_node((node_fd_t*)web_node);
    }
    rw_buffer_pop_frount(&node->read_buffer, msg_size(src)+4);
    el_call_after_read_node(node, 4, &remote_pipe_read_msg_size);
}

static void remote_pipe_to_web(node_fd_t *node, char *data, size_t size){
    el_write_to_node(node, data, size);
}

// node is web_node
static void web_on_close(node_fd_t *node){
    node_web_t *nodeex = (node_web_t*)node;
    printf("web error.\n");
    node->i_am_zombe = 1;
    loop_manager_remove_id(node->manager, nodeex->id);
    send_close_msg(node->manager, nodeex->pipe_fd, nodeex->id);
}

static int remote_create_web_link_fd_ip4(char *addr){
    uint32_t ip;
    uint16_t port;
    int fd;
    int connect_ret;
    struct sockaddr_in saddr;
    
    ip = (uint8_t)addr[3]; ip <<= 8;
    ip |= (uint8_t)addr[2]; ip <<= 8;
    ip |= (uint8_t)addr[1]; ip <<= 8;
    ip |= (uint8_t)addr[0];
    
    port = (uint8_t)addr[5]; port = (uint16_t)(port<<8);
    port = (uint16_t)(port|(uint8_t)addr[4]);
    
    bzero(&saddr, sizeof(struct sockaddr_in));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = ip;
    saddr.sin_port = port;
    
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        printf("%s\n", "create socket fail.");
        exit(-1);
    }
    socket_non_blocking(fd);
    
    connect_ret = connect(fd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));
    if(connect_ret < 0){
        if(errno != EINPROGRESS){
            printf("connect web fail.\n");
            return -1;
        }
        printf("connecting. %s : %d\n", inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
    }
    return fd;
}

static void remote_link_to_web2(node_fd_t *node){
    static char rep_ok[] = {0x05,0x00,0x00,0x01,0,0,0,0,0,0};
    node_web_t *web_node = (node_web_t*)node;
    int opt_ret;
    int opt_ret_size;

    node->speical = NULL;
    opt_ret_size = sizeof(int);
    getsockopt(node->fd, SOL_SOCKET, SO_ERROR, &opt_ret, (socklen_t*)&opt_ret_size);
    if(opt_ret != 0){
        printf("connect web fail.\n");
        el_close_node(node);
    }
    else{
        printf("connect web ok.\n");
        node_fd_t *pipe_node = loop_manager_fd_2_node(node->manager, web_node->pipe_fd);
        printf("send ok msg.\n");
        msg_pipe_send(pipe_node, web_node->id, msg_type_connect, rep_ok, 10);
        el_call_after_read_node(node, 0, &remote_web_to_pipe);
    }
}

static void remote_web_to_pipe(node_fd_t *node){
    node_web_t *web_node = (node_web_t*)node;
    node_fd_t *pipe_node = loop_manager_fd_2_node(node->manager, web_node->pipe_fd);
    if(pipe_node == NULL){
        el_close_node(node);
        return;
    }

//     printf("[remote] send data size: %d\n", (int)node->read_buffer.done);
    msg_pipe_send(pipe_node, web_node->id, msg_type_pipe, rw_buffer_frount(&node->read_buffer), (int)node->read_buffer.done);
    
    node->read_buffer.wanna += node->read_buffer.done;
    rw_buffer_pop_frount(&node->read_buffer, node->read_buffer.done);
    el_call_after_read_node(node, 0, &remote_web_to_pipe);
}


