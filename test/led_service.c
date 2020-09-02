#include "binder_common.h"
#include "binder_ipc.h"
#include "binder_io.h"

#define LED_SERVICE_NAME "fv.led.service"
#define KEY_SERVICE_NAME "fv.key.service"


int led_on_transact(uint32_t code, tBinderIo * msg, tBinderIo * reply, uint32_t flag){
    char * name = NULL;
    if(!msg || !reply) return -1;

    name = binder_io_get_string(msg, NULL);

    printf("led_on_transact get msg from client:%s\n",name);
    
    binder_io_append_uint32(reply, 0);
    
    return 0;
}


int key_on_transact(uint32_t code, tBinderIo * msg, tBinderIo * reply, uint32_t flag){
    char * name = NULL;
    if(!msg || !reply) return -1;

    name = binder_io_get_string(msg, NULL);
    printf("key_on_transact get msg from client:%s\n",name);
    binder_io_append_uint32(reply, 0);
    
    return 0;
}



tBinderService led_service = {
    .transact_cb = led_on_transact,
    .link_to_death_cb = NULL,
    .unlink_to_death_cb = NULL,
    .death_notify_cb = NULL,
};

tBinderService key_service = {
    .transact_cb = key_on_transact,
    .link_to_death_cb = NULL,
    .unlink_to_death_cb = NULL,
    .death_notify_cb = NULL,
};



int main(int argc, char * argv[]){
    char name[64] = {0};
    printf("res:%d\n",binder_add_service(LED_SERVICE_NAME, &led_service));
    
    printf("res:%d\n",binder_add_service(KEY_SERVICE_NAME, &key_service));

    binder_list_service(0, name, sizeof(name));
    printf("binder service service 0 : %s\n", name);
    binder_list_service(1, name, sizeof(name));
    printf("binder service service 1 : %s\n", name);
    binder_thread_enter_loop(1,1);

    binder_threads_shutdown();
    return 0;
}
