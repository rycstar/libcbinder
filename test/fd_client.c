#include "binder_common.h"
#include "binder_ipc.h"
#include "binder_io.h"
#include <sys/time.h>
#include <fcntl.h>


int main(int argc, char * argv[]){
    struct timeval tpstart,tpend;
    long int v_sec = 0;
    tIpcThreadInfo *ti = NULL;
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    tBinderIo bio, msg;
    char buf[16] = {0};
    int fd = 0, read_len, i;

    if(argc < 2){
        printf("Usage : fd_client <filepath>\n");
        return -1;
    }
    uint32_t fd_handle = binder_get_service("fv.fd.service");

    if(fd_handle == 0){
        printf("get fd handle fail...exit!\n");
        return -1;
    }

    fd = open(argv[1], O_RDONLY);
    if(fd <= 0) return -1;

    
    ti = binder_get_thread_info();
    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);

    binder_io_append_string(&bio, "fd_config_file");
    binder_io_append_fd(&bio, fd);

    gettimeofday(&tpstart,NULL);
    memset(&msg,0,sizeof(msg));
    if(BINDER_STATUS_OK == binder_cmd_sync_call(ti,&bio,&msg,fd_handle,0)){
        /*parse and free buffer*/
        binder_cmd_freebuf(ti, msg.data0);
    }
    gettimeofday(&tpend,NULL);
    v_sec = 1000000 *(tpend.tv_sec - tpstart.tv_sec) + (tpend.tv_usec - tpstart.tv_usec);
    printf("binder sync call use time %ld(us)\n",v_sec);

    read_len = read(fd, buf, sizeof(buf));
    printf("client read fd buf:\n");
    for(i = 0; i < read_len; i++)
            printf("0x%02x\t",buf[i]);
    printf("\n");

    printf("client close fd:%d\n",close(fd));
    
    binder_cmd_release(ti,fd_handle);
    flush_commands(ti);
    binder_threads_shutdown();    

    return 0;
}

