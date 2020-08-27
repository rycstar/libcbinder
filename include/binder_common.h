#ifndef __BINDER_COMMON_H__
#define __BINDER_COMMON_H__

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/prctl.h> 
#include <sys/ioctl.h>

#include "binder.h"

#define x_likely(x)  __builtin_expect(!!(x), 1)
#define x_unlikely(x)  __builtin_expect(!!(x), 0)

#define x_align4(x) ((x+3) & (~3))
#define x_align8(x) ((x+7) & (~8))

#define DEFAULT_BINDER_DEV "/dev/binder"
#define DEFAULT_BINDER_MAP_SIZE (128*1024)

#define DEFAULT_BINDER_IOBUF_SIZE 128
#define DEFAULT_OFFSET_LIST_SIZE 4

#define DEFAULT_SRV_MANAGER_NAME "FV.Cbinder.ServiceManager"
#define DEFAULT_SRV_MANAGER_HANDLE 0
#define INVALID_STRING_TAG 0xFFFFFFFF

#define NO_ERROR 0
#define DEFAULT_BINDER_THREAD_NUM 2

/*for Memory Optimize, reduce 2 page
* Detail: the commit of binder in Google
*Modify the binder to request 1M - 2 pages instead of 1M.  The backing store
    in the kernel requires a guard page, so 1M allocations fragment memory very
    badly.  Subtracting a couple of pages so that they fit in a power of
    two allows the kernel to make more efficient use of its virtual address space.
*/
#define BINDER_VM_SIZE ((128*1024) - sysconf(_SC_PAGE_SIZE)*2)
//#define BINDER_VM_SIZE (128*1024) /*128k*/

enum {
    PING_TRANSACTION  = B_PACK_CHARS('_','P','N','G'),
    SVC_MGR_GET_SERVICE = 1,
    SVC_MGR_CHECK_SERVICE,
    SVC_MGR_ADD_SERVICE,
    SVC_MGR_LIST_SERVICES,
};









#endif
