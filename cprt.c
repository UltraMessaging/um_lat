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

  GetSystemTimeAsFileTime (&tfile);
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

  argv_len = strlen(argv_str);
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
