#include "binder_common.h"
#include "binder_ipc.h"
#include "binder_io.h"

#define FD_SERVICE_NAME "fv.fd.service"


int fd_on_transact(uint32_t code, tBinderIo * msg, tBinderIo * reply, uint32_t flag){
    char * name = NULL;
    char buf[8] = {0};
    int fd = 0, i = 0, read_len;
    if(!msg || !reply) return -1;

    name = binder_io_get_string(msg, NULL);

    printf("fd_on_transact get name from client : %s code: %d\n",name, code);

    fd = binder_io_get_fd(msg, 0);

    if(!fd) return -1;

    read_len = read(fd, buf, sizeof(buf));

    printf("fd service read buf:\n");
    for(i = 0; i < read_len; i++)
        printf("0x%02x\t",buf[i]);
    printf("service close fd:%d\n",close(fd));
    binder_io_append_uint32(reply, 0);

    return 0;
}

tBinderService fd_service = {
    .transact_cb = fd_on_transact,
    .link_to_death_cb = NULL,
    .unlink_to_death_cb = NULL,
    .death_notify_cb = NULL,
};

int main(int argc, char * argv[]){
    binder_add_service(FD_SERVICE_NAME,&fd_service);

    binder_thread_enter_loop(1,1);

    binder_threads_shutdown();
}

