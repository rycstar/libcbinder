#include "binder_common.h"
#include "binder_ipc.h"
#include <pthread.h>

#define BINDER_READ_BUF_SIZE 256

static int binder_execute_cmds(tIpcThreadInfo * ti, uint32_t cmd, void * data);

/*
* for transaction data, we need to notice that:
* txn data point to "another buffer", so after writed into outbuf, 
we need to talk with driver immediately if the "another buffer" in stack area. 
*/
int ti_write_outbuf(tIpcThreadInfo *ti, void * buf, size_t sz){
    if(x_unlikely(!ti) || x_unlikely(!buf)) return -1;
    if(ti->out_buf.size - ti->out_buf.consumed < sz) return -1;

    memcpy(ti->out_buf.ptr + ti->out_buf.consumed, buf, sz);
    ti->out_buf.consumed += sz;
    return 0;
}


int talk_with_driver(tIpcThreadInfo *ti,int read_flag){
    tBinderBuf *r_buf = NULL, * w_buf = NULL;
    tBinderBuf tmp_w_buf;
    int need_read = 0, unprocess_size = 0, ret = -1;
    if(x_unlikely(!ti)) return ret;
    if(x_unlikely(!ti->bs)) return ret;

    r_buf = &(ti->in_buf);
    w_buf = &(ti->out_buf);

    need_read = read_flag && r_buf->consumed == 0;

    /*set to null point if we don't need read*/
    if(!need_read) r_buf = NULL;
    /*set to null point if there is no data to write*/
    if(w_buf->consumed == 0) w_buf = NULL;
    /*return if there is no data out and can not read*/
    if(!w_buf && !r_buf) return 0; /*we should still handle the in_data in this case.*/

    if(w_buf)
        binder_buf_init(&tmp_w_buf, w_buf->ptr, w_buf->consumed, 0);

    
    if((ret = binder_write_read(ti->bs, w_buf ? &tmp_w_buf : NULL, r_buf)) >= 0){
        if(w_buf){
            unprocess_size = tmp_w_buf.size - tmp_w_buf.consumed;
            memmove(w_buf->ptr, w_buf->ptr + tmp_w_buf.consumed, unprocess_size);
            w_buf->consumed = unprocess_size;
        }
    }
    return ret;
}

void flush_commands(tIpcThreadInfo *ti){
    if(talk_with_driver(ti, 0) >= 0){
        if(ti->out_buf.consumed > 0){
            /*still have out buffer not flushed, try again*/
            talk_with_driver(ti, 0);
            if(ti->out_buf.consumed > 0){
                printf("Warning : still have command not flushed!\n");
            }
        }
    }
}

/*
* return the err code
* only NO_ERROR case we need to parse the reply
* negative number means service err code
* positive number means the binder system err code
*/
static int _binder_cmd_wait_rsp(tIpcThreadInfo *ti, tBinderIo * reply){
    int err = BINDER_STATUS_OK;
    uint32_t cmd = 0;
    tBinderBuf bbuf;
    tBinderIo tmp_reply;
    while(1){ 
        if(0 > talk_with_driver(ti, 1)){
            err = BINDER_STATUS_IO_ERROR;
            break;
        }
        binder_buf_init(&bbuf, ti->in_buf.ptr, ti->in_buf.consumed, 0);
        cmd = binder_buf_get_next_cmd(&bbuf);
        
        if(bbuf.consumed > 0){
            /*move the in buf first*/
            binder_buf_move_buffer(&(ti->in_buf), &bbuf);
        }

        if(cmd == 0){
            continue;
        }else{
            void * data = (void *)(bbuf.ptr + sizeof(cmd));
            //printf("_binder_cmd_wait_rsp cmd : %04x\n",cmd);
            switch(cmd){
                case BR_TRANSACTION_COMPLETE:
                    if(!reply){
                        err = BINDER_STATUS_OK; 
                        goto finish;
                    }
                    break;
                case BR_DEAD_REPLY:
                    err = BINDER_STATUS_FAILED;
                    goto finish;
                case BR_FAILED_REPLY:
                    err = BINDER_STATUS_DEAD_OBJECT;
                    goto finish;
                case BR_REPLY:
                    if(reply){
                        struct binder_transaction_data * txn = (struct binder_transaction_data *) data;
                        binder_io_init_from_txn(reply, txn);
                        if (txn->flags & TF_STATUS_CODE){
                            /*it's a status code, means functions call return a error code, we parse it and free the buffer*/
                            err = *((int32_t*)(txn->data.ptr.buffer));
                            binder_cmd_freebuf(ti,(void *)txn->data.ptr.buffer);
                        }
                    }else{
                        /*this case should not be come in if every one write the correct function call.
                        * we add it to avoid binder buufer not freed.
                        */
                        struct binder_transaction_data * txn = (struct binder_transaction_data *) data;
                        binder_io_init_from_txn(&tmp_reply, txn);
                        binder_cmd_freebuf(ti,(void *)txn->data.ptr.buffer);
                        continue;
                    }
                    goto finish;
                default:
                    /*execute cmd in sevice mode*/
                    err = binder_execute_cmds(ti, cmd, data);
                    break;

            }
        }
    }
finish:
    ti->txn_status = err;
    return err;
}
/*
* call the target functions according to the code
* For BC_TRANSACTION cmd type
*/
static int _binder_cmd_call(tIpcThreadInfo *ti,tBinderIo *bio, tBinderIo *reply,uint32_t target, uint32_t code, int flag){
    int ret = 0;
    uint32_t cmd = BC_TRANSACTION;
    struct binder_transaction_data txn;

    memset(&txn, 0, sizeof(txn));

    txn.target.handle = target;
    txn.code = code;
    if(flag)    txn.flags = TF_ONE_WAY;
    
    /*accept fd transfer*/
    txn.flags |= TF_ACCEPT_FDS;
    binder_io_to_txn(bio, &txn);

    ti_write_outbuf(ti, &cmd, sizeof(cmd));
    ti_write_outbuf(ti, &txn, sizeof(txn));
    if(flag){
        /*async call will receive BR_TRANSACTION_COMPLETE from kernel only*/
        ret = _binder_cmd_wait_rsp(ti, NULL);
    }else{
        /*sync call will receive  BR_TRANSACTION_COMPLETE from kernel and BR_REPLY from target*/
        ret = _binder_cmd_wait_rsp(ti,reply);

    }

    return ret;
}

int binder_cmd_sync_call(tIpcThreadInfo *ti,tBinderIo *bio, tBinderIo *msg,uint32_t target, uint32_t code){
    return _binder_cmd_call(ti, bio, msg,target,code, 0);
}

int binder_cmd_async_call(tIpcThreadInfo *ti,tBinderIo *bio, tBinderIo *msg,uint32_t target, uint32_t code){
    return _binder_cmd_call(ti, bio, msg,target,code, 1);
}

/*
* For BC_ACQUIRE
* just send data into thread out buffer, if user want to send to driver,
* call flush_commands to talk with driver.
*/
int binder_cmd_acquire(tIpcThreadInfo *ti, uint32_t target){
    int32_t tmp_cmd[2] = {0};
    //tBinderBuf bbuf;
    
    //binder_buf_init(&bbuf,(const char *)tmp_cmd, sizeof(tmp_cmd),1);
    
    tmp_cmd[0] = BC_ACQUIRE;
    tmp_cmd[1] = target;
    
    return ti_write_outbuf(ti, (void*)&tmp_cmd, sizeof(tmp_cmd));
    //return binder_write_read(ti->bs, &bbuf, NULL);
}


/*
* For BC_RELEASE
* just send data into thread out buffer, if user want to send to driver,
* call flush_commands to talk with driver.
*/

int binder_cmd_release(tIpcThreadInfo *ti, uint32_t target){
    int32_t tmp_cmd[2] = {0};
    //tBinderBuf bbuf;

    //binder_buf_init(&bbuf,(const char *)tmp_cmd, sizeof(tmp_cmd),1);
    tmp_cmd[0] = BC_RELEASE;
    tmp_cmd[1] = target;
    printf("binder cmd release handle : %08x\n", target);
    return ti_write_outbuf(ti, (void*)&tmp_cmd, sizeof(tmp_cmd));
    //return binder_write_read(ti->bs, &bbuf, NULL);
}

/*
* For BC_REQUEST_DEATH_NOTIFICATION, just write buffer into thread out buffer
*/

int binder_cmd_link_to_death(tIpcThreadInfo *ti, uint32_t target, void * death){
    struct {
        uint32_t cmd;
        struct binder_handle_cookie payload;
    } __attribute__((packed)) death_data;

    death_data.cmd = BC_REQUEST_DEATH_NOTIFICATION;
    death_data.payload.handle = target;
    death_data.payload.cookie = (binder_uintptr_t)death;
    
    return ti_write_outbuf(ti, (void*)&death_data, sizeof(death_data));
}

/*
* For BC_FREE_BUFFER
* ptr is the transaction data in struct binder_transaction_data 
* All the transaction buffer should be free after handle done!!!

* just send data into thread out buffer, if user want to send to driver,
* call flush_commands to talk with driver.

*/
int binder_cmd_freebuf(tIpcThreadInfo *ti, void * ptr){
    //tBinderBuf bbuf;
    struct _binder_cmd_freebuf{
        int32_t cmd;
        binder_uintptr_t ptr;
    }__attribute__((packed)) freebuf_cmd;

    //binder_buf_init(&bbuf,(const char *)freebuf_cmd, sizeof(freebuf_cmd));
    
    freebuf_cmd.cmd = BC_FREE_BUFFER;
    freebuf_cmd.ptr = (binder_uintptr_t)ptr;
    return ti_write_outbuf(ti, (void*)&freebuf_cmd, sizeof(freebuf_cmd));
    //return binder_write_read(ti->bs, &bbuf, NULL);
}

/********************************IPC thread related functions*************************************/
static int gHaveThreadKey = 0;
static int gShutingdown = 0;
static pthread_key_t gThreadKey = 0;
static pthread_mutex_t gThreadKeyMutex = PTHREAD_MUTEX_INITIALIZER;

static tIpcThreadInfo * init_thread_info(){
    tIpcThreadInfo * ti = NULL;

    ti = (tIpcThreadInfo *) malloc(sizeof(tIpcThreadInfo));
    if(ti){
        memset(ti, 0 ,sizeof(tIpcThreadInfo));
        ti->bs = binder_open(DEFAULT_BINDER_DEV,DEFAULT_BINDER_MAP_SIZE);
        if(!ti->bs) goto fail;
        ti->in_buf.ptr = (char * )malloc(MAX_THREAD_BUF_LEN);
        if(ti->in_buf.ptr){
            memset(ti->in_buf.ptr, 0 ,MAX_THREAD_BUF_LEN);
            ti->in_buf.size = MAX_THREAD_BUF_LEN;
        }else{
            goto fail;
        }

        ti->out_buf.ptr = (char * )malloc(MAX_THREAD_BUF_LEN);
        if(ti->out_buf.ptr){
            memset(ti->out_buf.ptr, 0 ,MAX_THREAD_BUF_LEN);
            ti->out_buf.size = MAX_THREAD_BUF_LEN;
        }else{
            goto fail;
        }
    }
    return ti;

fail:
    if(ti->bs)  binder_close(ti->bs);
    if(ti->in_buf.ptr) free(ti->in_buf.ptr);
    if(ti->out_buf.ptr) free(ti->out_buf.ptr);
    if(ti) free(ti);
    return NULL;
}

static void destory_thread_info(void * ptr){
    tIpcThreadInfo * ti = (tIpcThreadInfo *)ptr;
    if(ti){
        if(ti->in_buf.ptr) free(ti->in_buf.ptr);
        if(ti->out_buf.ptr) free(ti->out_buf.ptr);

#if 0
        if (ti->bs.fd > 0) {
            ioctl(ti->bs.fd , BINDER_THREAD_EXIT, 0);
        }
#endif        
        binder_close(ti->bs);
        free(ti);
    }
}


tIpcThreadInfo * binder_get_thread_info(){
    const pthread_key_t key = gThreadKey;
    tIpcThreadInfo * ti = NULL;
    if(gHaveThreadKey){
retry:
        ti = (tIpcThreadInfo *)pthread_getspecific(key);
        if(ti) return ti;
        if((ti = init_thread_info()) != NULL){
            pthread_setspecific(gThreadKey, ti);
        }
        return ti;
    }

    if(gShutingdown) return NULL;
    
    pthread_mutex_lock(&gThreadKeyMutex);
    if(!gHaveThreadKey){
        if(pthread_key_create(&gThreadKey, destory_thread_info) != 0){
            printf("Error: unable to create thread key.\n");
            pthread_mutex_unlock(&gThreadKeyMutex);
            return NULL;
        }
        
        gHaveThreadKey = 1;
    }
    pthread_mutex_unlock(&gThreadKeyMutex);
    goto retry;
}

/*
* this function should be called by the main thread of the process
* after all the threads exited
*/
void binder_threads_shutdown(){
    tIpcThreadInfo * ti = NULL;
    
    gShutingdown = 1;

    if(gHaveThreadKey){
        ti = (tIpcThreadInfo *)pthread_getspecific(gThreadKey);
        destory_thread_info((void *)ti);
        pthread_setspecific(gThreadKey, NULL);
        pthread_key_delete(gThreadKey);
        gHaveThreadKey = 0;
    }
}


/************************************binder serives related functions*****************************************/

int binder_execute_cmds(tIpcThreadInfo * ti, uint32_t cmd, void * data){
    int32_t result = BINDER_STATUS_OK;
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    tBinderIo bio,reply;
    tBinderService *b_srv = NULL;
    struct binder_ptr_cookie* ptr = NULL;
    binder_uintptr_t __ptr,__cookie;
    int32_t r_cmd = 0;
    switch(cmd){
        case BR_ERROR:
            result = *((int32_t *)data);
            break;
        case BR_OK:
            break;
        case BR_ACQUIRE:
            {
                ptr = (struct binder_ptr_cookie*)data;
                __ptr = (binder_uintptr_t)(ptr->ptr);
                __cookie = (binder_uintptr_t)(ptr->cookie);
                /*do sth to increase ref*/
                r_cmd = BC_ACQUIRE_DONE;
                ti_write_outbuf(ti, &r_cmd, sizeof(r_cmd));
                ti_write_outbuf(ti, (void *)&__ptr, sizeof(__ptr));
                ti_write_outbuf(ti, (void *)&__cookie, sizeof(__cookie));
            }
            break;
        case BR_RELEASE:
            /*do sth to decrease ref*/
            break;
        case BR_INCREFS:
            {
                ptr = (struct binder_ptr_cookie*)data;
                /*do sth*/
                __ptr = (binder_uintptr_t)(ptr->ptr);
                __cookie = (binder_uintptr_t)(ptr->cookie);
                r_cmd = BC_INCREFS_DONE;
                ti_write_outbuf(ti, &r_cmd, sizeof(r_cmd));
                ti_write_outbuf(ti, &__ptr, sizeof(__ptr));
                ti_write_outbuf(ti, &__cookie, sizeof(__cookie));
            }
            break;
        case BR_DECREFS:
            ptr = (struct binder_ptr_cookie*)data;
            /*do sth*/
            break;
        case BR_ATTEMPT_ACQUIRE:
            {
                int32_t success = 0;
                ptr = (struct binder_ptr_cookie*)data;
                /*do sth*/
                r_cmd = BC_ACQUIRE_RESULT;
                ti_write_outbuf(ti, &r_cmd, sizeof(r_cmd));
                ti_write_outbuf(ti, &success, sizeof(success));
            }
            break;
        case BR_TRANSACTION:
            {
                int32_t status_flag = 0;
                struct binder_transaction_data * tr = (struct binder_transaction_data *)data;
                if(tr->target.ptr){
                    tBinderService *b_srv = (tBinderService *)tr->target.ptr;
                    binder_io_init_from_txn(&bio, tr);
                    binder_io_init(&reply, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
                    if(b_srv && b_srv->transact_cb) status_flag = b_srv->transact_cb(tr->code,&bio,&reply,tr->flags);
                }

                /*send reply for one way call*/
                if(! (tr->flags & TF_ONE_WAY)){
                    struct binder_transaction_data reply_txn;
                    memset(&reply_txn, 0 ,sizeof(reply_txn));
                    /*if this is a status code, turn on the flag.*/
                    if(status_flag){
                        reply_txn.flags = TF_STATUS_CODE;
                        reply_txn.data_size = sizeof(status_flag);
                        reply_txn.data.ptr.buffer = (binder_uintptr_t)&status_flag;
                        reply_txn.offsets_size = 0;
                        reply_txn.data.ptr.offsets = 0;
                    }else{
                        binder_io_to_txn(&reply, &reply_txn);
                    }
                    r_cmd = BC_REPLY;
                    
                    ti_write_outbuf(ti, &r_cmd, sizeof(r_cmd));
                    ti_write_outbuf(ti, &reply_txn, sizeof(reply_txn));
                    /*wait for BR_TRANSACTION_COMPLETE from driver*/
                    result = _binder_cmd_wait_rsp(ti, NULL);
                    
                }

                /*free the txn buffer*/
                binder_cmd_freebuf(ti, (void *)tr->data.ptr.buffer);
            }
            break;
        case BR_DEAD_BINDER:
            __ptr = *(binder_uintptr_t *)data;
            b_srv = (tBinderService*) __ptr;
            r_cmd = BC_DEAD_BINDER_DONE;
            /*
                do something to notify (dead) event
            */
            //if(b_srv->death_notify_cb) b_srv->death_notify_cb();
            ti_write_outbuf(ti, &r_cmd, sizeof(r_cmd));
            ti_write_outbuf(ti, &__ptr, sizeof(__ptr));
            break;
        case BR_CLEAR_DEATH_NOTIFICATION_DONE:
            b_srv = (tBinderService*)(binder_uintptr_t)data;
            /*
                decrease the reference of the binder proxy
            */
            break;
        case BR_FINISHED:
            result = BINDER_STATUS_TIMEOUT;
            break;
        case BR_NOOP:
            break;
        case BR_SPAWN_LOOPER:
            binder_thread_enter_loop(0);
            break;
        default:
            printf("Warning:Unknown cmd :%d received from binder!!!\n",cmd);
            result = BINDER_STATUS_UNKNOWN;
            break;
    }
    return result;
}

static atomic_uint g_thread_pool_seq = ATOMIC_VAR_INIT(0);

void* binder_thread_loop_run(void * arg){
    tBinderThreadData *t_data = (tBinderThreadData *)arg;
    uint32_t seq = 0;
    int32_t cmd = 0, result = 0;
    
    tBinderBuf rbuf;
    void * data = NULL;
    pid_t pid = getpid();
    tIpcThreadInfo * ti = binder_get_thread_info();

    if(!ti){
        printf("#Error: failed to start binder thread...\n");
        return NULL;
    }
    
    seq = atomic_fetch_add(&g_thread_pool_seq, 1);
    if(strlen(t_data->t_name) == 0)
        snprintf(t_data->t_name,sizeof(t_data->t_name),"Binder:%d_%x", pid, seq);
    prctl(PR_SET_NAME,t_data->t_name);
    
    /**/
    cmd = t_data->isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER;
    ti_write_outbuf(ti, &cmd, sizeof(cmd));
    do{
        if(talk_with_driver(ti, 1) >= 0){
            binder_buf_init(&rbuf, ti->in_buf.ptr, ti->in_buf.consumed, 0);

            if(rbuf.size <= 0) continue;
            
            cmd = binder_buf_get_next_cmd(&rbuf);

            if(rbuf.consumed > 0){
                /*move the in buf first*/
                binder_buf_move_buffer(&(ti->in_buf), &rbuf);
            }

            if(cmd == 0){
                printf("Error: not a excepted cmd and data in thread:%s\n",t_data->t_name);
                continue;
            }
            
            data = (void *)(rbuf.ptr + sizeof(cmd));

            result = binder_execute_cmds(ti, cmd, data);
        }else{
            printf("Error: fail to talk with driver, exit!\n");
            result = BINDER_STATUS_IO_ERROR;
        }
    }while(result >= 0  && result != BINDER_STATUS_IO_ERROR);

    cmd = BC_EXIT_LOOPER;
    ti_write_outbuf(ti, &cmd, sizeof(cmd));
    flush_commands(ti);
    return NULL;
}


int binder_thread_enter_loop(int isMain){
    pthread_t tid;
    tBinderThreadData *t_data = NULL;
    /*start a thread to listen binder return msg*/
    t_data = (tBinderThreadData *)malloc(sizeof(*t_data));

    if(!t_data) return -1;
    memset(t_data, 0 ,sizeof(*t_data));

    t_data->isMain = isMain;
    
    pthread_create(&tid, NULL, binder_thread_loop_run, (void *)t_data);

    /*generate and set thread name*/

    pthread_join(tid,NULL);
    if(t_data) free(t_data);
    return 0;
}



int binder_add_service(const char * name, tBinderService * b_srv){
    tIpcThreadInfo *ti = binder_get_thread_info();
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    tBinderIo bio, reply;
    uint32_t cmd = BC_TRANSACTION;
    struct binder_transaction_data txn;

    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
    
    binder_io_append_uint32(&bio, 0);/*strict_policy in android, unused now*/
    binder_io_append_string(&bio, DEFAULT_SRV_MANAGER_NAME);
    binder_io_append_string(&bio, name);
    binder_io_append_obj(&bio,(void *)b_srv);
    
    memset(&txn, 0 , sizeof(txn));
    txn.target.handle = DEFAULT_SRV_MANAGER_HANDLE;
    txn.code = SVC_MGR_ADD_SERVICE;
    binder_io_to_txn(&bio, &txn);

    ti_write_outbuf(ti, &cmd, sizeof(cmd));
    ti_write_outbuf(ti, &txn, sizeof(txn));

    flush_commands(ti);
    if(BINDER_STATUS_OK == _binder_cmd_wait_rsp(ti, &reply)){
        /*free buffer*/
        binder_cmd_freebuf(ti, reply.data0);
    }

    return 0;
}

uint32_t binder_check_service(const char * service_name){
    return 0;
}

uint32_t binder_get_service(const char * service_name){
    tIpcThreadInfo *ti = binder_get_thread_info();
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    tBinderIo bio, reply;
    uint32_t cmd = BC_TRANSACTION, handle = 0;
    struct binder_transaction_data txn;

    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
    
    binder_io_append_uint32(&bio, 0);/*strict_policy in android, unused now*/
    binder_io_append_string(&bio, DEFAULT_SRV_MANAGER_NAME);
    binder_io_append_string(&bio, service_name);

    memset(&txn, 0 , sizeof(txn));
    txn.target.handle = DEFAULT_SRV_MANAGER_HANDLE;
    txn.code = SVC_MGR_GET_SERVICE;
    binder_io_to_txn(&bio, &txn);

    ti_write_outbuf(ti, &cmd, sizeof(cmd));
    ti_write_outbuf(ti, &txn, sizeof(txn));

    flush_commands(ti);
    if(BINDER_STATUS_OK ==_binder_cmd_wait_rsp(ti, &reply)){
        if( (handle = binder_io_get_ref(&reply, 0)) > 0){
            binder_cmd_acquire(ti, handle);
            flush_commands(ti);
        }
        /*free buffer*/
        binder_cmd_freebuf(ti, reply.data0);
    }
    return handle;
}

size_t binder_list_service(int idx, char* s_name, int len){
    tIpcThreadInfo *ti = binder_get_thread_info();
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    tBinderIo bio, reply;
    uint32_t cmd = BC_TRANSACTION;
    struct binder_transaction_data txn;
    size_t s_len = 0;

    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
    
    binder_io_append_uint32(&bio, 0);/*strict_policy in android, unused now*/
    binder_io_append_string(&bio, DEFAULT_SRV_MANAGER_NAME);
    binder_io_append_uint32(&bio, idx);

    memset(&txn, 0 , sizeof(txn));
    txn.target.handle = DEFAULT_SRV_MANAGER_HANDLE;
    txn.code = SVC_MGR_LIST_SERVICES;
    binder_io_to_txn(&bio, &txn);

    ti_write_outbuf(ti, &cmd, sizeof(cmd));
    ti_write_outbuf(ti, &txn, sizeof(txn));

    flush_commands(ti);

    if(BINDER_STATUS_OK ==_binder_cmd_wait_rsp(ti, &reply)){
        uint8_t* buf= binder_io_get_string(&reply, &s_len);
        if(buf && s_name){
            snprintf(s_name, len, "%s", buf);
        }
        binder_cmd_freebuf(ti, reply.data0);
    }
    return s_len;
}


