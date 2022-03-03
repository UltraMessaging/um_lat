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
#include <string.h>
#include <time.h>
#include <errno.h>

#if defined(_WIN32)
LARGE_INTEGER cprt_frequency;
#endif


#if defined(_WIN32)
void cprt_inittime()
{
  QueryPerformanceFrequency(&cprt_frequency);
}  /* cprt_inittime */

void cprt_gettime(struct timespec *ts)
{
  LARGE_INTEGER ticks;
  uint64_t ns;

  QueryPerformanceCounter(&ticks);
  ns = ticks.QuadPart;
  ns *= 1000000000;
  ns /= cprt_frequency.QuadPart;

  ts->tv_sec = ns / 1000000000;
  ts->tv_sec = ns % 1000000000;
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


#if defined(_WIN32)
/*******************************************************************************
 * Copyright (c) 2012-2017, Kim Grasman <kim.grasman@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Kim Grasman nor the
 *     names of contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL KIM GRASMAN BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

char* optarg;
int optopt;
/* The variable optind [...] shall be initialized to 1 by the system. */
int optind = 1;
int opterr;

static char* optcursor = NULL;

/* Implemented based on [1] and [2] for optional arguments.
   optopt is handled FreeBSD-style, per [3].
   Other GNU and FreeBSD extensions are purely accidental.

[1] http://pubs.opengroup.org/onlinepubs/000095399/functions/getopt.html
[2] http://www.kernel.org/doc/man-pages/online/pages/man3/getopt.3.html
[3] http://www.freebsd.org/cgi/man.cgi?query=getopt&sektion=3&manpath=FreeBSD+9.0-RELEASE
*/
int getopt(int argc, char* const argv[], const char* optstring) {
  int optchar = -1;
  const char* optdecl = NULL;

  optarg = NULL;
  opterr = 0;
  optopt = 0;

  /* Unspecified, but we need it to avoid overrunning the argv bounds. */
  if (optind >= argc)
    goto no_more_optchars;

  /* If, when getopt() is called argv[optind] is a null pointer, getopt()
     shall return -1 without changing optind. */
  if (argv[optind] == NULL)
    goto no_more_optchars;

  /* If, when getopt() is called *argv[optind]  is not the character '-',
     getopt() shall return -1 without changing optind. */
  if (*argv[optind] != '-')
    goto no_more_optchars;

  /* If, when getopt() is called argv[optind] points to the string "-",
     getopt() shall return -1 without changing optind. */
  if (strcmp(argv[optind], "-") == 0)
    goto no_more_optchars;

  /* If, when getopt() is called argv[optind] points to the string "--",
     getopt() shall return -1 after incrementing optind. */
  if (strcmp(argv[optind], "--") == 0) {
    ++optind;
    goto no_more_optchars;
  }

  if (optcursor == NULL || *optcursor == '\0')
    optcursor = argv[optind] + 1;

  optchar = *optcursor;

  /* FreeBSD: The variable optopt saves the last known option character
     returned by getopt(). */
  optopt = optchar;

  /* The getopt() function shall return the next option character (if one is
     found) from argv that matches a character in optstring, if there is
     one that matches. */
  optdecl = strchr(optstring, optchar);
  if (optdecl) {
    /* [I]f a character is followed by a colon, the option takes an
       argument. */
    if (optdecl[1] == ':') {
      optarg = ++optcursor;
      if (*optarg == '\0') {
        /* GNU extension: Two colons mean an option takes an
           optional arg; if there is text in the current argv-element
           (i.e., in the same word as the option name itself, for example,
           "-oarg"), then it is returned in optarg, otherwise optarg is set
           to zero. */
        if (optdecl[2] != ':') {
          /* If the option was the last character in the string pointed to by
             an element of argv, then optarg shall contain the next element
             of argv, and optind shall be incremented by 2. If the resulting
             value of optind is greater than argc, this indicates a missing
             option-argument, and getopt() shall return an error indication.

             Otherwise, optarg shall point to the string following the
             option character in that element of argv, and optind shall be
             incremented by 1.
          */
          if (++optind < argc) {
            optarg = argv[optind];
          } else {
            /* If it detects a missing option-argument, it shall return the
               colon character ( ':' ) if the first character of optstring
               was a colon, or a question-mark character ( '?' ) otherwise.
            */
            optarg = NULL;
            optchar = (optstring[0] == ':') ? ':' : '?';
          }
        } else {
          optarg = NULL;
        }
      }

      optcursor = NULL;
    }
  } else {
    /* If getopt() encounters an option character that is not contained in
       optstring, it shall return the question-mark ( '?' ) character. */
    optchar = '?';
  }

  if (optcursor == NULL || *++optcursor == '\0')
    ++optind;

  return optchar;

no_more_optchars:
  optcursor = NULL;
  return -1;
}
#endif
