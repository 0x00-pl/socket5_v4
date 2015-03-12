#include "epoll_loop.h"
#include "rb_tree.h"
#include "rw_buffer.h"
#include "dns_pool.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>


void socket_non_blocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags|O_NONBLOCK);
}

int socket_allow_reuse(int fd){
    int opt;
    opt = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        printf("%s\n", "set socket reuse fail.");
        return -1;
    }
    return 0;
}

void node_fd_init(node_fd_t *node, int fd, int epoll_event, struct loop_manager_decl *manager){
    node->fd = fd;
    node->e_event = epoll_event;
    node->manager = manager;
    rw_buffer_init(&node->read_buffer);
    rw_buffer_init(&node->write_buffer);
    node->speical = NULL;
    node->close = NULL;
    node->after_read = NULL;
    node->i_am_zombe = 0;
//     printf("fd %d init\n", node->fd);
}

void node_fd_fin(node_fd_t *node){
//     printf("fd %d fin\n", node->fd);
    rw_buffer_fin(&node->read_buffer);
    rw_buffer_fin(&node->write_buffer);
    close(node->fd);
}


static void node_fd_insert(node_fd_t **proot, node_fd_t *node){
    node_fd_t *parent = NULL;
    node_fd_t *sub_root = *proot;
    
    while(sub_root != NULL){
        if(sub_root->fd == node->fd){
            rbt_replace((rb_node**)proot, (rb_node*)sub_root, (rb_node*)node);
            node_fd_fin(sub_root);
            free(sub_root);
            return;
        }
        parent = sub_root;
        sub_root = (node_fd_t*)(node->fd<sub_root->fd? sub_root->base.left: sub_root->base.right);
    }
    node->base.parent = (rb_node*)parent;
    if(parent != NULL){
        *(node->fd<parent->fd? &parent->base.left: &parent->base.right) = (rb_node*)node;
    }
    rbt_after_insert((rb_node**)proot, (rb_node*)node);
}

static node_fd_t *node_fd_find(node_fd_t *root, int fd){
    while(root != NULL){
        if(fd == root->fd){
            return root;
        }
        root = (node_fd_t*)((fd < root->fd)? root->base.left: root->base.right);
    }
    return NULL;
}

void node_id_init(node_id_t *nodeid, int id, int fd){
    nodeid->id = id;
    nodeid->fd = fd;
}
void node_id_fin(node_id_t *nodeid){
    (void)nodeid;
}

void node_id_insert(node_id_t **proot, node_id_t *node){
    node_id_t *parent = NULL;
    node_id_t *sub_root = *proot;
    
    while(sub_root != NULL){
        if(sub_root->id == node->id){
            rbt_replace((rb_node**)proot, (rb_node*)sub_root, (rb_node*)node);
            node_id_fin(sub_root);
            free(sub_root);
            return;
        }
        parent = sub_root;
        sub_root = (node_id_t*)(node->id<sub_root->id? sub_root->base.left: sub_root->base.right);
    }
    node->base.parent = (rb_node*)parent;
    if(parent != NULL){
        *(node->id<parent->id? &parent->base.left: &parent->base.right) = (rb_node*)node;
    }
    rbt_after_insert((rb_node**)proot, (rb_node*)node);
}

node_id_t *node_id_find(node_id_t *root, int id){
    while(root != NULL){
        if(id == root->id){
            return root;
        }
        root = (node_id_t*)((id < root->id)? root->base.left: root->base.right);
    }
    return NULL;
}

static uint32_t *dns_servers(){
    static uint32_t dns_servers_[3];
    dns_servers_[0] = inet_addr("8.8.8.8");
    dns_servers_[1] = inet_addr("8.8.4.4");
    dns_servers_[2] = inet_addr("114.114.114.114");
    dns_servers_[3] = inet_addr("202.99.160.68");
    dns_servers_[4] = 0;
    return dns_servers_;
}

void loop_manager_init(loop_manager_t *lm){
    lm->root = NULL;
    lm->idroot = NULL;
    lm->dns_servers = NULL;
    lm->dns_servers_size = 0;
    lm->epoll_fd = epoll_create1(0);
    lm->dns_pool = (dns_pool_t*)malloc(sizeof(dns_pool_t));
    dns_pool_init(lm->dns_pool);
    lm->dns_fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_non_blocking(lm->dns_fd);
    lm->dns_servers = dns_servers();
    lm->dns_servers_size = 0;
    while(lm->dns_servers[lm->dns_servers_size] != 0){
        lm->dns_servers_size++;
    }
}

void loop_manager_close(loop_manager_t *lm){
    node_fd_t *iter;
    node_fd_t *iter_next;
    
    dns_pool_fin(lm->dns_pool);
    free(lm->dns_pool);
    
    for(iter=(node_fd_t*)rbt_min((rb_node*)lm->root); iter!=NULL; iter=iter_next){
        iter_next = (node_fd_t*)rbt_next((rb_node*)iter);
        el_close_node(iter);
    }
}

void loop_manager_fin(loop_manager_t *lm){
    node_fd_t *fd_iter;
    node_fd_t *fd_iter_next;
    node_id_t *id_iter;
    node_id_t *id_iter_next;
    
    
    for(fd_iter=(node_fd_t*)rbt_min((rb_node*)lm->root); fd_iter!=NULL; fd_iter=fd_iter_next){
        fd_iter_next = (node_fd_t*)rbt_next((rb_node*)fd_iter);
        loop_manager_force_remove_node(lm, fd_iter);
    }
    for(id_iter=(node_id_t*)rbt_min((rb_node*)lm->idroot); id_iter!=NULL; id_iter=id_iter_next){
        id_iter_next = (node_id_t*)rbt_next((rb_node*)id_iter);
        rbt_pop((rb_node**)&lm->idroot, (rb_node*)id_iter);
        node_id_fin(id_iter);
        free(id_iter);
    }
    close(lm->epoll_fd);
}

node_fd_t *loop_manager_fd_2_node(loop_manager_t *manager, int fd){
    return node_fd_find(manager->root, fd);
}

node_fd_t *loop_manager_id_2_node(loop_manager_t *manager, int id){
    node_id_t *node_id = node_id_find(manager->idroot, id);
    int fd;
    if(node_id == NULL){return NULL;}
    fd = node_id->fd;
    return loop_manager_fd_2_node(manager, fd);
}

void loop_manager_add_id(loop_manager_t *lm, int id, int fd){
//     printf("add id[fd]: %d[%d]\n", id, fd);
    node_id_t *node_id;
    node_id = (node_id_t*)malloc(sizeof(node_id_t));
    node_id_init(node_id, id, fd);
    node_id_insert(&lm->idroot, node_id);
}

void loop_manager_remove_id(loop_manager_t* lm, int id){
//     printf("remove id: %d\n", id);
    node_id_t *node_id;
    node_id = node_id_find(lm->idroot, id);
    if(node_id == NULL){return;}
    rbt_pop((rb_node**)&lm->idroot, (rb_node*)node_id);
    node_id_fin(node_id);
    free(node_id);
}

void loop_manager_add_node(loop_manager_t *lm, node_fd_t *node){
    struct epoll_event epe;
    socket_non_blocking(node->fd);
    node_fd_insert(&lm->root, node);
    
    bzero(&epe, sizeof(epe));
    epe.data.fd = node->fd;
    epe.events = (uint32_t)node->e_event;
    epoll_ctl(lm->epoll_fd, EPOLL_CTL_ADD, node->fd, &epe);
}

void loop_manager_force_remove_node(loop_manager_t* manager, node_fd_t* node){
    if(node == NULL){return;}
    rbt_pop((rb_node**)&manager->root, (rb_node*)node);
    epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, node->fd, NULL);
    node_fd_fin(node);
    free(node);
}

void loop_manager_modify_node(loop_manager_t *manager, node_fd_t *node){
    struct epoll_event epe;
    bzero(&epe, sizeof(epe));
    epe.data.fd = node->fd;
    epe.events = (uint32_t)node->e_event;
    epoll_ctl(manager->epoll_fd, EPOLL_CTL_MOD, node->fd, &epe);
}

static void write_callback(node_fd_t *node){
    ssize_t writed;
//     printf("write size: %d\n", (int)node->write_buffer.wanna);
    if(node->write_buffer.wanna > 0){
        writed = write(node->fd, rw_buffer_frount(&node->write_buffer), node->write_buffer.wanna);
        if(writed <= 0){
            printf("%s\n", "write fd fail");
            el_close_node(node);
            return;
        }
        node->write_buffer.done += (size_t)writed;
        rw_buffer_pop_frount(&node->write_buffer, (size_t)writed);
    }
    if(node->write_buffer.done >= node->write_buffer.wanna){
        // write done
        node->e_event &= (~EPOLLOUT);
        loop_manager_modify_node(node->manager, node);
    }
}

static void read_callback(node_fd_t *node){
    static size_t READ_BLOCK_SIZE = 1024;
    ssize_t readed;
    
    for(;;){
        rw_buffer_size_upto(&node->read_buffer, node->read_buffer.done + READ_BLOCK_SIZE);
        readed = read(node->fd, rw_buffer_done(&node->read_buffer), READ_BLOCK_SIZE);
        if(readed <= 0){
//             if(errno==EAGAIN && errno==EWOULDBLOCK){
//                 printf("errno: %d\n", errno);
//                 printf("why fd: %d EAGAIN?\n", node->fd); // maybe because last read had just read a size of READ_BLOCK_SIZE.
//                 readed = 0;
//             }
//             else
            {
                printf("read fd %d fail, connection break.\n", node->fd);
                printf("errno: %d\n", errno);
                el_close_node(node);
                loop_manager_force_remove_node(node->manager, node);
                return;
            }
        }
        node->read_buffer.done += (size_t)readed;
        if((size_t)readed <= READ_BLOCK_SIZE){
            // cur read done
            if(node->read_buffer.done >= node->read_buffer.wanna){
                // no more for read
                node->e_event &= (~EPOLLIN);
                loop_manager_modify_node(node->manager, node);
                // callback
                node->after_read(node);
                return;
            }
            return; // wait for read more.
        }
    }
}

void loop_manager_poll(loop_manager_t *manager, int timeout){
    struct epoll_event epe[512];
    node_fd_t *node;
    node_fd_t *node_next;
    int ep_size;
    int cur_fd;
    int cur_event;
    int i;
    
    dns_recv_reply(manager->dns_pool, manager->dns_fd);
    
    ep_size = epoll_wait(manager->epoll_fd, epe, 512, timeout);
    for(i=0; i<ep_size; i++){
        cur_fd = epe[i].data.fd;
        cur_event = (int)epe[i].events;
        node = node_fd_find(manager->root, cur_fd);
        if(cur_event & EPOLLERR){
            el_close_node(node);
            loop_manager_force_remove_node(node->manager, node);
        }
        else if(node->speical != NULL){
            node->speical(node);
        }
        else if(cur_event & (EPOLLOUT|EPOLLIN)){
            if(cur_event & EPOLLOUT){
                write_callback(node);
            }
            if(cur_event & EPOLLIN){
                read_callback(node);
            }
        }
        else{
            // unknow
            printf("%s\n", "unknow epoll event");
            el_close_node(node);
        }
    }
    // kill zombe
    for(node=(node_fd_t*)rbt_min((rb_node*)manager->root); node!=NULL; node=node_next){
        node_next = (node_fd_t*)rbt_next((rb_node*)node);
        if(node->i_am_zombe == 0){continue;}
        if(node->write_buffer.wanna == 0 || (node->e_event&EPOLLERR)){
            rbt_pop((rb_node**)&manager->root, (rb_node*)node);
            epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, node->fd, NULL);
            node_fd_fin(node);
            free(node);
        }
        else{
            el_enable_write(node);
        }
    }
}

void el_enable_write(node_fd_t *node){
    if((node->e_event & EPOLLOUT) == 0){
        node->e_event |= EPOLLOUT;
        loop_manager_modify_node(node->manager, node);
    }
}

void el_enable_read(node_fd_t *node){
    if((node->e_event & EPOLLIN) == 0){
        node->e_event |= EPOLLIN;
        loop_manager_modify_node(node->manager, node);
    }
}

void el_close_node(node_fd_t *node){
    if(node == NULL){return;}
    node->i_am_zombe = 1;
    if(node->close != NULL){
        node->close(node);
    }
    el_enable_write(node);
}

void el_write_to_node(node_fd_t *node, const char *buff, size_t size){
    rw_buffer_size_upto(&node->write_buffer, node->write_buffer.wanna + size);
    memcpy(rw_buffer_wanna(&node->write_buffer), buff, size);
    node->write_buffer.wanna += size;
    el_enable_write(node);
}

void el_call_after_read_node(node_fd_t *node, size_t size, el_callback_f *callback){
//     printf("fd %d need read %zu + %d\n", node->fd, node->read_buffer.wanna, (int)size);
    rw_buffer_size_upto(&node->read_buffer, node->read_buffer.wanna + size);
    node->read_buffer.wanna += size;
    node->after_read = callback;
    
    if(node->read_buffer.done >= node->read_buffer.wanna && node->read_buffer.wanna != 0){
        node->after_read(node);
    }
    else{
        el_enable_read(node);
    }
}

void el_default_connect_callback(node_fd_t *node){
    int opt_ret;
    int opt_ret_size;

    node->speical = NULL;
    opt_ret_size = sizeof(int);
    getsockopt(node->fd, SOL_SOCKET, SO_ERROR, &opt_ret, (socklen_t*)&opt_ret_size);
    if(opt_ret != 0){
        el_close_node(node);
    }
    else{
        el_enable_read(node);
    }
}