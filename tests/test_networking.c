// Команды выполнять в папке сборки
// cmake .. -DBUILD_TESTING=ON
// make -j8
// ctest --force-new-ctest-process --verbose

// Для работы тестов нужен запущеный локально VDI сервер


#include <glib.h>
#include <locale.h>

#include "vdi_session.h"


typedef struct {
    VdiSession *session;
} NetworkingFixture;

//=======================================================================
// Utility
static GTask *execute_sync_task(GTaskThreadFunc task_func, gpointer task_data)
{
    GTask *task = g_task_new(NULL, NULL, NULL, NULL);
    if (task_data)
        g_task_set_task_data(task, task_data, NULL);
    g_task_run_in_thread_sync(task, task_func);

    return task;
}

//=======================================================================
//Fixtures
static void
fixture_login_up(NetworkingFixture *fixture, gconstpointer user_data G_GNUC_UNUSED)
{
    fixture->session = get_vdi_session_static();
    // login
    vdi_session_set_credentials("vdiadmin", "Bazalt1!", "", "127.0.0.1", 8888, FALSE);
    GTask *task = execute_sync_task(vdi_session_log_in_task, NULL);

    LoginData *login_data = g_task_propagate_pointer(G_TASK(task), NULL);
    g_info("reply_msg: %s", login_data->reply_msg);
    g_assert_null(login_data->reply_msg);

    g_object_unref(task);

    vdi_api_session_free_login_data(login_data);
}

static void
fixture_login_down(NetworkingFixture *fixture G_GNUC_UNUSED, gconstpointer user_data G_GNUC_UNUSED)
{
    // logout
    gboolean is_success = vdi_session_logout();
    g_assert_true(is_success);
    vdi_session_static_destroy();
}

//=======================================================================
// Tests
static void
test_login_logout(NetworkingFixture *fixture G_GNUC_UNUSED, gconstpointer user_data G_GNUC_UNUSED)
{
    // Check token
    g_autofree gchar *token = NULL;
    token = vdi_session_get_token();
    g_assert_nonnull(token);
}

static void
test_get_pool_data(NetworkingFixture *fixture G_GNUC_UNUSED, gconstpointer user_data G_GNUC_UNUSED)
{
    GTask *task = execute_sync_task(vdi_session_get_vdi_pool_data_task, NULL);

    // check
    gpointer ptr_res = g_task_propagate_pointer(G_TASK(task), NULL);
    g_assert_nonnull(ptr_res);

    g_autofree gchar *response_body_str = NULL;
    response_body_str = ptr_res;

    JsonParser *parser = json_parser_new();
    ServerReplyType server_reply_type;
    json_get_data_or_errors_object(parser, response_body_str, &server_reply_type);
    // Проверяем что запрос закончился успешно
    g_assert_true(server_reply_type == SERVER_REPLY_TYPE_DATA);
    g_object_unref(parser);

    g_object_unref(task);
}

static void
test_turn_on_and_turn_off_2fa(NetworkingFixture *fixture G_GNUC_UNUSED, gconstpointer user_data G_GNUC_UNUSED)
{
    // Get user data
    GTask *get_user_data_task = execute_sync_task(vdi_session_get_user_data_task, NULL);
    // Check
    gpointer ptr_res = g_task_propagate_pointer(G_TASK(get_user_data_task), NULL);
    g_assert_nonnull(ptr_res);
    UserData *tk_user_data = (UserData *)ptr_res;
    g_assert_true(tk_user_data->is_success);
    vdi_api_session_free_tk_user_data(tk_user_data);

    g_object_unref(get_user_data_task);

    // Generate QR code
    GTask *generate_qr_code_task = execute_sync_task(vdi_session_generate_qr_code_task, NULL);
    // Check
    ptr_res = g_task_propagate_pointer(G_TASK(generate_qr_code_task), NULL);
    g_assert_nonnull(ptr_res);
    tk_user_data = (UserData *)ptr_res;
    g_assert_true(tk_user_data->is_success);
    g_assert_nonnull(tk_user_data->qr_uri);
    g_assert_nonnull(tk_user_data->secret);
    g_info("secret: %s", tk_user_data->secret);
    vdi_api_session_free_tk_user_data(tk_user_data);

    g_object_unref(generate_qr_code_task);

    // Turn on 2fa
    tk_user_data = calloc(1, sizeof(UserData));
    tk_user_data->two_factor = TRUE;
    GTask *turn_on_2fa_task = execute_sync_task(vdi_session_update_user_data_task, tk_user_data);
    // Check
    ptr_res = g_task_propagate_pointer(G_TASK(turn_on_2fa_task), NULL);
    g_assert_nonnull(ptr_res);
    tk_user_data = (UserData *)ptr_res;
    g_assert_true(tk_user_data->is_success);
    g_assert_true(tk_user_data->two_factor);
    vdi_api_session_free_tk_user_data(tk_user_data);

    g_object_unref(turn_on_2fa_task);

    // Turn off 2fa
    tk_user_data = calloc(1, sizeof(UserData));
    tk_user_data->two_factor = FALSE;
    GTask *task = execute_sync_task(vdi_session_update_user_data_task, tk_user_data);
    // Check
    ptr_res = g_task_propagate_pointer(G_TASK(task), NULL);
    g_assert_nonnull(ptr_res);
    tk_user_data = (UserData *)ptr_res;
    g_assert_true(tk_user_data->is_success);
    g_assert_false(tk_user_data->two_factor);
    vdi_api_session_free_tk_user_data(tk_user_data);

    g_object_unref(task);
}

int
main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add("/test_login_logout", NetworkingFixture, NULL,
               fixture_login_up, test_login_logout, fixture_login_down);

    g_test_add("/test_get_pool_data", NetworkingFixture, NULL,
               fixture_login_up, test_get_pool_data, fixture_login_down);

    g_test_add("/test_turn_on_and_turn_off_2fa", NetworkingFixture, NULL,
               fixture_login_up, test_turn_on_and_turn_off_2fa, fixture_login_down);

    return g_test_run();
}
