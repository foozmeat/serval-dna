/*
Serval DNA instance directory path
Copyright (C) 2012 Serval Project Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdlib.h>
#include "serval.h"
#include "str.h"
#include "os.h"
#include "strbuf.h"
#include "strbuf_helpers.h"

/*
 * A default INSTANCE_PATH can be set on the ./configure command line, eg:
 *
 *      ./configure INSTANCE_PATH=/var/local/serval/node
 *
 * This will cause servald to never use FHS paths, and always use an instance
 * path, even if the SERVALINSTANCE_PATH environment variable is not set.
 */
#ifdef INSTANCE_PATH
#define DEFAULT_INSTANCE_PATH INSTANCE_PATH
#else
#ifdef ANDROID
#define DEFAULT_INSTANCE_PATH "/data/data/org.servalproject/var/serval-node"
#else
#define DEFAULT_INSTANCE_PATH NULL
#endif
#endif

static int know_instancepath = 0;
static char *instancepath = NULL;

const char *instance_path()
{
  if (!know_instancepath) {
    instancepath = getenv("SERVALINSTANCE_PATH");
    if (instancepath)
      instancepath = DEFAULT_INSTANCE_PATH;
  }
  return instancepath;
}

static int vformf_path(struct __sourceloc __whence, strbuf b, const char *syspath, const char *fmt, va_list ap)
{
  if (fmt)
    strbuf_va_vprintf(b, fmt, ap);
  if (!strbuf_overrun(b) && (strbuf_len(b) == 0 || strbuf_str(b)[0] != '/')) {
    strbuf_reset(b);
    const char *ipath = instance_path();
    strbuf_puts(b, ipath ? ipath : syspath);
    if (fmt) {
      strbuf_putc(b, '/');
      strbuf_va_vprintf(b, fmt, ap);
    }
  }
  if (!strbuf_overrun(b))
    return 1;
  WHYF("instance path overflow (strlen %lu, sizeof buffer %lu): %s",
      (unsigned long)strbuf_count(b),
      (unsigned long)strbuf_size(b),
      alloca_str_toprint(strbuf_str(b)));
  return 0;
}

int _formf_etc_serval_path(struct __sourceloc __whence, char *buf, size_t bufsiz, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = vformf_path(__whence, strbuf_local(buf, bufsiz), "/etc/serval", fmt, ap);
  va_end(ap);
  return ret;
}

int _formf_run_serval_path(struct __sourceloc __whence, char *buf, size_t bufsiz, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = _vformf_run_serval_path(__whence, buf, bufsiz, fmt, ap);
  va_end(ap);
  return ret;
}

int _vformf_run_serval_path(struct __sourceloc __whence, char *buf, size_t bufsiz, const char *fmt, va_list ap)
{
  return vformf_path(__whence, strbuf_local(buf, bufsiz), "/var/run/serval", fmt, ap);
}

int _formf_log_path(struct __sourceloc __whence, char *buf, size_t bufsiz, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = vformf_path(__whence, strbuf_local(buf, bufsiz), "/var/log", fmt, ap);
  va_end(ap);
  return ret;
}

int _strbuf_log_serval_path(struct __sourceloc __whence, strbuf sb, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = vformf_path(__whence, sb, "/var/log/serval", fmt, ap);
  va_end(ap);
  return ret;
}

int _formf_cache_serval_path(struct __sourceloc __whence, char *buf, size_t bufsiz, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = vformf_path(__whence, strbuf_local(buf, bufsiz), "/var/cache/serval", fmt, ap);
  va_end(ap);
  return ret;
}

int _formf_tmp_serval_path(struct __sourceloc __whence, char *buf, size_t bufsiz, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = vformf_path(__whence, strbuf_local(buf, bufsiz), "/tmp/serval", fmt, ap);
  va_end(ap);
  return ret;
}

int _formf_servald_proc_path(struct __sourceloc __whence, char *buf, size_t bufsiz, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = vformf_path(__whence, strbuf_local(buf, bufsiz), "/var/run/serval/proc", fmt, ap);
  va_end(ap);
  return ret;
}

int create_serval_instance_dir()
{
  const char *ipath = instance_path();
  if (ipath) {
    if (emkdirs(ipath, 0700) == -1)
      return -1;
  }
  return 0;
}
