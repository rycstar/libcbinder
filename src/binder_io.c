#include "binder_common.h"
#include "binder_io.h"


/***********************binder io write functions***********************/

static void * binder_io_alloc(tBinderIo * bio, size_t size){
    void * ptr = NULL;

    if(x_likely(bio)){
        size = x_align4(size);
        if(size <= bio->data_avail){
            ptr = bio->data;
            bio->data += size;
            bio->data_avail -= size;
        }
    }
    return ptr;
}

static struct flat_binder_object * binder_io_alloc_obj(tBinderIo * bio){
    struct flat_binder_object * ptr = NULL;
    if(x_likely(bio)){
        if(bio->offs_avail <= 0) return ptr;

        ptr = (struct flat_binder_object *)binder_io_alloc(bio, sizeof(*ptr));

        if(!ptr) return ptr;

        /*the offset value is obj point to data0 point*/
        *(bio->offs) = ((char *)ptr) - ((char *)bio->data0);
        bio->offs_avail--;
        bio->offs++;
    }
    return ptr;
}


void binder_buf_init(tBinderBuf *buf, char * val, size_t val_size, int reset){
    if(x_likely(buf) && x_likely(val)){
        buf->ptr = val;
        buf->consumed = 0;
        buf->size = val_size;
        if(reset) memset(val, 0, val_size);
    }
}


uint32_t binder_buf_get_next_cmd(tBinderBuf * buf){
    uint32_t cmd = 0 ;
    size_t remaining = buf->size - buf->consumed;
    
    if(remaining >= sizeof(cmd)){
        cmd = *(uint32_t *)(buf->ptr + buf->consumed);
        if(remaining >= sizeof(cmd) + _IOC_SIZE(cmd)){
            buf->consumed += sizeof(cmd) + _IOC_SIZE(cmd);
            return cmd;
        }else{
            /*in this case ,we need to release the buf ro not???*/
            printf("Error: buffer cmd without excepted data size!!!, clear it ?\n");
            buf->consumed = remaining; /*clear it*/
            return 0;
        }
    }

    return cmd;
}

void binder_buf_move_buffer(tBinderBuf *b_src_buf, tBinderBuf * consumed_buf){
    size_t unprocess_size = 0;
    unprocess_size = consumed_buf->size - consumed_buf->consumed;
    memmove(b_src_buf->ptr, b_src_buf->ptr + consumed_buf->consumed, unprocess_size);
    b_src_buf->consumed = unprocess_size;
}


int binder_io_init(tBinderIo * bio, void * data, size_t data_size, size_t offset_list_size){
    int ret = -1;
    size_t offsets_byte_len = 0;

    if(x_likely(bio) && x_likely(data)){
        memset(bio, 0, sizeof(tBinderIo));
        memset(data, 0, sizeof(data_size));
        offsets_byte_len = offset_list_size * sizeof(binder_size_t);

        if(offsets_byte_len > (data_size >> 1)){
            printf("%s, Error: offset byte len is too big, please alloc more data for binder io\n",__func__);
            return ret;
        }
        
        bio->offs = bio->offs0 = data;
        bio->offs_avail = offset_list_size;

        bio->data = bio->data0 = data + offsets_byte_len;
        bio->data_avail = (data_size - offsets_byte_len);
        ret = 0;
    }

    return ret;
}

int binder_io_append_uint32(tBinderIo * bio, uint32_t val){
    uint32_t * ptr;
    int ret = -1;
    if(x_likely(bio)){
        ptr = (uint32_t *)binder_io_alloc(bio, sizeof(* ptr));
        if(ptr){
            *ptr = val;
            ret = 0;
        }
    }
    return ret;
}

int binder_io_append_string(tBinderIo * bio, const char * str){
    int ret = -1;
    uint32_t * ptr;
    uint32_t len = 0;

    if(x_likely(bio)){
        if(str){
            len = strlen(str);
            ptr = (uint32_t*) binder_io_alloc(bio, sizeof(uint32_t) + (len + 1));
            if(ptr){
                *ptr = len;
                memcpy((uint8_t *)(++ptr), str, len);
                ret = 0;
            }else{
                ret = binder_io_append_uint32(bio, INVALID_STRING_TAG);
            }
        }else{
            ret = binder_io_append_uint32(bio, INVALID_STRING_TAG);
        }
    }
    return ret;
}

int binder_io_append_fd(tBinderIo * bio, int fd){
    int ret = -1;
    struct binder_fd_object *fd_obj = NULL;

    if(x_likely(bio)){
        /*
            the memory area of binder_fd_object is same as flat_binder_object
        */
        fd_obj = (struct binder_fd_object *)binder_io_alloc_obj(bio);
        if(fd_obj){
            fd_obj->pad_flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
            fd_obj->hdr.type = BINDER_TYPE_FD;
            fd_obj->fd = fd;
            fd_obj->cookie = 0;
            ret = 0;
        }
    }
    return ret;
}

/*
* This function is for local obj, 
* if the obj is a inner-process object, use this function
* is the obj is a reference object of other process, use binder_io_append_ref
*/
int binder_io_append_obj(tBinderIo * bio, void * ptr){
    int ret = -1;
    struct flat_binder_object *flat_obj = NULL;
    
    if(x_likely(bio)){
        flat_obj = binder_io_alloc_obj(bio);
        if(flat_obj){
            flat_obj->flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
            flat_obj->hdr.type = BINDER_TYPE_BINDER;
            flat_obj->binder = (binder_uintptr_t)ptr;
            flat_obj->cookie = 0;
            ret = 0;
        }
    }
    
    return ret;
}

/*
* This function is for remote obj.
* if the obj is a inner-process object, use binder_io_append_obj
* is the obj is a reference object of other process, use this function
*/
int binder_io_append_ref(tBinderIo * bio,  uint32_t hdl){
    int ret = -1;
    struct flat_binder_object *flat_obj = NULL;
    
    if(x_likely(bio)){
        flat_obj = binder_io_alloc_obj(bio);
        if(flat_obj){
            flat_obj->flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
            flat_obj->hdr.type = BINDER_TYPE_HANDLE;
            flat_obj->handle = hdl;
            flat_obj->cookie = 0;
            ret = 0;
        }
    }
    
    return ret;
}

/***********************binder io read functions***********************/

static void * binder_io_get(tBinderIo * bio, size_t size){
    void * ptr = NULL;

    if(x_likely(bio)){
        size = x_align4(size);
        if(bio->data_avail < size){
            bio->data_avail = 0;
        }else{
            ptr = bio->data;
            bio->data += size;
            bio->data_avail -= size;
        }
    }

    return ptr;
}


static struct flat_binder_object * binder_io_obj_get(tBinderIo *bio, uint32_t offset_idx){
    struct flat_binder_object *flat_obj = NULL;
    size_t cur_off = bio->data - bio->data0;
    if(offset_idx < bio->offs_avail && cur_off == bio->offs[offset_idx]){
        flat_obj = (struct flat_binder_object *)binder_io_get(bio, sizeof(*flat_obj));
    }

    return flat_obj;
}

/*
* translate the transaction data into binder io format
* use binder_io_get functions to get the parameters.
*/
void binder_io_init_from_txn(tBinderIo *bio, struct binder_transaction_data *txn){
    if(x_likely(bio) && x_likely(txn)){
        bio->data = bio->data0 = (char *)(intptr_t)txn->data.ptr.buffer;
        bio->offs = bio->offs0 = (binder_size_t *)(intptr_t)txn->data.ptr.offsets;
        bio->data_avail = txn->data_size;
        bio->offs_avail = txn->offsets_size / sizeof(binder_size_t);
    }
}


/*
* translate the binder io data into transaction format
*/
void binder_io_to_txn(tBinderIo *bio, struct binder_transaction_data *txn){
    if(x_likely(bio) && x_likely(txn)){
        txn->data_size = bio->data - bio->data0;
        txn->offsets_size = ((char *)bio->offs) - ((char *)bio->offs0);
        txn->data.ptr.buffer = (uintptr_t)bio->data0;
        txn->data.ptr.offsets = (uintptr_t)bio->offs0;
    }
}


uint32_t binder_io_get_uint32(tBinderIo *bio){
    uint32_t *ptr = binder_io_get(bio, sizeof(uint32_t));
    return ptr ? *ptr : 0;
}

char* binder_io_get_string(tBinderIo *bio, size_t *sz){
    /*first get the len of string*/
    uint32_t s_len = binder_io_get_uint32(bio);

    if(s_len == INVALID_STRING_TAG) return NULL;

    if(sz) *sz = s_len;
    return (char*)binder_io_get(bio, s_len + 1);
}

uint32_t binder_io_get_fd(tBinderIo *bio, uint32_t offset_idx){
    uint32_t fd = 0;

    struct flat_binder_object *flat_obj = NULL;
    flat_obj = binder_io_obj_get(bio, offset_idx);
    if(flat_obj && flat_obj->hdr.type == BINDER_TYPE_FD){
        fd = flat_obj->handle;
    }
    return fd;
}

binder_uintptr_t binder_io_get_obj(tBinderIo *bio, uint32_t offset_idx){
    binder_uintptr_t binder = 0;

    struct flat_binder_object *flat_obj = NULL;
    flat_obj = binder_io_obj_get(bio, offset_idx);
    if(flat_obj && flat_obj->hdr.type == BINDER_TYPE_BINDER){
        binder = flat_obj->binder;
    }
    return binder;
}

uint32_t binder_io_get_ref(tBinderIo *bio, uint32_t offset_idx){
    uint32_t handle = 0;

    struct flat_binder_object *flat_obj = NULL;
    flat_obj = binder_io_obj_get(bio, offset_idx);
    if(flat_obj && flat_obj->hdr.type == BINDER_TYPE_HANDLE){
        handle = flat_obj->handle;
    }
    return handle;
}



