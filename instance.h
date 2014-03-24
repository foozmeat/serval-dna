/* 
Serval DNA header file - system paths
Copyright (C) 2014 Serval Project Inc.

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

#ifndef __SERVAL_DNA__INSTANCE_H
#define __SERVAL_DNA__INSTANCE_H

#include "log.h"
#include "strbuf.h"

const char *instance_path(); // returns NULL if not using an instance path
int create_serval_instance_dir();

/* Handy macros for forming the absolute paths of various files, using a char[]
 * buffer whose declaration is in scope (so that sizeof(buf) will work).
 * Evaluates to true if the pathname fits into the provided buffer, false (0)
 * otherwise (after logging an error).
 */
#define FORM_ETC_SERVAL_PATH(buf, path)     (_formf_etc_serval_path(__WHENCE__, buf, sizeof(buf), "%s", (path)))
#define FORM_RUN_SERVAL_PATH(buf, path)     (_formf_run_serval_path(__WHENCE__, buf, sizeof(buf), "%s", (path)))
#define FORM_LOG_PATH(buf, path)            (_formf_log_path(__WHENCE__, buf, sizeof(buf), "%s", (path)))
#define FORM_CACHE_SERVAL_PATH(buf, path)   (_formf_cache_serval_path(__WHENCE__, buf, sizeof(buf), "%s", (path)))
#define FORM_TMP_SERVAL_PATH(buf, path)     (_formf_tmp_serval_path(__WHENCE__, buf, sizeof(buf), "%s", (path)))
#define FORMF_SERVALD_PROC_PATH(buf,fmt,...) (_formf_servald_proc_path(__WHENCE__, buf, sizeof(buf), fmt, ##__VA_ARGS__)

int _formf_etc_serval_path(struct __sourceloc, char *buf, size_t bufsiz, const char *fmt, ...) __attribute__((format(printf,4,5)));
int _formf_run_serval_path(struct __sourceloc, char *buf, size_t bufsiz, const char *fmt, ...) __attribute__((format(printf,4,5)));
int _formf_cache_serval_path(struct __sourceloc, char *buf, size_t bufsiz, const char *fmt, ...) __attribute__((format(printf,4,5)));
int _formf_tmp_serval_path(struct __sourceloc, char *buf, size_t bufsiz, const char *fmt, ...) __attribute__((format(printf,4,5)));
int _formf_servald_proc_path(struct __sourceloc, char *buf, size_t bufsiz, const char *fmt, ...) __attribute__((format(printf,4,5)));

#define vformf_run_serval_path(buf,bufsiz,fmt,ap) (_vformf_run_serval_path(__WHENCE__, buf, bufsiz, fmt, ap))
int _vformf_run_serval_path(struct __sourceloc, char *buf, size_t bufsiz, const char *fmt, va_list);

int strbuf_log_path(strbuf sb);
int strbuf_log_serval_path(strbuf sb);

#endif // __SERVAL_DNA__INSTANCE_H
