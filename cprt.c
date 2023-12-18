/* cprt.c - Module for portable functions that can't be done
 *   with macros in the cprt.h file.
 * See https://github.com/fordsfords/cprt
 */
/*
# This code and its documentation is Copyright 2002-2021 Steven Ford
# and licensed "public domain" style under Creative Commons "CC0":
#   http://creativecommons.org/publicdomain/zero/1.0/
# To the extent possible under law, the contributors to this project have
# waived all copyright and related or neighboring rights to this work.
# In other words, you can use this code for any purpose without any
# restrictions.  This work is published from: United States.  The project home
# is https://github.com/fordsfords/cprt
*/

#if ! defined(_WIN32)
/* Unix */
#define _GNU_SOURCE
#endif

#include "cprt.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#if defined(_WIN32)
LARGE_INTEGER cprt_frequency;
LARGE_INTEGER cprt_start_time;
#endif


#if defined(_WIN32)
int cprt_timeofday(struct cprt_timeval *tv, void *unused_tz)
{
  FILETIME tfile;
  uint64_t time;
  /* Static constant to remove time skew between FILETIME and POSIX
   * time.  POSIX and Win32 use different epochs (Jan. 1, 1970 v.s.
   * Jan. 1, 1601).  The following constant defines the difference
   * in 100ns ticks.  */
  static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

  GetSystemTimeAsFileTime(&tfile);
  time = (uint64_t)tfile.dwLowDateTime;
  time += ((uint64_t)tfile.dwHighDateTime) << 32;

  tv->tv_sec = (long)((time - EPOCH) / 10000000L);
  tv->tv_usec = (long)(((time - EPOCH) / 10) % 1000000L);

  return 0;
}  /* cprt_timeofday */


void cprt_inittime()
{
  QueryPerformanceFrequency(&cprt_frequency);
  QueryPerformanceCounter(&cprt_start_time);
}  /* cprt_inittime */


void cprt_gettime(struct cprt_timespec *ts)
{
  LARGE_INTEGER ticks;
  uint64_t ns;

  QueryPerformanceCounter(&ticks);
  ns = ticks.QuadPart - cprt_start_time.QuadPart;
  ns *= (1000000000/cprt_frequency.QuadPart);

  ts->tv_sec = (time_t)(ns / 1000000000);
  ts->tv_nsec = (long)(ns % 1000000000);
}  /* cprt_gettime */


#elif defined(__APPLE__)
void cprt_inittime()
{
}  /* cprt_inittime */


#else  /* Non-Apple Unixes */
void cprt_inittime()
{
}  /* cprt_inittime */


#endif


void cprt_sleep_ns(uint64_t duration_ns)
{
  uint64_t ns_so_far;
  struct cprt_timespec cur_ts;
  struct cprt_timespec start_ts;

  CPRT_GETTIME(&start_ts);
  cur_ts = start_ts;
  do {  /* while */
    CPRT_DIFF_TS(ns_so_far, cur_ts, start_ts);
    CPRT_GETTIME(&cur_ts);
  } while (ns_so_far < duration_ns);
}  /* cprt_sleep_ns */


void cprt_localtime_r(time_t *timep, struct tm *result)
{
#if defined(_WIN32)
  int i = localtime_s(result, timep);
#else  /* Unixes */
  localtime_r(timep, result);
#endif
}  /* cprt_localtime_r */


char *cprt_strerror(int errnum, char *buffer, size_t buf_sz)
{
#if defined(_WIN32)
  strerror_s(buffer, buf_sz, errnum);

#elif defined(__linux__)
  char work_buffer[1024];
  char *err_str;

  /* Note that this uses the GNU-variant of strerror_r. */
  err_str = strerror_r(errnum, work_buffer, sizeof(work_buffer));
  if (err_str != NULL) {
    strncpy(buffer, err_str, buf_sz);
    buffer[buf_sz-1] = '\0';  /* make sure it has a null term. */
  }

#else  /* Non-Linux Unix. */
  strerror_r(errnum, buffer, buf_sz);

#endif

  return buffer;
}  /* cprt_strerror */


void cprt_perrno(char *in_str, char *file, int line)
{
#if defined(_WIN32)
  DWORD my_errno = errno;
  if (my_errno != 0) {
    char my_errstr[1024];
    cprt_strerror(my_errno, my_errstr, sizeof(my_errstr));
    fprintf(stderr, "ERROR (%s:%d): %s: errno=%u: '%s'\n",
        CPRT_BASENAME(file), line, in_str,
        my_errno, my_errstr);
  }
  else {
    char *rtn_msg;
    DWORD err;
    my_errno = WSAGetLastError();
    err = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, my_errno, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&rtn_msg, 0, NULL);
    if (err > 0) { /* If format worked */
      fprintf(stderr, "ERROR (%s:%d): %s: WSAGetLastError=%d: %s",
          CPRT_BASENAME(file), line, in_str, my_errno, rtn_msg);
      LocalFree(rtn_msg);
    }
    else {
      fprintf(stderr, "ERROR (%s:%d): %s: WSAGetLastError=%d (no description)\n",
          CPRT_BASENAME(file), line, in_str, my_errno);
    }
  }
  fflush(stderr);

#else  /* Unix. */
  char my_errno = errno;
  char my_errstr[1024];
  cprt_strerror(my_errno, my_errstr, sizeof(my_errstr));
  fprintf(stderr, "ERROR (%s:%d): %s: errno=%u: %s\n",
      CPRT_BASENAME(file), line, in_str,
      my_errno, my_errstr);
  fflush(stderr);

#endif
}  /* cprt_perrno */


/* Get date/time stamp (date optional) with up to microsecond precision.
 * Returns passed-in string pointer for convenience. */
char *cprt_timestamp(char *str, int bufsz, int do_date, int precision)
{
  static unsigned long long pow_10[7] = { 1, 10, 100, 1000, 10000, 100000, 1000000 };
  struct cprt_timeval cur_time_tv;
  struct tm tm_buf;
  char *rtn_str = str;

  CPRT_TIMEOFDAY(&cur_time_tv, NULL);
  CPRT_LOCALTIME_R(&cur_time_tv.tv_sec, &tm_buf);  /* Break down current time. */

  if (do_date && precision > 0) {
    CPRT_SNPRINTF(str, bufsz, "%04d-%02d-%02d %02d:%02d:%02d.%0*d",
        (int)tm_buf.tm_year + 1900, (int)tm_buf.tm_mon + 1, (int)tm_buf.tm_mday,
        (int)tm_buf.tm_hour, (int)tm_buf.tm_min, (int)tm_buf.tm_sec,
        precision, (int)(cur_time_tv.tv_usec / pow_10[6 - precision]));
  }
  else if (do_date && precision == 0) {
    CPRT_SNPRINTF(str, bufsz, "%04d-%02d-%02d %02d:%02d:%02d",
        (int)tm_buf.tm_year + 1900, (int)tm_buf.tm_mon + 1, (int)tm_buf.tm_mday,
        (int)tm_buf.tm_hour, (int)tm_buf.tm_min, (int)tm_buf.tm_sec);
  }
  else if (!do_date && precision > 0) {
    CPRT_SNPRINTF(str, bufsz, "%02d:%02d:%02d.%0*d",
        (int)tm_buf.tm_hour, (int)tm_buf.tm_min, (int)tm_buf.tm_sec,
        precision, (int)(cur_time_tv.tv_usec / pow_10[6 - precision]));
  }
  else {  /* !do_date && precision==0 */
    CPRT_SNPRINTF(str, bufsz, "%02d:%02d:%02d",
        (int)tm_buf.tm_hour, (int)tm_buf.tm_min, (int)tm_buf.tm_sec);
  }

  return rtn_str;
}  /* cprt_timestamp */


/* Called like printf but prints ms-resolution "delta" timestamp.
 * Also flushes stdout. */
void cprt_vts_fprintf(FILE *fp, const char *format, va_list argp)
{
  size_t fmt_len, ts_len;
  char *fmt_buf;

  /* Create new format string with timestamp prepended to it. */
  fmt_len = strlen(format) + 32;  /* Allows yyyy-mmm-dd hh:mm:ss.uuuuuuuu: */
  fmt_buf = malloc(fmt_len);
  cprt_timestamp(fmt_buf, 32, 1, 3);  /* Include date and 3 decimals for seconds. */
  ts_len = strlen(fmt_buf);
  snprintf(&fmt_buf[ts_len], fmt_len - ts_len, ": %s", format);

  vfprintf(fp, fmt_buf, argp);   /* Pass in new format string. */
  fflush(fp);

  free(fmt_buf);
}  /* cprt_vts_fprintf */


void cprt_ts_printf(const char *format, ...)
{
  va_list argp;
  va_start(argp, format);  /* Tell va_* where the start of argp is. */
  cprt_vts_fprintf(stdout, format, argp);   /* Pass in new format string. */
  va_end(argp);
}  /* cprt_ts_printf */


void cprt_ts_eprintf(const char *format, ...)
{
  va_list argp;
  va_start(argp, format);  /* Tell va_* where the start of argp is. */
  cprt_vts_fprintf(stderr, format, argp);   /* Pass in new format string. */
  va_end(argp);
}  /* cprt_ts_eprintf */


/* This produces wall clock seconds after Unix epoc to ms precision. */
uint64_t cprt_get_ms_time()
{
    struct cprt_timeval tv;

    CPRT_TIMEOFDAY(&tv, NULL);
    return ((uint64_t)tv.tv_sec * 1000) + ((uint64_t)tv.tv_usec / 1000);
}  /* cprt_get_ms_time */


/* Called like printf but prints ms-resolution "delta" timestamp.
 * Also flushes stdout. */
void cprt_vms_fprintf(FILE *fp, uint64_t start_ms, const char *format, va_list argp)
{
  size_t fmt_len;
  char *fmt_buf;
  uint64_t cur_ms = cprt_get_ms_time();

  /* Create new format string with timestamp prepended to it. */
  fmt_len = strlen(format) + 30;  /* Allows up to 24 digits of seconds. */
  fmt_buf = malloc(fmt_len);
  snprintf(fmt_buf, fmt_len, "%"PRIu64".%03"PRIu64": %s",
      (cur_ms - start_ms)/1000, (cur_ms - start_ms) % 1000, format);

  /* Do the printf. */
  vfprintf(fp, fmt_buf, argp);   /* Pass in new format string. */
  fflush(fp);

  free(fmt_buf);
}  /* cprt_vms_fprintf */


void cprt_ms_printf(uint64_t start_ms, const char *format, ...)
{
  va_list argp;
  va_start(argp, format);  /* Tell va_* where the start of argp is. */
  cprt_vms_fprintf(stdout, start_ms, format, argp);   /* Pass in new format string. */
  va_end(argp);
}  /* cprt_ms_printf */


void cprt_ms_eprintf(uint64_t start_ms, const char *format, ...)
{
  va_list argp;
  va_start(argp, format);  /* Tell va_* where the start of argp is. */
  cprt_vms_fprintf(stderr, start_ms, format, argp);   /* Pass in new format string. */
  va_end(argp);
}  /* cprt_ms_eprintf */


void cprt_set_affinity(uint64_t in_mask)
{
#if defined(_WIN32)
  DWORD_PTR rc;
  rc = SetThreadAffinityMask(GetCurrentThread(), in_mask);
  if (rc == 0) {
    errno = GetLastError();
    CPRT_PERRNO("SetThreadAffinityMask");
  }

#elif defined(__linux__)
  cpu_set_t cpuset;
  int i;
  uint64_t bit = 1;
  CPU_ZERO(&cpuset);
  for (i = 0; i < 64; i++) {
    if ((in_mask & bit) == bit) {
      CPU_SET(i, &cpuset);
    }
    bit = bit << 1;
  }
  CPRT_EOK0(errno = pthread_setaffinity_np(
      pthread_self(), sizeof(cpuset), &cpuset));

#else /* Non-Linux Unix. */
#endif
}  /* cprt_set_affinity */


/* Return 0 on success, -1 on error (sets errno). */
int cprt_try_affinity(uint64_t in_mask)
{
#if defined(_WIN32)
  DWORD_PTR rc;
  rc = SetThreadAffinityMask(GetCurrentThread(), in_mask);
  if (rc == 0) {
    errno = GetLastError();
    return -1;
  }

#elif defined(__linux__)
  cpu_set_t cpuset;
  int i;
  uint64_t bit = 1;
  CPU_ZERO(&cpuset);
  for (i = 0; i < 64; i++) {
    if ((in_mask & bit) == bit) {
      CPU_SET(i, &cpuset);
    }
    bit = bit << 1;
  }
  errno = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  if (errno != 0) {
    return -1;
  }

#else /* Non-Linux Unix. */
#endif
  return 0;
}  /* cprt_try_affinity */


#define CPRT_MAX_EVENTS 1024
int cprt_num_events = 0;
int cprt_events[CPRT_MAX_EVENTS];
void cprt_event(int event)
{
  cprt_events[(CPRT_ATOMIC_INC_VAL(&cprt_num_events) - 1) % CPRT_MAX_EVENTS] =
      event;
}  /* cprt_event */

void cprt_dump_events(FILE *fd)
{
  int i, n;
  n = cprt_num_events;
  printf("cprt_num_events=%d\n", n);
  for (i = 1; i <= CPRT_MAX_EVENTS; i++) {
    fprintf(fd, "  cprt_event[%d] = %09d\n",
        (n - i), cprt_events[(n - i) % CPRT_MAX_EVENTS]);
    if (n == i) {
      break;  /* There were less than CPRT_MAX_EVENTS events. */
    }
  }
}  /* cprt_dump_events */


/* Portable getopt(). */
char* cprt_optarg;
int cprt_optopt;
int cprt_optind = 1;  /* Next argv to process. */
int cprt_opterr = 1;  /* Print errors to stderr. */

int cprt_getopt(int argc, char *const argv[], const char *optstring)
{
  char *argv_str;
  char *opt_spec;  /* Option specifier in optstring. */
  int argv_len;

  cprt_optarg = NULL;
  cprt_optopt = 0;

  if (cprt_optind >= argc) {
    /* End of command line. */
    return EOF;
  };

  argv_str = argv[cprt_optind];

  argv_len = (int)strlen(argv_str);
  if (argv_len < 2) {
    /* A single character, even if a '-', is not an option. Its an arg. */
    return EOF;
  }
  if (argv_str[0] != '-') {
    /* No more options, this argv is the first arg. */
    return EOF;
  }
  if (strcmp(argv_str, "--") == 0) {
    /* Special "--" option; next argv is arg. */
    cprt_optind++;
    return EOF;
  }

  cprt_optopt = argv_str[1];
  if (! isgraph(cprt_optopt)) {
    if (cprt_opterr != 0) {
      fprintf(stderr, "Error, argv[%d] has non-printable character.\n", cprt_optind);
    }
    return '?';
  }
  opt_spec = strchr("-:;", cprt_optopt);
  if (strchr("-:;", cprt_optopt) != NULL) {
    if (cprt_opterr != 0) {
      fprintf(stderr, "Error, '%c' not a valid option character.\n", cprt_optopt);
    }
    return '?';
  }

  opt_spec = strchr(optstring, cprt_optopt);
  if (opt_spec == NULL) {
    if (cprt_opterr != 0) {
      fprintf(stderr, "Error, '%c' not a defined option.\n", cprt_optopt);
    }
    return '?';
  }

  /* User specified a valid option. See if needs value. */
  if (opt_spec[1] == ':') {
    /* Value is needed. See if it's part of the same argv. */
    if (argv_len > 2) {
      cprt_optarg = &argv_str[2];
      cprt_optind++;  /* Next option. */
      return cprt_optopt;
    }

    cprt_optind++;  /* Value must be next argv. */
    if (cprt_optind >= argc) {
      /* Ran out of argvs, value not supplied. */
      if (cprt_opterr != 0) {
        fprintf(stderr, "Error, '-%c' requires value.\n", cprt_optopt);
      }
      return '?';
    }
    cprt_optarg = argv[cprt_optind];
    cprt_optind++;  /* Next option. */
    return cprt_optopt;
  }
  else {  /* Option has no value. */
    if (argv_len > 2) {
      /* This getopt() implementation does not support multiple options
       * in one argv. */
      if (cprt_opterr != 0) {
        fprintf(stderr, "Error, '-%c' must have no value.\n", cprt_optopt);
      }
      return '?';
    }
    cprt_optind++;  /* Next option. */
    return cprt_optopt;
  }
}  /* cprt_getopt */
