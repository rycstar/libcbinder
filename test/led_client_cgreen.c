#include "binder_common.h"
#include "binder_ipc.h"
#include "binder_io.h"
#include <sys/time.h>
#include <cgreen/cgreen.h>

tIpcThreadInfo *g_tsd = NULL;

Describe(Dev_client);

BeforeEach(Dev_client) { 
    g_tsd = binder_get_thread_info();
    assert_that(g_tsd, is_not_equal_to(0));
}
AfterEach(Dev_client) {
    flush_commands(g_tsd);
    binder_threads_shutdown();
}


Ensure(Dev_client, sync_call_led_service){
    tBinderIo bio, msg;
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    
    uint32_t led_handle = binder_get_service("fv.led.service");
    assert_that(led_handle, is_not_equal_to(0));

    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
    binder_io_append_string(&bio, "sync_call_led_service");

    assert_that(binder_cmd_sync_call(g_tsd,&bio,&msg,led_handle,0), is_equal_to(BINDER_STATUS_OK));
    binder_cmd_freebuf(g_tsd, msg.data0);
    binder_cmd_release(g_tsd,led_handle);
}

Ensure(Dev_client, async_call_led_service){
    tBinderIo bio, msg;
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    
    
    uint32_t led_handle = binder_get_service("fv.led.service");

    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
    binder_io_append_string(&bio, "async_call_led_service");

    assert_that(binder_cmd_async_call(g_tsd,&bio,&msg,led_handle,0), is_equal_to(BINDER_STATUS_OK));
    binder_cmd_release(g_tsd,led_handle);
    
}


Ensure(Dev_client, sync_call_key_service){
    tBinderIo bio, msg;
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    
    uint32_t key_handle = binder_get_service("fv.key.service");
    assert_that(key_handle, is_not_equal_to(0));

    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
    binder_io_append_string(&bio, "sync_call_key_service");

    assert_that(binder_cmd_sync_call(g_tsd,&bio,&msg,key_handle,0), is_equal_to(BINDER_STATUS_OK));
    binder_cmd_freebuf(g_tsd, msg.data0);
    binder_cmd_release(g_tsd,key_handle);
    
}

Ensure(Dev_client, async_call_key_service){
    tBinderIo bio, msg;
    char binder_buf[DEFAULT_BINDER_IOBUF_SIZE] = {0};
    
    uint32_t key_handle = binder_get_service("fv.key.service");
    assert_that(key_handle, is_not_equal_to(0));

    binder_io_init(&bio, binder_buf, sizeof(binder_buf), DEFAULT_OFFSET_LIST_SIZE);
    binder_io_append_string(&bio, "async_call_key_service");

    assert_that(binder_cmd_async_call(g_tsd,&bio,&msg,key_handle,0), is_equal_to(BINDER_STATUS_OK));
    binder_cmd_release(g_tsd,key_handle);
    
}


int main(int argc, char * argv[]){
    TestSuite *suite = create_test_suite();

    add_test_with_context(suite, Dev_client, sync_call_led_service);
    add_test_with_context(suite, Dev_client, async_call_led_service);
    add_test_with_context(suite, Dev_client, sync_call_key_service);
    add_test_with_context(suite, Dev_client, async_call_key_service);

    //return run_single_test(suite,"Dev_client",create_text_reporter());
    return run_test_suite(suite, create_text_reporter());
}


