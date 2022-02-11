/* cprt.h - for portability. */
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
#ifndef CPRT_H
#define CPRT_H

#if defined(_WIN32)
  #include <winsock2.h>
  #pragma comment(lib, "Ws2_32.lib")
  #pragma warning(disable : 4996)

#else  /* Unix */
  #include <unistd.h>
  #include <stdlib.h>
  #include <errno.h>
  #include <time.h>
  #include <pthread.h>
  #if defined(__APPLE__)
    #include <dispatch/dispatch.h>
  #else  /* Non-Apple Unixes. */
    #include <sched.h>
    #include <semaphore.h>
  #endif
#endif

#include <inttypes.h>


#ifdef __cplusplus
extern "C" {
#endif

/* Macro to approximate the basename() function. */
#if defined(_WIN32)
  #define CPRT_BASENAME(_p) ((strrchr(_p, '\\') == NULL) ? (_p) : (strrchr(_p, '\\')+1))
#else
  #define CPRT_BASENAME(_p) ((strrchr(_p, '/') == NULL) ? (_p) : (strrchr(_p, '/')+1))
#endif

/* Macro to print errno in human-readable form and exit(1). */
#define CPRT_PERRNO(cprt_perrno_in_str_) do { \
  char cprt_perrno_errno_ = errno; \
  char cprt_perrno_errstr_[1024]; \
  cprt_strerror(cprt_perrno_errno_, cprt_perrno_errstr_, sizeof(cprt_perrno_errstr_)); \
  fprintf(stderr, "ERROR (%s:%d): %s: errno=%u ('%s')\n", \
      CPRT_BASENAME(__FILE__), __LINE__, cprt_perrno_in_str_, \
      cprt_perrno_errno_, cprt_perrno_errstr_); \
  fflush(stderr); \
  exit(1); \
} while (0)

/* Use when non-zero means error. */
#define CPRT_EOK0(cprt_eok0_expr) do { \
  if ((cprt_eok0_expr) != 0) { \
    int cprt_eok0_errno = errno; \
    char cprt_eok0_errstr[1024]; \
    CPRT_SNPRINTF(cprt_eok0_errstr, sizeof(cprt_eok0_errstr), "'%s' is not 0", #cprt_eok0_expr); \
    errno = cprt_eok0_errno; \
    CPRT_PERRNO(cprt_eok0_errstr); \
  } \
} while (0)

/* Use when NULL means error. */
#define CPRT_ENULL(cprt_enull_expr) do { \
  if ((cprt_enull_expr) == NULL) { \
    int cprt_enull_errno = errno; \
    char cprt_enull_errstr[1024]; \
    CPRT_SNPRINTF(cprt_enull_errstr, sizeof(cprt_enull_errstr), "'%s' is NULL", #cprt_enull_expr); \
    errno = cprt_enull_errno; \
    CPRT_PERRNO(cprt_enull_errstr); \
  } \
} while (0)

/* Use when -1 means error. */
#define CPRT_EM1(cprt_em1_expr) do { \
  if ((cprt_em1_expr) == -1) { \
    int cprt_em1_errno = errno; \
    char cprt_em1_errstr[1024]; \
    CPRT_SNPRINTF(cprt_em1_errstr, sizeof(cprt_em1_errstr), "'%s' is -1", #cprt_em1_expr); \
    errno = cprt_em1_errno; \
    CPRT_PERRNO(cprt_em1_errstr); \
  } \
} while (0)

#define CPRT_ASSERT(cprt_assert_cond) do { \
  if (! (cprt_assert_cond)) { \
    fprintf(stderr, "ERROR (%s:%d): ERROR: '%s' not true\n", \
      CPRT_BASENAME(__FILE__), __LINE__, #cprt_assert_cond); \
    fflush(stderr); \
    exit(1); \
  } \
} while (0)

#define CPRT_ABORT(cprt_abort_in_str) do { \
  fprintf(stderr, "ERROR (%s:%d): ABORT: %s\n", \
    CPRT_BASENAME(__FILE__), __LINE__, cprt_abort_in_str); \
  fflush(stderr); \
  abort(); \
} while (0)


/* Macro to make it easier to use sscanf().  See:
 *   https://stackoverflow.com/questions/25410690
 */
#define CPRT_STRDEF2(cprt_strdef2_in_str) #cprt_strdef2_in_str
#define CPRT_STRDEF(cprt_strdef_in_str) CPRT_CPRT_STRDEF2(cprt_strdef_in_str)

/* See http://blog.geeky-boy.com/2014/06/clangllvm-optimize-o3-understands-free.html
 *  for why the CPRT_VOL32 macro is needed.
 */
#define CPRT_VOL32(cprt_vol32_ptr) (*(volatile uint32_t *)&(cprt_vol32_ptr))


/* See https://github.com/fordsfords/safe_atoi */
#define CPRT_ATOI(a_,r_) do { \
  char *in_a_ = (a_); \
  int new_errno_; \
  unsigned long long fs_[9] = {  /* All '1's by variable size. */ \
    0, 0xff, 0xffff, 0, 0xffffffff, 0, 0, 0, 0xffffffffffffffff }; \
  (r_) = fs_[sizeof(r_)]; \
  if ((r_) < 0) { /* Is result a signed value? */ \
    char *temp_ = NULL;  long long llresult_; \
    if (strlen(in_a_) > 2 && in_a_[0] == '0' && (in_a_[1] == 'x' || in_a_[1] == 'X')) { \
      in_a_ += 2;  /* Skip past '0x'. */ \
      errno = 0; \
      llresult_ = strtoll(in_a_, &temp_, 16); \
      new_errno_ = errno; \
    } else { \
      errno = 0; \
      llresult_ = strtoll(in_a_, &temp_, 10); \
      new_errno_ = errno; \
    } \
    if (new_errno_ != 0 || temp_ == in_a_ || temp_ == NULL || *temp_ != '\0') { \
      if (new_errno_ == 0) { \
        new_errno_ = EINVAL; \
      } \
      fprintf(stderr, "%s:%d, Error, invalid number for %s: '%s'\n", \
         __FILE__, __LINE__, #r_, in_a_); \
    } else { /* strtol thinks success; check for overflow. */ \
      (r_) = llresult_; /* "return" value of macro */ \
      if ((r_) != llresult_) { \
        fprintf(stderr, "%s:%d, %s over/under flow: '%s'\n", \
           __FILE__, __LINE__, #r_, in_a_); \
        new_errno_ = ERANGE; \
      } \
    } \
  } else { \
    char *temp_ = NULL;  unsigned long long llresult_; \
    if (strlen(in_a_) > 2 && in_a_[0] == '0' && (in_a_[1] == 'x' || in_a_[1] == 'X')) { \
      in_a_ += 2;  /* Skip past '0x'. */ \
      errno = 0; \
      llresult_ = strtoull(in_a_, &temp_, 16); \
      new_errno_ = errno; \
    } else { \
      errno = 0; \
      llresult_ = strtoull(in_a_, &temp_, 10); \
      new_errno_ = errno; \
    } \
    if (new_errno_ != 0 || temp_ == in_a_ || temp_ == NULL || *temp_ != '\0') { \
      if (new_errno_ == 0) { \
        new_errno_ = EINVAL; \
      } \
      fprintf(stderr, "%s:%d, Error, invalid number for %s: '%s'\n", \
         __FILE__, __LINE__, #r_, in_a_); \
    } else { /* strtol thinks success; check for overflow. */ \
      (r_) = llresult_; /* "return" value of macro */ \
      if ((r_) != llresult_) { \
        fprintf(stderr, "%s:%d, %s over/under flow: '%s'\n", \
           __FILE__, __LINE__, #r_, in_a_); \
        new_errno_ = ERANGE; \
      } \
    } \
  } \
  errno = new_errno_; \
  CPRT_EOK0(errno); \
} while (0)


#if defined(_WIN32)
  #define CPRT_NET_START do { \
    WSADATA cprt_net_start_wsadata; \
    errno = WSAStartup(MAKEWORD(2,2), &cprt_net_start_wsadata); \
    CPRT_EOK0(errno); \
  } while (0)
  #define CPRT_NET_CLEANUP do { \
    int cprt_net_start_e; \
    cprt_net_start_e = WSACleanup(); \
    if (cprt_net_start_e != 0) { \
      errno = GetLastError(); \
      CPRT_PERRNO("WSACleanup"); \
    } \
  } while (0)

#else  /* Unix */
  #define CPRT_NET_START do { \
    errno = 0; \
  } while (0)
  #define CPRT_NET_CLEANUP do { \
    errno = 0; \
  } while (0)
#endif


#if defined(_WIN32)
  #define CPRT_SNPRINTF _snprintf
  #define CPRT_STRDUP _strdup
  #define CPRT_SLEEP_SEC(s_) Sleep((s_)*1000)
  #define CPRT_SLEEP_MS Sleep
  #define CPRT_STRTOK strtok_s

#else  /* Unix */
  #define CPRT_SNPRINTF snprintf
  #define CPRT_STRDUP strdup
  #define CPRT_SLEEP_SEC sleep
  #define CPRT_SLEEP_MS(ms_) usleep((ms_)*1000)
  #define CPRT_STRTOK strtok_r
#endif


#if defined(_WIN32)
  #define CPRT_MUTEX_T CRITICAL_SECTION
  #define CPRT_MUTEX_INIT(_m) InitializeCriticalSection(&(_m))
  #define CPRT_MUTEX_INIT_RECURSIVE(_m) InitializeCriticalSection(&(_m))
  #define CPRT_MUTEX_LOCK(_m) EnterCriticalSection(&(_m))
  #define CPRT_MUTEX_TRYLOCK(_got_it, _m) (_got_it) = TryEnterCriticalSection(&(_m))
  #define CPRT_MUTEX_UNLOCK(_m) LeaveCriticalSection(&(_m))
  #define CPRT_MUTEX_DELETE(_m) DeleteCriticalSection(&(_m))

#else  /* Unix */
  #define CPRT_MUTEX_T pthread_mutex_t
  #define CPRT_MUTEX_INIT_RECURSIVE(_m) do { \
    pthread_mutexattr_t _mutexattr; \
    CPRT_EOK0(errno = pthread_mutexattr_init(&_mutexattr)); \
    CPRT_EOK0(errno = pthread_mutexattr_settype(&_mutexattr, \
        PTHREAD_MUTEX_RECURSIVE)); \
    CPRT_EOK0(errno = pthread_mutex_init(&(_m),&_mutexattr)); \
    CPRT_EOK0(errno = pthread_mutexattr_destroy(&_mutexattr)); \
  } while(0)
  #define CPRT_MUTEX_INIT(_m) CPRT_EOK0(errno = pthread_mutex_init(&(_m), NULL))
  #define CPRT_MUTEX_LOCK(_m) CPRT_EOK0(errno = pthread_mutex_lock(&(_m)))
  #define CPRT_MUTEX_TRYLOCK(_got_it, _m) do { \
    errno = pthread_mutex_trylock(&(_m)); \
    if (errno == 0) { \
      _got_it = 1; \
    } else if (errno == EBUSY) { \
      _got_it = 0; \
    } else { \
      CPRT_PERRNO("pthread_mutex_trylock"); \
    } \
  } while (0)
  #define CPRT_MUTEX_UNLOCK(_m) pthread_mutex_unlock(&(_m))
  #define CPRT_MUTEX_DELETE(_m) pthread_mutex_destroy(&(_m))
#endif


#if defined(_WIN32)
  #define CCPRT_SEM_T prt_sem_t;
  #define CPRT_SEM_INIT(_s, _i) do { \
    (_s) = CreateSemaphore(NULL, _i, INT_MAX, NULL); \
    if ((_s) == NULL) { \
      errno = GetLastError();\
      CPRT_PERRNO("CreateThread"); \
    } \
  } while (0)
  #define CPRT_SEM_DELETE(_s) do { \
    BOOL rc_ = CloseHandle(_s); \
    if (rc_ == 0) { \
      errno = GetLastError();\
      CPRT_PERRNO("CreateThread"); \
    } \
  } while (0)
  #define CPRT_SEM_POST(_s) do { \
    BOOL rc_ = ReleaseSemaphore(_s, 1, NULL); \
    if (rc_ == 0) { \
      errno = GetLastError();\
      CPRT_PERRNO("CreateThread"); \
    } \
  } while (0)
  #define CPRT_SEM_WAIT(_s) do { \
    WaitForSingleObject(_s, INFINITE); \
  } while (0)

#elif defined(__APPLE__)
  #define CCPRT_SEM_T dispatch_semaphore_t
  #define CPRT_SEM_INIT(_s, _i) _s = dispatch_semaphore_create(_i)
  #define CPRT_SEM_DELETE(_s) dispatch_release(_s)
  #define CPRT_SEM_POST(_s) dispatch_semaphore_signal(_s)
  #define CPRT_SEM_WAIT(_s) dispatch_semaphore_wait(_s, DISPATCH_TIME_FOREVER)

#else  /* Non-Apple Unixes */
  #define CCPRT_SEM_T sem_t
  #define CPRT_SEM_INIT(_s, _i) CPRT_EOK0(sem_init(&(_s), 0, _i))
  #define CPRT_SEM_DELETE(_s) CPRT_EOK0(sem_destroy(&(_s)))
  #define CPRT_SEM_POST(_s) CPRT_EOK0(sem_post(&(_s)))
  #define CPRT_SEM_WAIT(_s) CPRT_EOK0(sem_wait(&(_s)))
#endif


#if defined(_WIN32)
  #define CPRT_THREAD_T HANDLE
  #define CPRT_THREAD_ENTRYPOINT DWORD WINAPI
  #define CPRT_THREAD_CREATE(_tid, _tstrt, _targ) do {\
    DWORD _ignore;\
    _tid = CreateThread(NULL, 0, _tstrt, _targ, 0, &_ignore);\
    if (_tid == NULL) {\
      errno = GetLastError();\
      CPRT_PERRNO("CreateThread"); \
    }\
  } while (0)
  #define CPRT_THREAD_EXIT do { ExitThread(0); } while (0)
  #define CPRT_THREAD_JOIN(_tid) WaitForSingleObject(_tid, INFINITE)

#else  /* Unix */
  #define CPRT_THREAD_T pthread_t
  #define CPRT_THREAD_ENTRYPOINT void *
  #define CPRT_THREAD_CREATE(_tid, _tstrt, _targ) \
    CPRT_EOK0(errno = pthread_create(&(_tid), NULL, _tstrt, _targ))
  #define CPRT_THREAD_EXIT pthread_exit(NULL)
  #define CPRT_THREAD_JOIN(_tid) \
    CPRT_EOK0(errno = pthread_join(_tid, NULL))
#endif

#define CPRT_CPU_ZERO(_cprt_cpuset) do { \
  uint64_t *_cprt_cpuset_p = (_cprt_cpuset); \
  *_cprt_cpuset_p = 0; \
} while (0)

#define CPRT_CPU_SET(_cprt_cpunum, _cprt_cpuset) do { \
  uint64_t *_cprt_cpuset_p = (_cprt_cpuset); \
  if (_cprt_cpunum == 0) { \
    *_cprt_cpuset_p |= 1; \
  } else { \
    *_cprt_cpuset_p |= (1ull << _cprt_cpunum); \
  } \
} while (0)


#define CPRT_INITTIME cprt_inittime
#if defined(_WIN32)
  struct timespec {
    long tv_sec;
    long tv_nsec;
  }
  #define CPRT_GETTIME(_ts) cprt_gettime(_ts)
#elif defined(__APPLE__)
  #define CPRT_GETTIME(_ts) clock_gettime(CLOCK_MONOTONIC_RAW, _ts)
#else  /* Non-Apple Unixes */
  #define CPRT_GETTIME(_ts) clock_gettime(CLOCK_MONOTONIC, _ts)
#endif
#define CPRT_DIFF_TS(diff_ts_result_ns_, diff_ts_end_ts_, diff_ts_start_ts_) do { \
  (diff_ts_result_ns_) = (((uint64_t)diff_ts_end_ts_.tv_sec \
                           - (uint64_t)diff_ts_start_ts_.tv_sec) * 1000000000 \
                          + (uint64_t)diff_ts_end_ts_.tv_nsec) \
                         - (uint64_t)diff_ts_start_ts_.tv_nsec; \
} while (0)  /* DIFF_TS */

/* externals in cprt.c. */
char *cprt_strerror(int errnum, char *buffer, size_t buf_sz);
void cprt_set_affinity(uint64_t in_mask);
void cprt_inittime();

#if defined(_WIN32)
int cprt_win_gettime(struct timespec *tp);
extern char* optarg;
extern int optopt;
extern int optind;
extern int opterr;
int getopt(int argc, char* const argv[], const char* optstring);
#endif

#if defined(__cplusplus)
}
#endif

#endif  /* CPRT_H */
