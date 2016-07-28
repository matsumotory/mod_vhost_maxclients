/*
** mod_vhost_maxclients - Max Clients per VitualHost
**
** Copyright (c) MATSUMOTO Ryosuke 2015 -
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_core.h"
#include "http_log.h"
#include "util_time.h"

#include "ap_mpm.h"
#include "apr_strings.h"
#include "scoreboard.h"

#define MODULE_NAME "mod_vhost_maxclients"
#define MODULE_VERSION "0.5.1"

#if (AP_SERVER_MINORVERSION_NUMBER > 2)
#define __APACHE24__
#endif

#ifdef __APACHE24__
#define ap_get_scoreboard_worker ap_get_scoreboard_worker_from_indexes
#endif

#define _log_debug                                                                                                     \
  ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, NULL, "DEBUG: " MODULE_NAME "/" MODULE_VERSION "%s:%d", __func__, __LINE__)

#ifdef __APACHE24__
#include "http_main.h"
#else
#define ap_server_conf NULL
#endif

#define VHOST_MAXEXTENSIONS 16
#define AP_CTIME_COMPACT_LEN 20

#if !defined(__APACHE24__) && defined(_WIN32)
/*
 * libhttpd.dll does not export following variables.
 * This won't work correctly, but working well for other functional.
 */
int ap_extended_status = 0;
#endif

module AP_MODULE_DECLARE_DATA vhost_maxclients_module;
static int vhost_maxclients_server_limit, vhost_maxclients_thread_limit;
static apr_file_t *vhost_maxclients_log_fp = NULL;

typedef struct {

  /* vhost max clinetns */
  int dryrun;
  const char *log_path;
  signed int vhost_maxclients;
  signed int vhost_maxclients_log;
  signed int vhost_maxclients_per_ip;
  apr_array_header_t *ignore_extensions;
  unsigned int vhost_maxclients_time_from;
  unsigned int vhost_maxclients_time_to;

} vhost_maxclients_config;

#ifndef __APACHE24__
static apr_status_t ap_recent_ctime_compact(char *date_str, apr_time_t t)
{
  apr_time_exp_t xt;
  int real_year;
  int real_month;

  ap_explode_recent_localtime(&xt, t);
  real_year = 1900 + xt.tm_year;
  real_month = xt.tm_mon + 1;

  *date_str++ = real_year / 1000 + '0';
  *date_str++ = real_year % 1000 / 100 + '0';
  *date_str++ = real_year % 100 / 10 + '0';
  *date_str++ = real_year % 10 + '0';
  *date_str++ = '-';
  *date_str++ = real_month / 10 + '0';
  *date_str++ = real_month % 10 + '0';
  *date_str++ = '-';
  *date_str++ = xt.tm_mday / 10 + '0';
  *date_str++ = xt.tm_mday % 10 + '0';
  *date_str++ = ' ';
  *date_str++ = xt.tm_hour / 10 + '0';
  *date_str++ = xt.tm_hour % 10 + '0';
  *date_str++ = ':';
  *date_str++ = xt.tm_min / 10 + '0';
  *date_str++ = xt.tm_min % 10 + '0';
  *date_str++ = ':';
  *date_str++ = xt.tm_sec / 10 + '0';
  *date_str++ = xt.tm_sec % 10 + '0';
  *date_str++ = 0;

  return APR_SUCCESS;
}
#endif

#define vhost_maxclients_log_error(r, fmt, ...) _vhost_maxclients_log_error(r, apr_psprintf(r->pool, fmt, __VA_ARGS__))

static void *_vhost_maxclients_log_error(request_rec *r, char *log_body)
{
  char log_time[AP_CTIME_COMPACT_LEN];
  char *log;

/* example for compact format: "1993-06-30 21:49:08" */
/*                              1234567890123456789  */
#ifdef __APACHE24__
  int time_len = AP_CTIME_COMPACT_LEN;
  ap_recent_ctime_ex(log_time, r->request_time, AP_CTIME_OPTION_COMPACT, &time_len);
#else
  ap_recent_ctime_compact(log_time, r->request_time);
#endif
  log = apr_psprintf(r->pool, "%s %s\n", log_time, log_body);

  apr_file_puts(log, vhost_maxclients_log_fp);
  apr_file_flush(vhost_maxclients_log_fp);
}

static void *vhost_maxclients_create_server_config(apr_pool_t *p, server_rec *s)
{
  vhost_maxclients_config *scfg = (vhost_maxclients_config *)apr_pcalloc(p, sizeof(*scfg));

  scfg->dryrun = -1;
  scfg->log_path = NULL;
  scfg->vhost_maxclients = 0;
  scfg->vhost_maxclients_log = 0;
  scfg->vhost_maxclients_per_ip = 0;
  scfg->ignore_extensions = apr_array_make(p, VHOST_MAXEXTENSIONS, sizeof(char *));
  scfg->vhost_maxclients_time_from = 0;
  scfg->vhost_maxclients_time_to = 2359;

  return scfg;
}

static void *vhost_maxclients_create_server_merge_conf(apr_pool_t *p, void *b, void *n)
{
  vhost_maxclients_config *base = (vhost_maxclients_config *)b;
  vhost_maxclients_config *new = (vhost_maxclients_config *)n;
  vhost_maxclients_config *scfg = (vhost_maxclients_config *)apr_pcalloc(p, sizeof(*scfg));

  if (new->dryrun > -1) {
    scfg->dryrun = new->dryrun;
  } else {
    scfg->dryrun = base->dryrun;
  }
  scfg->log_path = base->log_path;
  scfg->vhost_maxclients = new->vhost_maxclients;
  scfg->vhost_maxclients_log = new->vhost_maxclients_log;
  scfg->vhost_maxclients_per_ip = new->vhost_maxclients_per_ip;
  scfg->ignore_extensions = new->ignore_extensions;
  scfg->vhost_maxclients_time_from = new->vhost_maxclients_time_from;
  scfg->vhost_maxclients_time_to = new->vhost_maxclients_time_to;

  return scfg;
}

static int check_extension(char *filename, apr_array_header_t *exts)
{
  int i;
  for (i = 0; i < exts->nelts; i++) {
    const char *extension = ((char **)exts->elts)[i];
    ssize_t name_len = strlen(filename) - strlen(extension);
    if (name_len >= 0 && strcmp(&filename[name_len], extension) == 0)
      return 1;
  }
  return 0;
}

static char *build_vhostport_name(request_rec *r)
{
#ifdef __APACHE24__
  ssize_t vhostport_len;
  char *vhostport;

  vhostport_len = strlen(r->hostname) + sizeof(":65536");
  vhostport = apr_pcalloc(r->pool, vhostport_len);
  apr_snprintf(vhostport, vhostport_len, "%s:%d", r->hostname, r->connection->local_addr->port);

  return vhostport;
#else
  return (char *)r->hostname;
#endif
}

static int vhost_maxclients_handler(request_rec *r)
{
  int i, j;
  int vhost_count = 0;
  int ip_count = 0;
  char *vhostport;
  vhost_maxclients_config *scfg =
      (vhost_maxclients_config *)ap_get_module_config(r->server->module_config, &vhost_maxclients_module);

  if (!ap_is_initial_req(r)) {
    return DECLINED;
  }

  if (scfg->vhost_maxclients <= 0) {
    return DECLINED;
  }

  if (r->hostname == NULL) {
    return DECLINED;
  }

  if (!ap_extended_status) {
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, "DEBUG: only used when ExtendedStatus On");
    return DECLINED;
  }

  /* check ignore extesions */
  if (check_extension(r->filename, scfg->ignore_extensions)) {
    return DECLINED;
  }

  apr_time_exp_t reqtime_result;
  apr_time_exp_lt(&reqtime_result, apr_time_now());

  unsigned int cur_hourmin;
  cur_hourmin = atoi(apr_psprintf(r->pool, "%02d%02d", reqtime_result.tm_hour, reqtime_result.tm_min));

  vhost_maxclients_log_error(r, "%d %d %d", scfg->vhost_maxclients_time_from, scfg->vhost_maxclients_time_to, cur_hourmin);

  if (scfg->vhost_maxclients_time_from > scfg->vhost_maxclients_time_to){
    scfg->vhost_maxclients_time_to += 2400;
  }

  if (!((scfg->vhost_maxclients_time_from < cur_hourmin) && (scfg->vhost_maxclients_time_to > cur_hourmin))){
    return DECLINED;
  }

  /* build vhostport name */
  vhostport = build_vhostport_name(r);

  for (i = 0; i < vhost_maxclients_server_limit; ++i) {
    for (j = 0; j < vhost_maxclients_thread_limit; ++j) {
      worker_score *ws_record = ap_get_scoreboard_worker(i, j);
#ifdef __APACHE24__
      char *client_ip = r->connection->client_ip;
#else
      char *client_ip = r->connection->remote_ip;
#endif

      switch (ws_record->status) {
      case SERVER_BUSY_READ:
      case SERVER_BUSY_WRITE:
      case SERVER_BUSY_KEEPALIVE:
      case SERVER_BUSY_LOG:
      case SERVER_BUSY_DNS:
      case SERVER_CLOSING:
      case SERVER_GRACEFUL:
        /* check maxclients per vhost */
        if (strcmp(vhostport, ws_record->vhost) == 0) {
          vhost_count++;
          ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, "DEBUG: (increment %s): %d/%d", vhostport,
                       vhost_count, scfg->vhost_maxclients);
          /* logging only for vhost_maxclients_log */
          if (scfg->vhost_maxclients_log > 0 && vhost_count > scfg->vhost_maxclients_log) {
            if (vhost_maxclients_log_fp != NULL) {
              vhost_maxclients_log_error(
                  r, "LOG-ONLY-VHOST_COUNT return 503 from %s : %d / %d client_ip: %s uri: %s filename: %s", vhostport,
                  vhost_count, scfg->vhost_maxclients_log, client_ip, r->uri, r->filename);
            } else {
              ap_log_error(
                  APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf,
                  "NOTICE: LOG-ONLY-VHOST_COUNT return 503 from %s : %d / %d client_ip: %s uri: %s filename: %s",
                  vhostport, vhost_count, scfg->vhost_maxclients_log, client_ip, r->uri, r->filename);
            }
          }
          if (vhost_count > scfg->vhost_maxclients) {
            if (scfg->dryrun > 0) {
              if (vhost_maxclients_log_fp != NULL) {
                vhost_maxclients_log_error(
                    r, "DRY-RUN-VHOST_COUNT return 503 from %s : %d / %d client_ip: %s uri: %s filename: %s", vhostport,
                    vhost_count, scfg->vhost_maxclients, client_ip, r->uri, r->filename);
              } else {
                ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf,
                             "DRY-RUN-VHOST_COUNT return 503 from %s : %d / %d client_ip: %s uri: %s filename: %s",
                             vhostport, vhost_count, scfg->vhost_maxclients, client_ip, r->uri, r->filename);
              }
            } else {
              if (vhost_maxclients_log_fp != NULL) {
                vhost_maxclients_log_error(
                    r, "VHOST_COUNT return 503 from %s : %d / %d client_ip: %s uri: %s filename: %s", vhostport,
                    vhost_count, scfg->vhost_maxclients, client_ip, r->uri, r->filename);
              } else {
                ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf,
                             "VHOST_COUNT return 503 from %s : %d / %d client_ip: %s uri: %s filename: %s", vhostport,
                             vhost_count, scfg->vhost_maxclients, client_ip, r->uri, r->filename);
              }
            }
            return (scfg->dryrun > 0) ? DECLINED : HTTP_SERVICE_UNAVAILABLE;
          }

          /* check maxclients per ip in same vhost */
          if (scfg->vhost_maxclients_per_ip > 0) {
            if (strcmp(client_ip, ws_record->client) == 0) {
              ip_count++;
              if (ip_count > scfg->vhost_maxclients_per_ip) {
                if (scfg->dryrun > 0) {
                  if (vhost_maxclients_log_fp != NULL) {
                    vhost_maxclients_log_error(
                        r, "DRY-RUN-CLIENT_COUNT return 503 from %s : %d / %d client_ip: %s uri: %s filename: %s",
                        vhostport, ip_count, scfg->vhost_maxclients_per_ip, client_ip, r->uri, r->filename);
                  } else {
                    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, "NOTICE: DRY-RUN-CLIENT_COUNT return "
                                                                              "503 from %s : %d / %d client_ip: %s "
                                                                              "uri: %s filename: %s",
                                 vhostport, ip_count, scfg->vhost_maxclients_per_ip, client_ip, r->uri, r->filename);
                  }
                } else {
                  if (vhost_maxclients_log_fp != NULL) {
                    vhost_maxclients_log_error(
                        r, "CLIENT_COUNT return 503 from %s : %d / %d client_ip: %s uri: %s filename: %s", vhostport,
                        ip_count, scfg->vhost_maxclients_per_ip, client_ip, r->uri, r->filename);
                  } else {
                    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf,
                                 "CLIENT_COUNT return 503 from %s : %d / %d client_ip: %s uri: %s filename: %s",
                                 vhostport, ip_count, scfg->vhost_maxclients_per_ip, client_ip, r->uri, r->filename);
                  }
                }
                return (scfg->dryrun > 0) ? DECLINED : HTTP_SERVICE_UNAVAILABLE;
              }
            }
          }
        }
        break;
      default:
        break;
      }
    }
  }

  /* not reached vhost_maxclients */
  return DECLINED;
}

static const char *set_vhost_maxclients_dryrun(cmd_parms *parms, void *mconfig, int flag)
{
  vhost_maxclients_config *scfg =
      (vhost_maxclients_config *)ap_get_module_config(parms->server->module_config, &vhost_maxclients_module);

  scfg->dryrun = flag;

  return NULL;
}

static const char *set_vhost_maxclientsvhost(cmd_parms *parms, void *mconfig, const char *arg1)
{
  vhost_maxclients_config *scfg =
      (vhost_maxclients_config *)ap_get_module_config(parms->server->module_config, &vhost_maxclients_module);
  signed long int limit = strtol(arg1, (char **)NULL, 10);

  if ((limit > 65535) || (limit < 0)) {
    return "Integer overflow or invalid number";
  }

  scfg->vhost_maxclients = limit;

  return NULL;
}

static const char *set_vhost_maxclientsvhost_log(cmd_parms *parms, void *mconfig, const char *arg1)
{
  vhost_maxclients_config *scfg =
      (vhost_maxclients_config *)ap_get_module_config(parms->server->module_config, &vhost_maxclients_module);
  signed long int limit = strtol(arg1, (char **)NULL, 10);

  if ((limit > 65535) || (limit < 0)) {
    return "Integer overflow or invalid number";
  }

  scfg->vhost_maxclients_log = limit;

  return NULL;
}

static const char *set_vhost_maxclientsvhost_log_path(cmd_parms *parms, void *mconfig, const char *arg1)
{
  vhost_maxclients_config *scfg =
      (vhost_maxclients_config *)ap_get_module_config(parms->server->module_config, &vhost_maxclients_module);

  scfg->log_path = arg1;

  return NULL;
}

static const char *set_vhost_maxclientsvhost_perip(cmd_parms *parms, void *mconfig, const char *arg1)
{
  vhost_maxclients_config *scfg =
      (vhost_maxclients_config *)ap_get_module_config(parms->server->module_config, &vhost_maxclients_module);
  signed long int limit = strtol(arg1, (char **)NULL, 10);

  if ((limit > 65535) || (limit < 0)) {
    return "Integer overflow or invalid number";
  }

  scfg->vhost_maxclients_per_ip = limit;

  return NULL;
}

static const char *set_vhost_ignore_extensions(cmd_parms *parms, void *mconfig, const char *arg)
{
  vhost_maxclients_config *scfg =
      (vhost_maxclients_config *)ap_get_module_config(parms->server->module_config, &vhost_maxclients_module);

  if (VHOST_MAXEXTENSIONS < scfg->ignore_extensions->nelts) {
    return "the number of ignore extensions exceeded";
  }

  *(const char **)apr_array_push(scfg->ignore_extensions) = arg;

  return NULL;
}

static const char *set_vhost_maxclients_time(cmd_parms *parms, void *mconfig, const char *arg1, const char *arg2)
{
  vhost_maxclients_config *scfg =
      (vhost_maxclients_config *)ap_get_module_config(parms->server->module_config, &vhost_maxclients_module);

  scfg->vhost_maxclients_time_from = atoi(arg1);
  scfg->vhost_maxclients_time_to = atoi(arg2);

  if(scfg->vhost_maxclients_time_from < 0 || scfg->vhost_maxclients_time_from > 2359){
    return "the limit time from invalid number";
  } else if (scfg->vhost_maxclients_time_to < 0 || scfg->vhost_maxclients_time_to > 2359){
    return "the limit time to invalid number";
  }

  return NULL;
}

static command_rec vhost_maxclients_cmds[] = {
    AP_INIT_FLAG("VhostMaxClientsDryRun", set_vhost_maxclients_dryrun, NULL, ACCESS_CONF | RSRC_CONF,
                 "Enable dry-run which don't return 503, logging only: On / Off (default Off)"),
    AP_INIT_TAKE1("VhostMaxClients", set_vhost_maxclientsvhost, NULL, RSRC_CONF | ACCESS_CONF,
                  "maximum connections per Vhost"),
    AP_INIT_TAKE1("VhostMaxClientsLogOnly", set_vhost_maxclientsvhost_log, NULL, RSRC_CONF | ACCESS_CONF,
                  "loggign only: maximum connections per Vhost"),
    AP_INIT_TAKE1("VhostMaxClientsLogPath", set_vhost_maxclientsvhost_log_path, NULL, RSRC_CONF | ACCESS_CONF,
                  "logging file path instead of error_log"),
    AP_INIT_TAKE1("VhostMaxClientsPerIP", set_vhost_maxclientsvhost_perip, NULL, RSRC_CONF | ACCESS_CONF,
                  "maximum connections per IP of Vhost"),
    AP_INIT_ITERATE("IgnoreVhostMaxClientsExt", set_vhost_ignore_extensions, NULL, ACCESS_CONF | RSRC_CONF,
                  "Set Ignore Extensions."),
    AP_INIT_TAKE2("VhostMaxClientsTime", set_vhost_maxclients_time, NULL, RSRC_CONF | ACCESS_CONF,
                  "Set Limit time."),
    {NULL},
};

static int vhost_maxclients_init(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *server)
{
  void *data;
  const char *userdata_key = "vhost_maxclients_init";
  vhost_maxclients_config *scfg =
      (vhost_maxclients_config *)ap_get_module_config(server->module_config, &vhost_maxclients_module);

  apr_pool_userdata_get(&data, userdata_key, server->process->pool);

  if (!data) {
    apr_pool_userdata_set((const void *)1, userdata_key, apr_pool_cleanup_null, server->process->pool);
    return OK;
  }

  ap_mpm_query(AP_MPMQ_HARD_LIMIT_THREADS, &vhost_maxclients_thread_limit);
  ap_mpm_query(AP_MPMQ_HARD_LIMIT_DAEMONS, &vhost_maxclients_server_limit);
  ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf,
               MODULE_NAME "/" MODULE_VERSION " enabled: %d/%d thread_limit/server_limit",
               vhost_maxclients_thread_limit, vhost_maxclients_server_limit);

  /* open custom log instead of error_log */
  if (scfg->log_path != NULL) {
    if (apr_file_open(&vhost_maxclients_log_fp, scfg->log_path, APR_WRITE | APR_APPEND | APR_CREATE, APR_OS_DEFAULT,
                      p) != APR_SUCCESS) {
      ap_log_error(APLOG_MARK, APLOG_EMERG, 0, server, "%s ERROR %s: vhost_maxclients log file oepn failed: %s",
                   MODULE_NAME, __func__, scfg->log_path);

      return HTTP_INTERNAL_SERVER_ERROR;
    }
  }

  return OK;
}

static void vhost_maxclients_register_hooks(apr_pool_t *p)
{
  ap_hook_post_config(vhost_maxclients_init, NULL, NULL, APR_HOOK_MIDDLE);
  ap_hook_access_checker(vhost_maxclients_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

#ifdef __APACHE24__
AP_DECLARE_MODULE(vhost_maxclients) = {
#else
module AP_MODULE_DECLARE_DATA vhost_maxclients_module = {
#endif
    STANDARD20_MODULE_STUFF, NULL,             /* create per-dir config structures     */
    NULL,                                      /* merge  per-dir    config structures  */
    vhost_maxclients_create_server_config,     /* create per-server config
                                                  structures  */
    vhost_maxclients_create_server_merge_conf, /* merge  per-server config structures  */
    vhost_maxclients_cmds,                     /* table of config file commands        */
    vhost_maxclients_register_hooks};
