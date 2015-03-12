#include "dns_pool.h"
#include "epoll_loop.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/inet.h>

static uint32_t hash_str(const char *str){
    uint32_t ret = 0;
    while(*str != '\0'){
        ret = (ret<<4)^((uint8_t)*str);
        ret ^= ret>>16;
        str++;
    }
    return ret;
}


void dns_find_callback_item_init(dns_find_callback_item_t *item, dns_find_callback_f *func, node_fd_t *node){
    item->func = func;
    item->node = node;
}

void dns_find_callback_item_close(dns_find_callback_item_t *item){
    item->func(item->node, NULL, 0);
}

void dns_item_init(dns_item_t *item, const char *addr_name, int timeout){
    size_t addr_name_size;
    addr_name_size = strlen(addr_name);
    
    item->hash_key = hash_str(addr_name);
    item->addr_name = (char*)malloc(addr_name_size+1);
    strcpy(item->addr_name, addr_name);
    item->ip__net_order = DNS_NOT_FOUND;
    item->timeout = timeout;
    item->livetime = 0;
    item->find_cb_list = NULL;
    item->find_cb_list_size = 0;
    item->find_cb_list_maxsize = 0;
}

void dns_item_fin(dns_item_t *item){
    size_t i;
    for(i=0; i<item->find_cb_list_size; i++){
        dns_find_callback_item_close(&item->find_cb_list[i]);
    }
    free(item->find_cb_list);
    free(item->addr_name);
}

void dns_item_add_callback(dns_item_t *item, dns_find_callback_f *func, node_fd_t *node){
    dns_find_callback_item_t *new_cb_list;
    if(item->find_cb_list_size+1 >= item->find_cb_list_maxsize){
        if(item->find_cb_list_maxsize <= 0){
            item->find_cb_list_maxsize = 4;
        }
        else{
            item->find_cb_list_maxsize *= 2;
        }
        new_cb_list = (dns_find_callback_item_t*)malloc(sizeof(dns_find_callback_item_t)*item->find_cb_list_maxsize);
        memcpy(new_cb_list, item->find_cb_list, item->find_cb_list_size*sizeof(dns_find_callback_item_t));
        free(item->find_cb_list);
        item->find_cb_list = new_cb_list;
    }
    item->find_cb_list[item->find_cb_list_size].func = func;
    item->find_cb_list[item->find_cb_list_size].node = node;
    item->find_cb_list_size++;
}

void dns_item_notify_all(dns_item_t *item){
    size_t i;
    dns_find_callback_f *callback;
    node_fd_t *node;
    for(i=0; i<item->find_cb_list_size; i++){
        callback = item->find_cb_list[i].func;
        node = item->find_cb_list[i].node;
        callback(node, item->addr_name, item->ip__net_order);
    }
    free(item->find_cb_list);
    item->find_cb_list = NULL;
    item->find_cb_list_size = 0;
    item->find_cb_list_maxsize = 0;
}

void dns_pool_init(dns_pool_t *pool){
    pool->items = NULL;
    pool->items_size = 0;
    pool->items_maxsize = 0;
}

dns_item_t *dns_pool_add_item(dns_pool_t *pool){
    dns_item_t *new_items;
    if(pool->items_size+1 >= pool->items_maxsize){
        if(pool->items_maxsize <= 0){
            pool->items_maxsize = 4;
        }
        else{
            pool->items_maxsize *= 2;
        }
        new_items = (dns_item_t*)malloc(sizeof(dns_item_t)*pool->items_maxsize);
        memcpy(new_items, pool->items, pool->items_size*sizeof(dns_item_t));
        free(pool->items);
        pool->items = new_items;
    }
    pool->items_size++;
    return &pool->items[pool->items_size-1];
}

dns_item_t *dns_pool_find_item(dns_pool_t *pool,  const char *addr_name){
    size_t i;
    uint32_t hash_key;
    hash_key = hash_str(addr_name);
    for(i=0; i<pool->items_size; i++){
        if(pool->items[i].hash_key != hash_key){continue;}
        if(strcmp(pool->items[i].addr_name, addr_name) != 0){continue;}
        return &pool->items[i];
    }
    return NULL;
}

void dns_pool_fin(dns_pool_t *pool){
    size_t i;
    for(i=0; i<pool->items_size; i++){
        dns_item_fin(&pool->items[i]);
    }
    free(pool->items);
}

void dns_add(dns_pool_t *pool, const char *addr_name, uint32_t ip__net_order, int timeout){
    dns_item_t *item = dns_pool_find_item(pool, addr_name);
    if(item == NULL){
        item = dns_pool_add_item(pool);
        dns_item_init(item, addr_name, timeout);
        item->ip__net_order = ip__net_order;
    }
    else{
        item->ip__net_order = ip__net_order;
        item->timeout = timeout;
        item->livetime = 0;
        dns_item_notify_all(item);
    }
}

uint32_t dns_find(dns_pool_t *pool, const char *addr_name, dns_find_callback_f *callback, node_fd_t *node){
    dns_item_t *item = dns_pool_find_item(pool, addr_name);
    if(item == NULL){
        item = dns_pool_add_item(pool);
        dns_item_init(item, addr_name, 30);
        dns_item_add_callback(item, callback, node);
    }
    else{
        if(item->ip__net_order == 0){
            dns_item_add_callback(item, callback, node);
        }
        else{
            callback(node, item->addr_name, item->ip__net_order);
            return item->ip__net_order;
        }
    }
    return DNS_NOT_FOUND;
}

void dns_time_tick(dns_pool_t *pool, int time_tick){
    size_t i;
    size_t p;
    
    p = 0;
    for(i=0; i<pool->items_size; i++){
        // it is solved dns item
        pool->items[i].livetime += time_tick;
        if(pool->items[i].livetime > pool->items[i].timeout){
            dns_item_notify_all(&pool->items[i]);
            dns_item_fin(&pool->items[i]);
        }
        else{
            pool->items[p] = pool->items[i];
            p++;
        }
    }
}

size_t build_request_name(char *dest, const char *addr_name){
    size_t p_beg = 0;
    size_t p_end = 0;
    size_t p_dest = 0;
    while(addr_name[p_end] != '\0'){
        for(p_end=p_beg; addr_name[p_end]!='.'&&addr_name[p_end]!='\0'; p_end++){}
        dest[p_dest] = (char)(p_end - p_beg);
        p_dest++;
        strncpy(&dest[p_dest], &addr_name[p_beg], p_end - p_beg);
        p_dest += p_end - p_beg;
        p_beg = p_end + 1;
    }
    dest[p_dest] = 0x00;
    p_dest++;
    return p_dest;
}

void build_request_name_reverse(char *dest, const char *request_name, size_t *p_name){
    size_t part_size;
    while(request_name[*p_name] != 0x00){
        if((request_name[*p_name] & 0xc0) == 0xc0){
            (*p_name) += 2;
            // unsupport
            // TODO support index
            return;
        }
        else{
            part_size = (uint8_t)request_name[*p_name];
            (*p_name)++;
            memcpy(dest, &request_name[*p_name], part_size);
            *p_name += part_size;
            dest += part_size;
            *dest = '.';
            dest++;
        }
    }
    (*p_name)++;
    dest--;
    *dest = '\0';
}

size_t build_request(char *dest, const char *addr_name, int is_qtype_a){
    static char header[12] = {
        0x42, 0x42, // random id
        0x01, 0x00, // only set RA
        0x00, 0x01, // QDCOUNT = 1
        0x00, 0x00, // ANCOUNT = 0
        0x00, 0x00, // NSCOUNT = 0
        0x00, 0x00  // ARCOUNT = 0
    };
    static char qtype_a_qclass_in[4] = {
        0x00, 0x01, // QTYPE A
        0x00, 0x01, // QCLASS IN 
    };
    static char qtype_aaaa_qclass_in[4] = {
        0x00, 0x1c, // QTYPE AAAA
        0x00, 0x01, // QCLASS IN 
    };
    size_t request_name_size;
    
    memcpy(dest, header, 12);
    dest += 12;
    request_name_size = build_request_name(dest, addr_name);
    dest += request_name_size;
    if(is_qtype_a){
        memcpy(dest, qtype_a_qclass_in, 4);
    }
    else{
        memcpy(dest, qtype_aaaa_qclass_in, 4);
    }
    dest += 4;
    return request_name_size + 16;
}

int parser_reply(char *reply, char *question_name_out, uint32_t *ip_neto_out, uint32_t *ttl_out){
    // ip is net-order
    char buff[1024];
    int qcount;
    int acount;
    size_t name_size;
    size_t i;
    uint16_t rdlength;
    uint16_t a_type;
    uint16_t a_class;
    
    qcount = (((uint8_t)reply[4])<<8) | ((uint8_t)reply[5]);
    acount = (((uint8_t)reply[6])<<8) | ((uint8_t)reply[7]);
    reply += 12; // header_size
    for(i=0; i<(size_t)qcount; i++){
        name_size = 0;
        build_request_name_reverse(question_name_out, reply, &name_size);
        printf("[debug] get dns: %s\n", question_name_out);
        reply += name_size + 4;
    }
    for(i=0; i<(size_t)acount; i++){
        name_size = 0;
        build_request_name_reverse(buff, reply, &name_size);
        reply += name_size;
        
        a_type = (uint16_t)((((uint8_t)reply[0])<<8)
                           |(((uint8_t)reply[1])));
        
        a_class = (uint16_t)((((uint8_t)reply[2])<<8)
                            |(((uint8_t)reply[3])));
        
        *ttl_out = (uint32_t)((((uint8_t)reply[4])<<24)
                             |(((uint8_t)reply[5])<<16)
                             |(((uint8_t)reply[6])<<8)
                             |(((uint8_t)reply[7])));
        
        rdlength = (uint16_t)((((uint8_t)reply[8])<<8)
                             |(((uint8_t)reply[9])));
        
        if(a_type!=1 || a_class!=1){
            reply += rdlength + 10;
            continue;
        }
        
        *ip_neto_out = (uint32_t)((((uint8_t)reply[10]))
                                 |(((uint8_t)reply[11])<<8)
                                 |(((uint8_t)reply[12])<<16)
                                 |(((uint8_t)reply[13])<<24));
                
//         printf("[debug] get ip: %s\n", inet_ntoa(*(struct in_addr*)ip_neto_out));
        return 0;
    }
    return -1;
}

void dns_request(int fd, uint32_t *dns_servers, size_t dns_servers_size,
                 dns_pool_t *pool, const char *addr_name,
                 dns_find_callback_f *callback, node_fd_t *node)
{
    char buff[1024];
    size_t i;
    size_t request_size;
    struct sockaddr_in sock_addr;
    
    if(inet_addr(addr_name) != INADDR_NONE){
        callback(node, addr_name, inet_addr(addr_name));
    }
    if(pool!=NULL && dns_find(pool, addr_name, callback, node) != 0){
        return;
    }
    bzero(&sock_addr, sizeof(sock_addr));
    request_size = build_request(buff, addr_name, 1);
    buff[0] = (char)(rand()&0xff);
    buff[1] = (char)(rand()&0xff);
    
    printf("[dns] request: %s\n", addr_name);
    for(i=0; i<dns_servers_size; i++){
        sock_addr.sin_family = AF_INET;
        sock_addr.sin_port = htons(53);
        sock_addr.sin_addr.s_addr = dns_servers[i];
        sendto(fd, buff, request_size, 0, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    }
}

void dns_recv_reply(dns_pool_t *pool, int fd){
    char buff[1024];
    char addr_name[1024];
    int ret;
    uint32_t ip__net_order;
    uint32_t ttl;
    ssize_t recv_len;
    struct sockaddr_in sock_addr;
    size_t sock_len;
    
    bzero(&sock_addr, sizeof(sock_addr));
    sock_len = sizeof(sock_addr);
    for(;;){
        recv_len = recvfrom(fd, buff, 1024, 0, (struct sockaddr*)&sock_addr, (socklen_t*)&sock_len);
        if(recv_len<=0){
            break;
        }
        if(recv_len<=12){
            // zero ACOUNT
            printf("[waring] [dns] recv error size: %d\n", (int)recv_len);
            break;
        }
        ret = parser_reply(buff, addr_name, &ip__net_order, &ttl);
        if(ret>=0 && pool!=NULL){
            dns_add(pool, addr_name, ip__net_order, (int)ttl);
        }
    }
}
