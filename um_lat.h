/* um_lat.h */
/*
  Copyright (c) 2021-2022 Informatica Corporation
  Permission is granted to licensees to use or alter this software for any
  purpose, including commercial applications, according to the terms laid
  out in the Software License Agreement.

  This source code example is provided by Informatica for educational
  and evaluation purposes only.

  THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES 
  EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF 
  NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR 
  PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE 
  UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES,
  BE LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR 
  INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE 
  TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF 
  THE LIKELIHOOD OF SUCH DAMAGES.
*/

#ifndef UM_LAT_H
#define UM_LAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* This is needed for affinity setting. */
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <inttypes.h>

/* Simple error handling macros:
 * E() - for UM API calls. Checks for error (return != 0). If error, prints
 *     file:line and error message, exits with bad status (1).
 * ASSRT() - test condition. If false, prints file:line and error message,
 *     exits with bad status (1).
 */

#define ASSRT(cond_) do { \
  if (! (cond_)) { \
    fprintf(stderr, "%s:%d, ERROR: '%s' not true\n", \
      __FILE__, __LINE__, #cond_); \
    fflush(stderr); \
    exit(1); \
  } \
} while (0)  /* ASSRT */

#define E(lbm_funct_call_) do { \
  int e_ = (lbm_funct_call_); \
  if (e_ == LBM_FAILURE) { \
    fprintf(stderr, "ERROR (%s:%d): %s failed: '%s'\n", \
       __FILE__, __LINE__, #lbm_funct_call_, lbm_errmsg()); \
    fflush(stderr); \
    exit(1); \
  } \
} while (0)  /* E */


#define FLAGS_TIMESTAMP    0x01
#define FLAGS_NON_BLOCKING 0x02
#define FLAGS_GENERIC_SRC  0x04

struct perf_msg_s {
  uint32_t flags;
  uint32_t msg_num;
  struct timespec send_ts;
};
typedef struct perf_msg_s perf_msg_t;

#if defined(__cplusplus)
}
#endif

#endif  /* UM_LAT_H */
