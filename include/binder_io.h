#ifndef __BINDER_IO_H__
#define __BINDER_IO_H__

#include "binder_common.h"

/*
*
*
buffer point
|<-offset avail->|<-------------data avail------------->| 
|-------------------------------------------------------|
|   offset area  |             data area                |
|-------------------------------------------------------|

*offs is a array of (binder_size_t), and offs_avail is the number of binder_size_t
so we need to use sizeof(binder_size_t) to calc the offs_avail value

Usually, offs0 is same as buffer, data0 = buffer + sizeof(binder_size_t) * array_size(offset)
*
*
*/
typedef struct binder_io
{
    char *data;            /* pointer to read/write from */
    binder_size_t *offs;   /* array of offsets */
    size_t data_avail;     /* bytes available in data buffer */
    size_t offs_avail;     /* entries available in offsets array */

    char *data0;           /* start of data buffer */
    binder_size_t *offs0;  /* start of offsets buffer */
}tBinderIo;

/*
* buffer for binder write and read, the write_size and read_size will be store into consumed var.
*/
typedef struct binder_buf{
    char * ptr;
    uint32_t consumed;
    uint32_t size;
}tBinderBuf;

/***********************binder io write functions***********************/
void binder_buf_init(tBinderBuf *buf, char * val, size_t val_size, int reset);

uint32_t binder_buf_get_next_cmd(tBinderBuf * buf);

void binder_buf_move_buffer(tBinderBuf *b_src_buf, tBinderBuf * consumed_buf);


int binder_io_init(tBinderIo * bio, void * data, size_t data_size, size_t offset_list_size);

int binder_io_append_uint32(tBinderIo * bio, uint32_t val);

int binder_io_append_string(tBinderIo * bio, const char * str);

int binder_io_append_fd(tBinderIo * bio, int fd);


/*
* This function is for local obj, 
* if the obj is a inner-process object, use this function
* is the obj is a reference object of other process, use binder_io_append_ref
*/
int binder_io_append_obj(tBinderIo * bio, void * ptr);

/*
* This function is for remote obj.
* if the obj is a inner-process object, use binder_io_append_obj
* is the obj is a reference object of other process, use this function
*/
int binder_io_append_ref(tBinderIo * bio,  uint32_t hdl);



/***********************binder io read functions***********************/

/*
* translate the transaction data into binder io format
* use binder_io_get functions to get the parameters.
*/
void binder_io_init_from_txn(tBinderIo *bio, struct binder_transaction_data *txn);

/*
* translate the binder io data into transaction format
*/
void binder_io_to_txn(tBinderIo *bio, struct binder_transaction_data *txn);


uint32_t binder_io_get_uint32(tBinderIo *bio);

char* binder_io_get_string(tBinderIo *bio, size_t *sz);

uint32_t binder_io_get_fd(tBinderIo *bio, uint32_t offset_idx);

binder_uintptr_t binder_io_get_obj(tBinderIo *bio, uint32_t offset_idx);

uint32_t binder_io_get_ref(tBinderIo *bio, uint32_t offset_idx);


#endif
