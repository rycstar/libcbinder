#ifndef __BINDER_HAL__
#define __BINDER_HAL__

#include "binder_common.h"
#include "binder_io.h"
/*
* the state of binder device, include fd, mapped addr, and map_size.
* mapped addr will not be used normally as the binder driver have translated the kernel addr into process addr in binder transaction.
* we only need to free it if we need to close the binder device.
*/
typedef struct binder_state{
    int fd;
    void *mapped;
    size_t map_size;
}tBinderState;

/*
* default size is (1M-2*page_size) in Android But I think it is too large for embeded linux device.
* One process should open binder device only once. so we will use a atomic parameter to control it.
*/
tBinderState * binder_open(const char * path, size_t size);


/*
* close the binder device and ummap the data
*/
void binder_close(tBinderState * bs);


/*
* claim to be a context manager service.
*/

int binder_request_context_manager(tBinderState * bs);

/*
* set max threads for binder
*/
int binder_set_max_threads(tBinderState * bs, size_t t_num);


/*
* set rw data into 'struct binder_write_read' for driver format, then talk with driver
* if write only , set read to NULL
* if read only , set write to NULL
*/
int binder_write_read(tBinderState * bs, tBinderBuf * w, tBinderBuf * r);
#endif
