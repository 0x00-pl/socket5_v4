#include "s5_remote.h"


int main(int argc, char **argv){
    (void)argc;(void)argv;
    loop_manager_t manager;
    loop_manager_init(&manager);
    
    remote_create_listener(&manager, 9999);
    
    while(manager.root != NULL){
        loop_manager_poll(&manager, 100);
    }
    
    loop_manager_fin(&manager);
    return 0;
}
