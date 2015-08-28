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

#include "ap_mpm.h"
#include "apr_strings.h"
#include "scoreboard.h"

#define MODULE_NAME "mod_vhost_maxclients"
#define MODULE_VERSION "0.0.1"

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

#if !defined(__APACHE24__) && defined(_WIN32)
/*
 * libhttpd.dll does not export following variables.
 * This won't work correctly, but working well for other functional.
 */
int ap_extended_status = 0;
#endif

module AP_MODULE_DECLARE_DATA vhost_maxclients_module;
static int vhost_maxclients_server_limit, vhost_maxclients_thread_limit;

typedef struct {

  /* vhost max clinetns */
  signed int vhost_maxclients;
  apr_array_header_t *ignore_extensions;

} vhost_maxclients_config;

static void *vhost_maxclients_create_server_config(apr_pool_t *p, server_rec *s)
{
  vhost_maxclients_config *scfg = (vhost_maxclients_config *)apr_pcalloc(p, sizeof(*scfg));

  scfg->vhost_maxclients = 0;
  scfg->ignore_extensions = apr_array_make(p, VHOST_MAXEXTENSIONS, sizeof(char *));

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
  ssize_t vhostport_len;
  char *vhostport;

  vhostport_len = strlen(r->hostname) + sizeof(":65536");
  vhostport = apr_pcalloc(r->pool, vhostport_len);
  apr_snprintf(vhostport, vhostport_len, "%s:%d", r->hostname, r->connection->local_addr->port);

  return vhostport;
}

static int vhost_maxclients_handler(request_rec *r)
{
  int i, j;
  int vhost_count = 0;
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

  /* build vhostport name */
  vhostport = build_vhostport_name(r);

  for (i = 0; i < vhost_maxclients_server_limit; ++i) {
    for (j = 0; j < vhost_maxclients_thread_limit; ++j) {
      worker_score *ws_record = ap_get_scoreboard_worker(i, j);

      switch (ws_record->status) {
      case SERVER_BUSY_READ:
      case SERVER_BUSY_WRITE:
      case SERVER_BUSY_KEEPALIVE:
      case SERVER_BUSY_LOG:
      case SERVER_BUSY_DNS:
      case SERVER_CLOSING:
      case SERVER_GRACEFUL:
        if (strcmp(vhostport, ws_record->vhost) == 0) {
          vhost_count++;
          ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, "DEBUG: (increment %s): %d/%d", vhostport,
                       vhost_count, scfg->vhost_maxclients);
          if (vhost_count > scfg->vhost_maxclients) {
            ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, "NOTICE: (return 503 from %s): %d/%d", vhostport,
                         vhost_count, scfg->vhost_maxclients);
            return HTTP_SERVICE_UNAVAILABLE;
          }
          break;
        }
      default:
        break;
      }
    }
  }

  /* not reached vhost_maxclients */
  return DECLINED;
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

static command_rec vhost_maxclients_cmds[] = {
    AP_INIT_TAKE1("VhostMaxClients", set_vhost_maxclientsvhost, NULL, RSRC_CONF | ACCESS_CONF,
                  "maximum connections per Vhost"),
    AP_INIT_ITERATE("IgnoreVhostMaxClientsExt", set_vhost_ignore_extensions, NULL, ACCESS_CONF | RSRC_CONF,
                    "Set Ignore Extensions."),
    {NULL},
};

static int vhost_maxclients_init(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
  ap_mpm_query(AP_MPMQ_HARD_LIMIT_THREADS, &vhost_maxclients_thread_limit);
  ap_mpm_query(AP_MPMQ_HARD_LIMIT_DAEMONS, &vhost_maxclients_server_limit);
  ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf,
               MODULE_NAME "/" MODULE_VERSION " enabled: %d/%d thread_limit/server_limit",
               vhost_maxclients_thread_limit, vhost_maxclients_server_limit);

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
    STANDARD20_MODULE_STUFF, NULL,         /* create per-dir config structures     */
    NULL,                                  /* merge  per-dir    config structures  */
    vhost_maxclients_create_server_config, /* create per-server config
                                              structures  */
    NULL,                                  /* merge  per-server config structures  */
    vhost_maxclients_cmds,                 /* table of config file commands        */
    vhost_maxclients_register_hooks};
