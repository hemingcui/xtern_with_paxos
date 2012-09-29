/*
 * Use LD_PRELOAD to intercept some important libc function calls for diagnosing x86 programs.
 */

#define _GNU_SOURCE
//#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <sys/socket.h>
#include <execinfo.h>

#define PROJECT_TAG "XTERN"
#define RESOLVE(x)	if (!fp_##x && !(fp_##x = dlsym(RTLD_NEXT, #x))) { fprintf(stderr, #x"() not found!\n"); exit(-1); }

//#define DEBUGF(a...)	if (debug) { fprintf(stderr, "%s: %s(): ", PROJECT_TAG, __FUNCTION__); fprintf(stderr, ##a); }

// Per thread variables.
static int num_threads = 0;
static int turn = 0;
static __thread int self_turn = -1;
static __thread int self_tid = -1;
static __thread struct timespec my_time;
static __thread int entered_sys = 0; // Avoid recursively enter backtrace(), since backtrace() will call pthread_mutex_lock().
static __thread FILE *log = NULL;
static pthread_mutex_t lock;

//  fprintf(stderr, "START: %s: %s(): tid %d\n", PROJECT_TAG, __FUNCTION__, self());

#define OPERATION_START initTid(); \
  updateTurn(); \
  struct timespec app_time; \
  update_time(&app_time);

//  fprintf(stderr, "END: %s: %s(): tid %d\n\n", PROJECT_TAG, __FUNCTION__, self());

#define OPERATION_END \
  struct timespec syscall_time; \
  update_time(&syscall_time); \
  if (!entered_sys) { \
    entered_sys = 1; \
    void *eip = get_eip(); \
    logOp(__FUNCTION__, eip, self_tid, self_turn, &app_time, &syscall_time); \
    entered_sys = 0; \
  } 
  //fprintf(stderr, "function %s ends, pthread self %d\n", __FUNCTION__, self());

void update_time(struct timespec *ret);
int internal_mutex_lock(pthread_mutex_t *mutex);
int internal_mutex_unlock(pthread_mutex_t *mutex);

int self() {
  assert(self_tid >= 0);
  return self_tid;
}

void initTid() {
  if (self_tid == -1) {
    internal_mutex_lock(&lock);
    self_tid = num_threads;
    num_threads++;
    if (num_threads == 1) // To match the idle thread in xtern.
      num_threads++;
    internal_mutex_unlock(&lock);
    struct timespec tmp;
    update_time(&tmp); // Init my_time.
  }
}

void updateTurn() {
  internal_mutex_lock(&lock);
  self_turn = turn;
  turn++;
  internal_mutex_unlock(&lock);
}

void time_diff(const struct timespec *start, const struct timespec *end, struct timespec *diff)
{
  if ((end->tv_nsec-start->tv_nsec) < 0) {
    diff->tv_sec = end->tv_sec - start->tv_sec - 1;
    diff->tv_nsec = 1000000000 - start->tv_nsec + end->tv_nsec;
  } else {
    diff->tv_sec = end->tv_sec - start->tv_sec;
    diff->tv_nsec = end->tv_nsec - start->tv_nsec;
  }
}

void update_time(struct timespec *ret)
{
  struct timespec start_time;
  clock_gettime(CLOCK_REALTIME , &start_time);
  time_diff(&my_time, &start_time, ret);
  my_time.tv_sec = start_time.tv_sec;
  my_time.tv_nsec = start_time.tv_nsec;
}

void *get_eip()
{
  void *tracePtrs[20];
  int ret = backtrace(tracePtrs, 20);
  assert(ret >= 0);
  return tracePtrs[2];  //  this is ret_eip of my caller
}

void logOp(const char *op, void *eip, int tid, int self_turn, struct timespec *app_time, struct timespec *syscall_time) {
  if (!log) {
    mkdir("./out", 0777);
    char buf[1024] = {0};
    sprintf(buf, "./out/tid-%d-%d.txt", getpid(), self());
    log = fopen(buf, "w");
    assert(log);
  }
  fprintf(log, "%s %p %d %f %f 0.0 %d n/a\n", op, eip, self_turn,
    (double)app_time->tv_sec + ((double)app_time->tv_nsec)/1000000000.0,
     (double)syscall_time->tv_sec + ((double)syscall_time->tv_nsec)/1000000000.0,
    tid);
  fflush(log);
}

static int (*fp_pthread_mutex_lock)(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex) {
  OPERATION_START;
  RESOLVE(pthread_mutex_lock);
  int ret = fp_pthread_mutex_lock(mutex);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

int internal_mutex_lock(pthread_mutex_t *mutex) {
  RESOLVE(pthread_mutex_lock);
  int ret = fp_pthread_mutex_lock(mutex);
  return ret;
}

static int (*fp_pthread_mutex_unlock)(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  OPERATION_START;
  RESOLVE(pthread_mutex_unlock);
  int ret = fp_pthread_mutex_unlock(mutex);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

int internal_mutex_unlock(pthread_mutex_t *mutex) {
  RESOLVE(pthread_mutex_unlock);
  int ret = fp_pthread_mutex_unlock(mutex);
  return ret;
}

static int (*fp_pthread_mutex_trylock)(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex) {
  OPERATION_START;
  RESOLVE(pthread_mutex_trylock);
  int ret = fp_pthread_mutex_trylock(mutex);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

static int (*fp_pthread_mutex_init)(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  OPERATION_START;
  RESOLVE(pthread_mutex_init);
  int ret = fp_pthread_mutex_init(mutex, attr);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

static int (*fp_pthread_mutex_destroy)(pthread_mutex_t *mutex);
int pthread_mutex_destroy(pthread_mutex_t *mutex) {
  OPERATION_START;
  RESOLVE(pthread_mutex_destroy);
  int ret = fp_pthread_mutex_destroy(mutex);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;  
}

static int (*fp_pthread_create)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
  OPERATION_START;
  RESOLVE(pthread_create);
  int ret = fp_pthread_create(thread, attr, start_routine, arg);
  int cur_errno = errno;
  OPERATION_END;
  //usleep(1); // Sleep for a while in order to make the child thread run first, this is to make thread id deterministic. Not a guarantee (and not need to).
  errno = cur_errno;
  return ret;
}

static int (*fp_pthread_join)(pthread_t thread, void **retval);
int pthread_join(pthread_t thread, void **retval) {
  OPERATION_START;
  RESOLVE(pthread_join);
  int ret = fp_pthread_join(thread, retval);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

static void (*fp_pthread_exit)(void *retval);
void pthread_exit(void *retval) {
  OPERATION_START;
  RESOLVE(pthread_exit);
  OPERATION_END;
  //int ret = 
  fp_pthread_exit(retval);
  //int cur_errno = errno;
  //errno = cur_errno;
  //return ret;
}

//int pthread_cancel(pthread_t thread);

static int (*fp_pthread_cond_init)(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
  OPERATION_START;
  RESOLVE(pthread_cond_init);
  int ret = fp_pthread_cond_init(cond, attr);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

static int (*fp_pthread_cond_wait)(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
  OPERATION_START;
  RESOLVE(pthread_cond_wait);
  int ret = fp_pthread_cond_wait(cond, mutex);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

static int (*fp_pthread_cond_destroy)(pthread_cond_t *cond);
int pthread_cond_destroy(pthread_cond_t *cond) {
  OPERATION_START;
  RESOLVE(pthread_cond_destroy);
  int ret = fp_pthread_cond_destroy(cond);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;  
}

static int (*fp_pthread_cond_timedwait)(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
  OPERATION_START;
  RESOLVE(pthread_cond_timedwait);
  int ret = fp_pthread_cond_timedwait(cond, mutex, abstime);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

static int (*fp_pthread_cond_signal)(pthread_cond_t *cond);
int pthread_cond_signal(pthread_cond_t *cond) {
  OPERATION_START;
  RESOLVE(pthread_cond_signal);
  int ret = fp_pthread_cond_signal(cond);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

static int (*fp_pthread_cond_broadcast)(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond) {
  OPERATION_START;
  RESOLVE(pthread_cond_broadcast);
  int ret = fp_pthread_cond_broadcast(cond);
  int cur_errno = errno;
  OPERATION_END;
  errno = cur_errno;
  return ret;
}

//int socket(int domain, int type, int protocol);

//ssize_t send(int socket, const void *buffer, size_t length, int flags);

//ssize_t sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);

//ssize_t sendmsg(int socket, const struct msghdr *message, int flags);

//ssize_t recv(int socket, void *buffer, size_t length, int flags);

//ssize_t recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len);

//ssize_t recvmsg(int socket, struct msghdr *message, int flags);

//int select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds, fd_set *restrict errorfds, struct timeval *restrict timeout);

//ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset); 

//ssize_t read(int fildes, void *buf, size_t nbyte);

//ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset); 

//ssize_t write(int fildes, const void *buf, size_t nbyte);

//int connect(int socket, const struct sockaddr *address, socklen_t address_len);

//int accept(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len);

//int bind(int socket, const struct sockaddr *address, socklen_t address_len);

//int listen(int socket, int backlog);

//int shutdown(int socket, int how);
