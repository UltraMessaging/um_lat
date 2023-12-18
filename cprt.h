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

/* Action to take on "fatal" error. */
#define CPRT_ERR_EXIT do { \
  CPRT_NET_CLEANUP; \
  exit(1); \
} while (0)

#include <stdio.h>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <mswsock.h>
  #include <mstcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  #pragma warning(disable : 4996)
  typedef unsigned __int8 uint8_t;
  typedef unsigned __int16 uint16_t;
  typedef unsigned __int32 uint32_t;
  typedef unsigned __int64 uint64_t;
  /* C99 printf format macros missing from VC. */
  #define PRId8 "d"
  #define PRId16 "d"
  #define PRId32 "d"
  #define PRId64 "I64d"
  #define PRIu8 "u"
  #define PRIu16 "u"
  #define PRIu32 "u"
  #define PRIu64 "I64u"
  #define PRIx8 "x"
  #define PRIx16 "x"
  #define PRIx32 "x"
  #define PRIx64 "I64x"
  #define PRIuSZ "Iu"
  /* C99 scanf format macros missing from VC. */
  #define SCNd8 "d"
  #define SCNd16 "hd"
  #define SCNd32 "ld"
  #define SCNd64 "I64d"
  #define SCNu8 "u"
  #define SCNu16 "hu"
  #define SCNu32 "lu"
  #define SCNu64 "I64u"
  #define SCNx64 "I64x"
  #define SCNuSZ "Iu"
  #define SCNuSZcast(x) (size_t * )(x)

  #define strtoll _strtoi64
  #define strtoull _strtoui64

#else  /* Unix */
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <errno.h>
  #include <time.h>
  #include <sys/time.h>
  #include <pthread.h>
  #if defined(__APPLE__)
    #include <dispatch/dispatch.h>
  #else  /* Non-Apple Unixes. */
    #include <sched.h>
    #include <semaphore.h>
  #endif
  #include <inttypes.h>
#endif

#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define CPRT_SOCKET SOCKET
  #define CPRT_SOCKET_CLOSE closesocket
  #define CPRT_SHUT_WR SD_SEND
  #define CPRT_SHUT_RD SD_RECEIVE
  #define CPRT_SHUT_RDWR SD_BOTH
#else  /* Unix */
  #define CPRT_SOCKET int
  #define CPRT_SOCKET_CLOSE close
  #define CPRT_SHUT_WR SHUT_WR
  #define CPRT_SHUT_RD SHUT_RD
  #define CPRT_SHUT_RDWR SHUT_RDWR
#endif
#define CPRT_LOCALTIME_R cprt_localtime_r

#if defined(_WIN32)
  #define CPRT_TIMEOFDAY cprt_timeofday
#else  /* Unix */
  #define CPRT_TIMEOFDAY gettimeofday
#endif
#define CPRT_LOCALTIME_R cprt_localtime_r

/* Get usec diff between two struct timeval (used by gettimeofday). */
#define CPRT_DIFF_TV(diff_tv_result_us_, diff_tv_end_us_, diff_tv_start_us_) do { \
  (diff_tv_result_us_) = (((uint64_t)diff_tv_end_us_.tv_sec \
                           - (uint64_t)diff_tv_start_us_.tv_sec) * 1000000000ull \
                          + (uint64_t)diff_tv_end_us_.tv_usec) \
                         - (uint64_t)diff_tv_start_us_.tv_usec; \
} while (0)  /* CPRT_DIFF_TV */


#if defined(_WIN32)
  #define CPRT_ATOMIC_INC_VAL(_p) InterlockedIncrement(_p)
  #define CPRT_ATOMIC_DEC_VAL(_p) InterlockedDecrement(_p)
#else  /* Unix */
  /* GCC supports multiple types. For portability, please limit to "long" (signed or unsigned). */
  #define CPRT_ATOMIC_INC_VAL(_p) __sync_add_and_fetch(_p, 1)
  #define CPRT_ATOMIC_DEC_VAL(_p) __sync_sub_and_fetch(_p, 1)
#endif

/* Macro to approximate the basename() function. */
#if defined(_WIN32)
  #define CPRT_BASENAME(_p) ((strrchr(_p, '\\') == NULL) ? (_p) : (strrchr(_p, '\\')+1))
#else
  #define CPRT_BASENAME(_p) ((strrchr(_p, '/') == NULL) ? (_p) : (strrchr(_p, '/')+1))
#endif

/* Macro to print errno in human-readable form. */
#define CPRT_PERRNO(cprt_perrno_in_str) do { \
  cprt_perrno(cprt_perrno_in_str, __FILE__, __LINE__); \
} while (0)

/* Use when non-zero means error. */
#define CPRT_EOK0(cprt_eok0_expr) do { \
  if ((cprt_eok0_expr) != 0) { \
    int cprt_eok0_errno = errno; \
    char cprt_eok0_errstr[1024]; \
    CPRT_SNPRINTF(cprt_eok0_errstr, sizeof(cprt_eok0_errstr), "'%s' is not 0", #cprt_eok0_expr); \
    errno = cprt_eok0_errno; \
    CPRT_PERRNO(cprt_eok0_errstr); \
    CPRT_ERR_EXIT; \
  } \
} while (0)

/* Use when non-zero means error. */
#define CPRT_EOK1(cprt_eok1_expr) do { \
  if ((cprt_eok1_expr) != 1) { \
    int cprt_eok1_errno = errno; \
    char cprt_eok1_errstr[1024]; \
    CPRT_SNPRINTF(cprt_eok1_errstr, sizeof(cprt_eok1_errstr), "'%s' is not 1", #cprt_eok1_expr); \
    errno = cprt_eok1_errno; \
    CPRT_PERRNO(cprt_eok1_errstr); \
    CPRT_ERR_EXIT; \
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
    CPRT_ERR_EXIT; \
  } \
} while (0)

/* Use when -1 means error. */
#define CPRT_EM1(cprt_em1_expr) do { \
  if ((long)(cprt_em1_expr) == -1) { \
    int cprt_em1_errno = errno; \
    char cprt_em1_errstr[1024]; \
    CPRT_SNPRINTF(cprt_em1_errstr, sizeof(cprt_em1_errstr), "'%s' is -1", #cprt_em1_expr); \
    errno = cprt_em1_errno; \
    CPRT_PERRNO(cprt_em1_errstr); \
    CPRT_ERR_EXIT; \
  } \
} while (0)

#define CPRT_ASSERT(cprt_assert_cond) do { \
  if (! (cprt_assert_cond)) { \
    cprt_ts_eprintf("ERROR (%s:%d): ERROR: '%s' not true\n", \
      CPRT_BASENAME(__FILE__), __LINE__, #cprt_assert_cond); \
    if (cprt_num_events > 0) { cprt_dump_events(stderr); } \
    fflush(stderr); \
    CPRT_ERR_EXIT; \
  } \
} while (0)

#define CPRT_ABORT(cprt_abort_in_str) do { \
  cprt_ts_eprintf("ERROR (%s:%d): ABORT: %s\n", \
    CPRT_BASENAME(__FILE__), __LINE__, cprt_abort_in_str); \
  fflush(stderr); \
  abort(); \
} while (0)


/* Macro to make it easier to use sscanf().  See:
 *   https://stackoverflow.com/questions/25410690
 */
#define CPRT_STRDEF2(cprt_strdef2_in_str) #cprt_strdef2_in_str
#define CPRT_STRDEF(cprt_strdef_in_str) CPRT_STRDEF2(cprt_strdef_in_str)

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
         CPRT_BASENAME(__FILE__), __LINE__, #r_, in_a_); \
    } else { /* strtol thinks success; check for overflow. */ \
      (r_) = llresult_; /* "return" value of macro */ \
      if ((r_) != llresult_) { \
        fprintf(stderr, "%s:%d, %s over/under flow: '%s'\n", \
           CPRT_BASENAME(__FILE__), __LINE__, #r_, in_a_); \
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
         CPRT_BASENAME(__FILE__), __LINE__, #r_, in_a_); \
    } else { /* strtol thinks success; check for overflow. */ \
      (r_) = llresult_; /* "return" value of macro */ \
      if ((r_) != llresult_) { \
        fprintf(stderr, "%s:%d, %s over/under flow: '%s'\n", \
           CPRT_BASENAME(__FILE__), __LINE__, #r_, in_a_); \
        new_errno_ = ERANGE; \
      } \
    } \
  } \
  errno = new_errno_; \
  CPRT_EOK0(errno); /* Omit this line if you want errors to return. */ \
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
      exit(1); /* Can't use CPRT_ERR_EXIT because it calls CPRT_NET_CLEANUP. */ \
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
  #define CPRT_SLEEP_NS cprt_sleep_ns  /* Uses busy spinning. */
  #define CPRT_STRTOK strtok_s

#else  /* Unix */
  #define CPRT_SNPRINTF snprintf
  #define CPRT_STRDUP strdup
  #define CPRT_SLEEP_SEC sleep
  #define CPRT_SLEEP_MS(ms_) usleep((ms_)*1000)
  #define CPRT_SLEEP_NS cprt_sleep_ns  /* Uses busy spinning. */
  #define CPRT_STRTOK strtok_r
#endif


#if defined(_WIN32)
  #define CPRT_COND_T CONDITION_VARIABLE
  #define CPRT_COND_INIT(_c) InitializeConditionVariable(&(_c))
  #define CPRT_COND_WAIT(_c, _m) SleepConditionVariableCS(&(_c), &(_m), INFINITE)
  #define CPRT_COND_SIGNAL(_c) WakeConditionVariable(&(_c))
  #define CPRT_COND_BROADCAST(_c) WakeAllConditionVariable(&(_c))
  #define CPRT_COND_DELETE(_c) do {;} while (0)

  #define CPRT_MUTEX_T CRITICAL_SECTION
  #define CPRT_MUTEX_INIT(_m) InitializeCriticalSection(&(_m))
  #define CPRT_MUTEX_INIT_RECURSIVE(_m) InitializeCriticalSection(&(_m))
  #define CPRT_MUTEX_LOCK(_m) EnterCriticalSection(&(_m))
  #define CPRT_MUTEX_TRYLOCK(_got_it, _m) (_got_it) = TryEnterCriticalSection(&(_m))
  #define CPRT_MUTEX_UNLOCK(_m) LeaveCriticalSection(&(_m))
  #define CPRT_MUTEX_DELETE(_m) DeleteCriticalSection(&(_m))

  #define CPRT_SPIN_T CRITICAL_SECTION
  #define CPRT_SPIN_INIT(_m) InitializeCriticalSectionAndSpinCount((&_m), -1)
  #define CPRT_SPIN_LOCK(_m) EnterCriticalSection(&(_m))
  #define CPRT_SPIN_TRYLOCK(_got_it, _m) (_got_it) = TryEnterCriticalSection(&(_m))
  #define CPRT_SPIN_UNLOCK(_m) LeaveCriticalSection(&(_m))
  #define CPRT_SPIN_DELETE(_m) DeleteCriticalSection(&(_m))

#else  /* Unixes. */
  #define CPRT_COND_T pthread_cond_t
  #define CPRT_COND_INIT(_c) pthread_cond_init(&(_c), NULL)
  #define CPRT_COND_WAIT(_c, _m) pthread_cond_wait(&(_c), &(_m))
  #define CPRT_COND_SIGNAL(_c) pthread_cond_signal(&(_c))
  #define CPRT_COND_BROADCAST(_c) pthread_cond_broadcast(&(_c))
  #define CPRT_COND_DELETE(_c) pthread_cond_destroy(&(_c))

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
      CPRT_ERR_EXIT; \
    } \
  } while (0)
  #define CPRT_MUTEX_UNLOCK(_m) pthread_mutex_unlock(&(_m))
  #define CPRT_MUTEX_DELETE(_m) pthread_mutex_destroy(&(_m))

  #if defined(__APPLE__)
    /* Apparently spinlocks are a no-no on newer MacOS. */
    #define CPRT_SPIN_T pthread_mutex_t
    #define CPRT_SPIN_INIT(_m) CPRT_EOK0(errno = pthread_mutex_init(&(_m), NULL))
    #define CPRT_SPIN_LOCK(_m) CPRT_EOK0(errno = pthread_mutex_lock(&(_m)))
    #define CPRT_SPIN_TRYLOCK(_got_it, _m) do { \
      errno = pthread_mutex_trylock(&(_m)); \
      if (errno == 0) { \
        _got_it = 1; \
      } else if (errno == EBUSY) { \
        _got_it = 0; \
      } else { \
        CPRT_PERRNO("pthread_mutex_trylock"); \
        CPRT_ERR_EXIT; \
      } \
    } while (0)
    #define CPRT_SPIN_UNLOCK(_m) pthread_mutex_unlock(&(_m))
    #define CPRT_SPIN_DELETE(_m) pthread_mutex_destroy(&(_m))
  #else  /* Non-Apple Unixes */
    #define CPRT_SPIN_T pthread_spinlock_t
    #define CPRT_SPIN_INIT(_m) pthread_spin_init(&(_m), PTHREAD_PROCESS_PRIVATE)
    #define CPRT_SPIN_LOCK(_m) pthread_spin_lock(&(_m))
    #define CPRT_SPIN_TRYLOCK(_got_it, _m) do { \
      errno = pthread_spin_trylock(&(_m)); \
      if (errno == 0) { \
        _got_it = 1; \
      } else if (errno == EBUSY) { \
        _got_it = 0; \
      } else { \
        CPRT_PERRNO("pthread_mutex_trylock"); \
        CPRT_ERR_EXIT; \
      } \
    } while (0)
    #define CPRT_SPIN_UNLOCK(_m) pthread_spin_unlock(&(_m))
    #define CPRT_SPIN_DELETE(_m) pthread_spin_destroy(&(_m))
  #endif
#endif


#if defined(_WIN32)
  #define CPRT_SEM_T HANDLE
  #define CPRT_SEM_INIT(_s, _i) do { \
    (_s) = CreateSemaphore(NULL, _i, INT_MAX, NULL); \
    if ((_s) == NULL) { \
      errno = GetLastError();\
      CPRT_PERRNO("CreateThread"); \
      CPRT_ERR_EXIT; \
    } \
  } while (0)
  #define CPRT_SEM_DELETE(_s) do { \
    BOOL rc_ = CloseHandle(_s); \
    if (rc_ == 0) { \
      errno = GetLastError();\
      CPRT_PERRNO("CreateThread"); \
      CPRT_ERR_EXIT; \
    } \
  } while (0)
  #define CPRT_SEM_POST(_s) do { \
    BOOL rc_ = ReleaseSemaphore(_s, 1, NULL); \
    if (rc_ == 0) { \
      errno = GetLastError();\
      CPRT_PERRNO("CreateThread"); \
      CPRT_ERR_EXIT; \
    } \
  } while (0)
  #define CPRT_SEM_WAIT(_s) do { \
    WaitForSingleObject(_s, INFINITE); \
  } while (0)

#elif defined(__APPLE__)
  #define CPRT_SEM_T dispatch_semaphore_t
  #define CPRT_SEM_INIT(_s, _i) _s = dispatch_semaphore_create(_i)
  #define CPRT_SEM_DELETE(_s) dispatch_release(_s)
  #define CPRT_SEM_POST(_s) dispatch_semaphore_signal(_s)
  #define CPRT_SEM_WAIT(_s) dispatch_semaphore_wait(_s, DISPATCH_TIME_FOREVER)

#else  /* Non-Apple Unixes */
  #define CPRT_SEM_T sem_t
  #define CPRT_SEM_INIT(_s, _i) CPRT_EOK0(sem_init(&(_s), 0, _i))
  #define CPRT_SEM_DELETE(_s) CPRT_EOK0(sem_destroy(&(_s)))
  #define CPRT_SEM_POST(_s) CPRT_EOK0(sem_post(&(_s)))
  #define CPRT_SEM_WAIT(_s) CPRT_EOK0(sem_wait(&(_s)))
#endif


#if defined(_WIN32)
  #define CPRT_THREAD_T HANDLE
  #define CPRT_THREAD_ID_T uint64_t
  #define CPRT_THREAD_ENTRYPOINT DWORD WINAPI
  #define CPRT_THREAD_CREATE(_tid, _tstrt, _targ) do {\
    DWORD _ignore;\
    _tid = CreateThread(NULL, 0, _tstrt, _targ, 0, &_ignore);\
    if (_tid == NULL) {\
      errno = GetLastError();\
      CPRT_PERRNO("CreateThread"); \
      CPRT_ERR_EXIT; \
    }\
  } while (0)
  #define CPRT_THREAD_EXIT do { ExitThread(0); } while (0)
  #define CPRT_THREAD_JOIN(_tid) WaitForSingleObject(_tid, INFINITE)
  #define CPRT_GET_THREAD_ID() ((CPRT_THREAD_ID_T)GetCurrentThreadId())

#else  /* Unix */
  #define CPRT_THREAD_T pthread_t
  #define CPRT_THREAD_ID_T uint64_t
  #define CPRT_THREAD_ENTRYPOINT void *
  #define CPRT_THREAD_CREATE(_tid, _tstrt, _targ) \
    CPRT_EOK0(errno = pthread_create(&(_tid), NULL, _tstrt, _targ))
  #define CPRT_THREAD_EXIT pthread_exit(NULL)
  #define CPRT_THREAD_JOIN(_tid) \
    CPRT_EOK0(errno = pthread_join(_tid, NULL))
  #define CPRT_GET_THREAD_ID() ((CPRT_THREAD_ID_T)pthread_self())
#endif

#define CPRT_CPU_ZERO(_cprt_cpuset) do { \
  uint64_t *_cprt_cpuset_p = (_cprt_cpuset); \
  *_cprt_cpuset_p = 0; \
} while (0)

#define CPRT_CPU_SET(_cprt_cpunum, _cprt_cpuset) do { \
  uint64_t *_cprt_cpuset_p = (_cprt_cpuset); \
  if ((_cprt_cpunum) == 0) { \
    *_cprt_cpuset_p |= 1; \
  } else { \
    *_cprt_cpuset_p |= (1ull << (_cprt_cpunum)); \
  } \
} while (0)


#define CPRT_INITTIME cprt_inittime
#if defined(_WIN32)
  struct cprt_timeval {
    time_t tv_sec;
    long tv_usec;
  };
  struct cprt_timespec {
    time_t tv_sec;
    long tv_nsec;
  };
  #define CPRT_GETTIME(_ts) cprt_gettime(_ts)
  void cprt_gettime(struct cprt_timespec *ts);
  void cprt_sleep_ns(uint64_t duration_ns);
  
#elif defined(__APPLE__)
  #define CPRT_GETTIME(_ts) clock_gettime(CLOCK_MONOTONIC_RAW, _ts)
  #define cprt_timeval timeval
  #define cprt_timespec timespec
#else  /* Non-Apple Unixes */
  #define CPRT_GETTIME(_ts) clock_gettime(CLOCK_MONOTONIC, _ts)
  #define cprt_timeval timeval
  #define cprt_timespec timespec
#endif

/* Get nsec diff between two struct timespec (used by clock_gettime). */
#define CPRT_DIFF_TS(diff_ts_result_ns_, diff_ts_end_ns_, diff_ts_start_ns_) do { \
  (diff_ts_result_ns_) = (((uint64_t)diff_ts_end_ns_.tv_sec \
                           - (uint64_t)diff_ts_start_ns_.tv_sec) * 1000000000ull \
                          + (uint64_t)diff_ts_end_ns_.tv_nsec) \
                         - (uint64_t)diff_ts_start_ns_.tv_nsec; \
} while (0)  /* CPRT_DIFF_TS */

/* externals in cprt.c. */
char *cprt_strerror(int errnum, char *buffer, size_t buf_sz);
void cprt_set_affinity(uint64_t in_mask);
int cprt_try_affinity(uint64_t in_mask);
void cprt_inittime();
void cprt_sleep_ns(uint64_t duration_ns);
void cprt_localtime_r(time_t *timep, struct tm *result);

#if defined(_WIN32)
  int cprt_timeofday(struct cprt_timeval *tv, void *unused_tz);
  int cprt_win_gettime(struct cprt_timespec *tp);
#endif


extern int cprt_num_events;
extern int cprt_events[1024];
void cprt_event(int e);
void cprt_dump_events();
void cprt_perrno(char *msg_str, char *file, int line);
char *cprt_timestamp(char *str, int bufsz, int do_date, int precision);
void cprt_vts_fprintf(FILE *fp, const char *format, va_list argp);
void cprt_ts_printf(const char *format, ...);
void cprt_ts_eprintf(const char *format, ...);
uint64_t cprt_get_ms_time();
void cprt_vms_fprintf(FILE *fp, uint64_t start_ms, const char *format, va_list argp);
void cprt_ms_printf(uint64_t start_ms, const char *format, ...);
void cprt_ms_eprintf(uint64_t start_ms, const char *format, ...);


extern char* cprt_optarg;
extern int cprt_optopt;
extern int cprt_optind;
extern int cprt_opterr;
int cprt_getopt(int argc, char* const argv[], const char* optstring);

#if defined(__cplusplus)
}
#endif

#endif  /* CPRT_H */
