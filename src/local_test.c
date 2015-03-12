#include "s5_local.h"


int main(int argc, char **argv){
    node_fd_t *node_pipe;
    (void)argc;(void)argv;
    loop_manager_t manager;
    loop_manager_init(&manager);
    
    node_pipe = local_create_pipe(&manager, "127.0.0.1", 9999);
    local_create_listener(&manager, node_pipe, 8888);
    
    while(manager.root != NULL){
        loop_manager_poll(&manager, 100);
    }
    
    loop_manager_fin(&manager);
    return 0;
}
