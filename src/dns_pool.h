#ifndef _DNS_POOL_H_
#define _DNS_POOL_H_

#include <stdint.h>
#include "epoll_loop.h"

typedef void dns_find_callback_f(node_fd_t *node, const char *addr_name, uint32_t ip__net_order);
typedef struct dns_find_callback_item_decl{
    dns_find_callback_f *func;
    node_fd_t *node;
} dns_find_callback_item_t;

void dns_find_callback_item_init(dns_find_callback_item_t *item, dns_find_callback_f *func, node_fd_t *node);
void dns_find_callback_item_close(dns_find_callback_item_t *item);

typedef struct dns_item_decl{
    uint32_t hash_key;
    char *addr_name;
    uint32_t ip__net_order;
    int timeout;
    int livetime;
    dns_find_callback_item_t *find_cb_list;
    size_t find_cb_list_size;
    size_t find_cb_list_maxsize;
} dns_item_t;

void dns_item_init(dns_item_t *item, const char *addr_name, int timeout);
void dns_item_fin(dns_item_t *item);
void dns_item_add_callback(dns_item_t *item, dns_find_callback_f *func, node_fd_t *node);
void dns_item_notify_all(dns_item_t *item);

typedef struct dns_pool_decl{
    dns_item_t *items;
    size_t items_size;
    size_t items_maxsize;
} dns_pool_t;

void dns_pool_init(dns_pool_t *pool);
dns_item_t *dns_pool_add_item(dns_pool_t *pool);
dns_item_t *dns_pool_find_item(dns_pool_t *pool,  const char *addr_name);
void dns_pool_fin(dns_pool_t *pool);
void dns_add(dns_pool_t *pool, const char *addr_name, uint32_t ip__net_order, int timeout);
#define DNS_NOT_FOUND 0
uint32_t dns_find(dns_pool_t *pool, const char *addr_name, dns_find_callback_f *callback, node_fd_t *node);
void dns_time_tick(dns_pool_t *pool, int time_tick);

size_t build_request_name(char *dest, const char *addr_name);
void build_request_name_reverse(char *dest, const char *request_name, size_t *p_name);
size_t build_request(char *dest, const char *addr_name, int is_qtype_a);
int parser_reply(char *reply, char *question_name_out, uint32_t *ip_neto_out, uint32_t *ttl_out);
void dns_request(int fd, uint32_t *dns_servers, size_t dns_servers_size,
                 dns_pool_t *pool, const char *addr_name,
                 dns_find_callback_f *callback, node_fd_t *node);
void dns_recv_reply(dns_pool_t *pool, int fd);


#endif
