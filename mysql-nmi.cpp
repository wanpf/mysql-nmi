// clang-format off
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <pipy/nmi.h>

extern "C" {

#include <string.h>
#include <mysql.h>

// clang-format on

int probe_mysql(char *ip, int port, char *user, char *passwd, char *sql) {
  int rc = -1000;
  MYSQL *con = mysql_init(NULL);

  if (con == NULL) {
    return -1;
  }
  if (mysql_real_connect(con, ip, user, passwd, NULL, port, NULL, 0) == NULL) {
    mysql_close(con);
    return -2;
  }
  if (mysql_query(con, sql)) {
    mysql_close(con);
    return -3;
  }

  MYSQL_RES *result = mysql_store_result(con);

  if (result == NULL) {
    mysql_close(con);
    return -4;
  }
  if (mysql_fetch_row(result)) {
    rc = 1;
  } else {
    rc = 0;
  }

  mysql_free_result(result);
  mysql_close(con);
  return rc;
}
}

class MysqlProbe {
  std::mutex m;
  std::condition_variable cv;
  pipy_pipeline m_pipeline;
  std::thread m_thread;
  bool ready = false;

  void main() {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [this] { return ready; });
    auto ppl = m_pipeline;
    rc = probe_mysql(ip, port, user, passwd, sql);
    pipy_schedule(ppl, 0, output_end, this);
  }

  static void output_end(void *user_ptr) {
    auto thiz = static_cast<MysqlProbe *>(user_ptr);
    auto ppl = thiz->m_pipeline;

    pjs_value response_head = pjs_object();
    pjs_object_set_property(response_head, pjs_string("result", strlen("result")), pjs_number(thiz->rc));
    pipy_output_event(ppl, pipy_MessageStart_new(response_head));
    pipy_output_event(ppl, pipy_MessageEnd_new(0, 0));

    pipy_free(ppl);
  }

public:
  MysqlProbe(pipy_pipeline ppl) : m_pipeline(ppl), m_thread([this]() { main(); }) {
    pipy_hold(m_pipeline);
    m_thread.detach();
  }

  void launch() {
    {
      std::lock_guard<std::mutex> lk(m);
      ready = true;
    }
    cv.notify_one();
  }

  int is_started;
  int rc;
  // user data
  int port;
  char ip[128];
  char user[128];
  char passwd[128];
  char sql[1024];
};

static void pipeline_init(pipy_pipeline ppl, void **user_ptr) { *user_ptr = new MysqlProbe(ppl); }

static void pipeline_free(pipy_pipeline ppl, void *user_ptr) { delete static_cast<MysqlProbe *>(user_ptr); }

static char *get_string(pjs_value head, char *name, char *buf, int size) {
  pjs_value value = pjs_undefined();

  pjs_object_get_property(head, pjs_string(name, strlen(name)), value);
  if (pjs_is_undefined(value)) {
    return NULL;
  }
  int n = pjs_string_get_utf8_data(value, buf, size - 1);
  if (n > 0) {
    buf[n] = '\0';
    return buf;
  }
  return NULL;
}

static int get_int(pjs_value head, char *name, int *n) {
  pjs_value value = pjs_undefined();

  pjs_object_get_property(head, pjs_string(name, strlen(name)), value);
  if (pjs_is_undefined(value)) {
    return -1;
  }
  *n = pjs_to_number(value);
  return 0;
}

static void pipeline_process(pipy_pipeline ppl, void *user_ptr, pjs_value evt) {
  MysqlProbe *state = (MysqlProbe *)user_ptr;
  if (pipy_is_MessageStart(evt)) {
    state->is_started = 1;
    pjs_value head = pipy_MessageStart_get_head(evt);
    if (!pjs_is_null(head)) {
      get_string(head, "mysqlIp", state->ip, sizeof(state->ip));
      get_int(head, "mysqlPort", &state->port);
      get_string(head, "mysqlUser", state->user, sizeof(state->user));
      get_string(head, "mysqlPasswd", state->passwd, sizeof(state->passwd));
      get_string(head, "mysqlSql", state->sql, sizeof(state->sql));
    }
  } else if (pipy_is_MessageEnd(evt)) {
    if (state->is_started == 1) {
      state->launch();
    }
  }
}

extern "C" void pipy_module_init() { pipy_define_pipeline("", pipeline_init, pipeline_free, pipeline_process); }

