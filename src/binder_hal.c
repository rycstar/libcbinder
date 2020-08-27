#include <stdatomic.h>
#include <pthread.h>
#include <sys/mman.h>

#include "binder_hal.h"

/*************************static variables******************************/
static atomic_uint g_bs_refcount = ATOMIC_VAR_INIT(0);
static tBinderState g_bs;
static pthread_mutex_t g_binder_mutex = PTHREAD_MUTEX_INITIALIZER;
/***********************************************************************/

/*
* default size is (1M-2*page_size) in Android But I think it is too large for embeded linux device.
* One process should open binder device only once. so we will use a atomic parameter to control it.
*/
tBinderState * binder_open(const char * path, size_t size){
    uint32_t bs_ref = 0;
    struct binder_version vers;
    tBinderState * bs = &g_bs;

    pthread_mutex_lock(&g_binder_mutex);

    bs_ref = atomic_load(&g_bs_refcount);
    if(bs_ref == 0){
        memset(bs, 0, sizeof(tBinderState));

        bs->fd = open(path, O_RDWR | O_CLOEXEC);
        if(bs->fd <= 0) goto open_fail;

        if ((ioctl(bs->fd, BINDER_VERSION, &vers) == -1) ||
            (vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION)) {
                printf("binder: driver version differs from user space\n");
                goto ioctl_fail;
        }

        binder_set_max_threads(bs, DEFAULT_BINDER_THREAD_NUM);
            
        bs->map_size = size;
        if(bs->map_size == 0) bs->map_size = BINDER_VM_SIZE;

        /*wo should check the map_size here,to do*/
        bs->mapped = mmap(NULL, bs->map_size, PROT_READ, MAP_PRIVATE, bs->fd, 0);

        if(bs->mapped == MAP_FAILED) {
            printf("binder: cannot map device\n");
            goto ioctl_fail;
        }       
    }

    atomic_fetch_add(&g_bs_refcount, 1);
    pthread_mutex_unlock(&g_binder_mutex);
    return bs;
ioctl_fail:
    if(bs->fd > 0) close(bs->fd);   
open_fail:
    pthread_mutex_unlock(&g_binder_mutex);
    return NULL;

}


/*
* close the binder device and ummap the data
*/
void binder_close(tBinderState * bs){
    uint32_t bs_ref = 0;
    
    if(!bs) return;

    pthread_mutex_lock(&g_binder_mutex);

    bs_ref = atomic_load(&g_bs_refcount);
    if(bs_ref > 1){
        atomic_fetch_sub(&g_bs_refcount, 1);
    }else if(bs->fd > 0){
        /*unmap and close fd*/
        munmap(bs->mapped, bs->map_size);
        close(bs->fd);
        bs->fd = 0;
    }
    
    pthread_mutex_unlock(&g_binder_mutex);

}


/*
* claim to be a context manager service.
*/

int binder_request_context_manager(tBinderState * bs){
    if(bs && bs->fd > 0){
        return ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
    }
    return -1;
}

/*
* set max threads for binder
*/
int binder_set_max_threads(tBinderState * bs, size_t t_num){
    unsigned int num = t_num;
    if(bs && bs->fd > 0){
        return ioctl(bs->fd, BINDER_SET_MAX_THREADS, &num);
    }
    return -1;
}


/*
* set rw data into 'struct binder_write_read' for driver format, then talk with driver
* if write only , set read to NULL
* if read only , set write to NULL
*/
int binder_write_read(tBinderState * bs, tBinderBuf * w, tBinderBuf * r){
    struct binder_write_read bwr;
    int res = -1;

    if(bs && bs->fd > 0){
        memset(&bwr, 0 ,sizeof(bwr));
        
        if(w){
            bwr.write_buffer = (binder_uintptr_t)(w->ptr + w->consumed);
            bwr.write_size = w->size - w->consumed;
        }

        if(r){
            bwr.read_buffer = (binder_uintptr_t)(r->ptr + r->consumed);
            bwr.read_size = r->size - r->consumed;
        }

        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
        if(res >= 0){
            if(w) w->consumed += bwr.write_consumed;
            if(r) r->consumed += bwr.read_consumed;
        }
    }
    return res;
}

