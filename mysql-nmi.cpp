#include <pipy/nmi.h>
#include <string>
#include <thread>
#include <cstdlib>

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

  unsigned int mysql_ct = 3;

  if (mysql_options(con, MYSQL_OPT_CONNECT_TIMEOUT, &mysql_ct)) {
    fprintf(stderr, "debug (mysql): [mysql_options] failed.");
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

//
// MysqlProbePipeline
//

struct config {
  int port;
  char ip[128];
  char user[128];
  char passwd[128];
  char sql[1024];
};

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

class MysqlProbePipeline {
public:
  MysqlProbePipeline(pipy_pipeline pipeline) : m_pipeline(pipeline) {}

  void process(pjs_value evt) {
    if (pipy_is_MessageStart(evt)) {
      if (!m_message_started) {
        m_message_started = true;
        {
          pjs_value head = pipy_MessageStart_get_head(evt);
          if (!pjs_is_null(head)) {
            get_string(head, "mysqlIp", cfg.ip, sizeof(cfg.ip));
            get_int(head, "mysqlPort", &cfg.port);
            get_string(head, "mysqlUser", cfg.user, sizeof(cfg.user));
            get_string(head, "mysqlPasswd", cfg.passwd, sizeof(cfg.passwd));
            get_string(head, "mysqlSql", cfg.sql, sizeof(cfg.sql));
          }
        }
        m_message_body.clear();
      }
    } else if (pipy_is_Data(evt)) {
      if (m_message_started) {
        auto len = pipy_Data_get_size(evt);
        auto buf = new char[len];
        pipy_Data_get_data(evt, buf, len);
        m_message_body.append(buf, len);
        delete [] buf;
      }
    } else if (pipy_is_MessageEnd(evt)) {
      if (m_message_started) {
        new MysqlProbe(m_pipeline, cfg, m_message_body);
        m_message_started = false;
      }
    }
  }

private:
  pipy_pipeline m_pipeline;
  std::string m_message_body;
  bool m_message_started = false;
  config cfg;

  //
  // MysqlProbePipeline::MysqlProbe
  //

  class MysqlProbe {
  public:
    MysqlProbe(pipy_pipeline pipeline, const config &cfg, const std::string &host)
      : m_pipeline(pipeline)
      , m_cfg(cfg)
      , m_host(host)
    {
      pipy_hold(pipeline);
      std::thread(
        [this]() {
          rc = probe_mysql(m_cfg.ip, m_cfg.port, m_cfg.user, m_cfg.passwd, m_cfg.sql);
          pipy_schedule(m_pipeline, 0, output, this);
        }
      ).detach();
    }

  private:
    pipy_pipeline m_pipeline;
    config m_cfg;
    std::string m_host;
    int m_result;
    int rc;

    static void output(void *user_ptr) {
      static_cast<MysqlProbe*>(user_ptr)->output();
    }

    void output() {
      pjs_value response_head = pjs_object();
      pjs_object_set_property(response_head, pjs_string("result", strlen("result")), pjs_number(rc));
      pipy_output_event(m_pipeline, pipy_MessageStart_new(response_head));
      pipy_output_event(m_pipeline, pipy_MessageEnd_new(0, 0));
      pipy_free(m_pipeline);
      delete this;
    }
  };
};

static void pipeline_init(pipy_pipeline ppl, void **user_ptr) {
  *user_ptr = new MysqlProbePipeline(ppl);
}

static void pipeline_free(pipy_pipeline ppl, void *user_ptr) {
  delete static_cast<MysqlProbePipeline*>(user_ptr);
}

static void pipeline_process(pipy_pipeline ppl, void *user_ptr, pjs_value evt) {
  static_cast<MysqlProbePipeline*>(user_ptr)->process(evt);
}

extern "C" void pipy_module_init() {
  pipy_define_pipeline(
    "",
    pipeline_init,
    pipeline_free,
    pipeline_process
  );
}

