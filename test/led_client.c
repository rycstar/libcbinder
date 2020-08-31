#include "binder_common.h"
#include "binder_ipc.h"
#include "binder_io.h"
#include <sys/time.h>


int main(int argc, char * argv[]){
    struct timeval tpstart,tpend;
    long int v_sec = 0;
    tIpcThreadInfo *ti = binder_get_thread_info();
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    tBinderIo bio, msg;
    char * s_name = "led_handfree";

    uint32_t led_handle = binder_get_service("fv.led.service");
    uint32_t key_handle = binder_get_service("fv.key.service");
    uint32_t target;
    if(led_handle == 0 || key_handle == 0){
        printf("get handle fail...exit!\n");
        return -1;
    }

    if(argc >= 2) s_name= argv[1];

    if(strstr(s_name,"led")) target = led_handle;
    else target = key_handle;
    
    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
    gettimeofday(&tpstart,NULL);
    binder_io_append_string(&bio, s_name);
    gettimeofday(&tpend,NULL);
    v_sec = 1000000 *(tpend.tv_sec - tpstart.tv_sec) + (tpend.tv_usec - tpstart.tv_usec);
    
    
    gettimeofday(&tpstart,NULL);
    memset(&msg, 0, sizeof(msg));
    if(BINDER_STATUS_OK == binder_cmd_sync_call(ti,&bio,&msg,target,0)){
        /*parse and free buffer*/
        binder_cmd_freebuf(ti, msg.data0);
    }
    gettimeofday(&tpend,NULL);
    v_sec = 1000000 *(tpend.tv_sec - tpstart.tv_sec) + (tpend.tv_usec - tpstart.tv_usec);
    printf("binder sync call use time %ld(us)\n",v_sec);

    
    gettimeofday(&tpstart,NULL);
    memset(&msg, 0, sizeof(msg));
    if(BINDER_STATUS_OK == binder_cmd_sync_call(ti,&bio,&msg,target,1)){
        /*parse and free buffer*/
        binder_cmd_freebuf(ti, msg.data0);
        flush_commands(ti);
    }
    gettimeofday(&tpend,NULL);
    v_sec = 1000000 *(tpend.tv_sec - tpstart.tv_sec) + (tpend.tv_usec - tpstart.tv_usec);
    printf("binder sync call use time %ld(us)\n",v_sec);

    s_name = "key_handfree";
    if(argc >= 3) s_name= argv[2];
    if(strstr(s_name,"led")) target = led_handle;
    else target = key_handle;
    
    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
    binder_io_append_string(&bio, s_name);
    
    gettimeofday(&tpstart,NULL);
    memset(&msg, 0, sizeof(msg));
    if(BINDER_STATUS_OK == binder_cmd_sync_call(ti,&bio,&msg,target,0)){
        /*parse and free buffer*/
        binder_cmd_freebuf(ti, msg.data0);
    }
    gettimeofday(&tpend,NULL);
    v_sec = 1000000 *(tpend.tv_sec - tpstart.tv_sec) + (tpend.tv_usec - tpstart.tv_usec);
    printf("binder async call use time %ld(us)\n",v_sec);

    gettimeofday(&tpstart,NULL);
    memset(&msg, 0, sizeof(msg));
    if(BINDER_STATUS_OK == binder_cmd_async_call(ti,&bio,&msg,target,1)){
        /*async call don't have reply message*/
        //binder_cmd_freebuf(ti, msg.data0);
    } 
    flush_commands(ti);
    gettimeofday(&tpend,NULL);
    v_sec = 1000000 *(tpend.tv_sec - tpstart.tv_sec) + (tpend.tv_usec - tpstart.tv_usec);
    printf("binder async call use time %ld(us)\n",v_sec);


    sleep(3);
    binder_cmd_release(ti,led_handle);
    binder_cmd_release(ti,key_handle);
    flush_commands(ti);
    binder_threads_shutdown();    
    return 0;
}

