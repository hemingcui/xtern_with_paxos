
#include <iostream>
#include <string>
#include <cstdlib>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <tr1/unordered_map>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/circular_buffer.hpp>

#include "tern/runtime/paxos-op-queue.h"


#define ELEM_CAPACITY 10000
#define DELTA 100 // TBD: don't know why we couldn't get 100% of the shared mem.
#define SEG_NAME "/PAXOS_OP_QUEUE"
#define CB_NAME "/CIRCULAR_BUFFER"
#define LOCK_FILE_NAME "/tmp/paxos_queue_file_lock"
//#define DEBUG_PAXOS_OP_QUEUE

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

#ifdef DEBUG_PAXOS_OP_QUEUE
#define PRINT std::cout
#else
#define PRINT if (false) std::cout
#endif

/** TBD: CURRENT WE ASSUME THE SERVER HAS ONLY ONE PROCESS TALKING TO THE PROXY.
IF THE SERVER HAVE MULTIPLE PROCESSES AND EACH OF THEM THEY CONNECT TO  
THE PROXY, THE PAXOS_OP NEEDS TO HAVE A PID FIELD. OR WE NEED TO SEPARATE THE 
PAXOS OP QUEUE TO A "PER SERVER PROCESS" BASED.**/


typedef std::tr1::unordered_map<uint64_t, int> conn_id_to_server_sock;
typedef std::tr1::unordered_map<int, uint64_t> server_sock_to_conn_id;
typedef std::tr1::unordered_map<unsigned, int> server_port_to_tid;
typedef std::tr1::unordered_map<int, unsigned> tid_to_server_port;
conn_id_to_server_sock conn_id_map;
server_sock_to_conn_id server_sock_map;
server_port_to_tid server_port_map;
tid_to_server_port tid_map;

void conns_init() {
  conn_id_map.clear();
  server_sock_map.clear();
  server_port_map.clear();
  tid_map.clear();
}

uint64_t conns_get_conn_id(int server_sock) {
  server_sock_to_conn_id::iterator it = server_sock_map.find(server_sock);
  assert(it != server_sock_map.end());
  return it->second;
}

int conns_exist_by_conn_id(uint64_t conn_id) {
  return (conn_id_map.find(conn_id) != conn_id_map.end());
}

int conns_exist_by_server_sock(int server_sock) {
  return (server_sock_map.find(server_sock) != server_sock_map.end());
}
    
int conns_get_server_sock(uint64_t conn_id) {
  conn_id_to_server_sock::iterator it = conn_id_map.find(conn_id);
  assert(it != conn_id_map.end());
  return it->second;
}

/** When the proxy sends a PAXQ_CLOSE operation, the server thread uses this function
to erase the connection_id. Do not need to erase the server_sock_map because the server
thread should do this job when it calls a close() operation. (because a socket is closed at both sides). **/
void conns_erase_by_conn_id(uint64_t conn_id) {
  conn_id_to_server_sock::iterator it = conn_id_map.find(conn_id);
  assert(it != conn_id_map.end());
  int server_sock = it->second;
  conn_id_map.erase(it);

  //server_sock_to_conn_id::iterator it2 = server_sock_map.find(server_sock);
  //assert(it2 != server_sock_map.end());
  //server_sock_map.erase(it2);
}

/** When the server calls a close() operation, the server thread uses this function
to erase the server_sock. Do not need to erase the conn_id_map because the server
thread should do this job when it receives a PAXQ_CLOSE from the proxy side
(because a socket is closed at both sides). **/
void conns_erase_by_server_sock(int server_sock) {
  server_sock_to_conn_id::iterator it2 = server_sock_map.find(server_sock);
  assert(it2 != server_sock_map.end());
  int conn_id = it2->second;
  server_sock_map.erase(it2);

  //conn_id_to_server_sock::iterator it = conn_id_map.find(conn_id);
  //assert(it != conn_id_map.end());
  //conn_id_map.erase(it);
}
    
void conns_add_pair(uint64_t conn_id, int server_sock) {
  assert(conn_id_map.find(conn_id) == conn_id_map.end());
  conn_id_map[conn_id] = server_sock;
  assert(server_sock_map.find(server_sock) == server_sock_map.end());
  server_sock_map[server_sock] = conn_id;
  std::cout << "conns_add_pair added pair: (connection_id "
    << conn_id << ", server_sock " << server_sock 
    << "), now conns size " << conns_get_conn_id_num() << std::endl;
}

/** Because a socket is closed at both sides (proxy and server),
sometimes the conn_id_map and server_sock_map can have different
sizes. Because this function is called by the TimeAlgo (to check the paxos
operations from the proxy queue), so I choose to return the conn_id_map.
**/
size_t conns_get_conn_id_num() {
  return conn_id_map.size();
}

void conns_add_tid_port_pair(int tid, unsigned port) {
  fprintf(stderr, "conns_add_tid_port_map insert pair (%d, %u)\n", tid, port);
  /** Heming: these two asserts are to guarantee an assumption that a server application only
  listens on one port, not multiple ports. Thus, a bind() operation of is called
  only once within a process. This assumption is reasonable in modern servers.
  **/
  assert(server_port_map.find(port) == server_port_map.end());
  assert(tid_map.find(tid) == tid_map.end());
  server_port_map[port] = tid;
  tid_map[tid] = port;
}

int conns_get_tid_from_port(unsigned port) {
  //FIXME: THIS ASSERTION MAY FAIL IF BIND() HAPPENS AFTER A CLIENTS CONNECT.
  fprintf(stderr, "conns_get_tid_from_port query from port %u\n", port);
  assert(server_port_map.find(port) != server_port_map.end());
  return server_port_map[port];
}

unsigned conns_get_port_from_tid(int tid) {
  //FIXME: THIS ASSERTION MAY FAIL IF BIND() HAPPENS AFTER A CLIENTS CONNECT.
  fprintf(stderr, "conns_get_port_from_tid query from tid %d\n", tid);
  assert(tid_map.find(tid) != tid_map.end());
  return tid_map[tid];
}

void conns_print() {
  if (conns_get_conn_id_num() > 0) {
    struct timeval tnow;
    gettimeofday(&tnow, NULL);
    std::cout << "conns_print connection pair: now time (" << tnow.tv_sec << "." << tnow.tv_usec
      << "), size " << conns_get_conn_id_num() << std::endl;
    conn_id_to_server_sock::iterator it = conn_id_map.begin();
    int i = 0;
    for (; it != conn_id_map.end(); it++, i++) {
      /*This assertion no longer holds because conn_id_map and server_sock_map
      can have different sizes (see conns_erase_*() functions). */
      //assert(server_sock_map.find(it->second) != server_sock_map.end());
      std::cout << "conns_print connection pair[" << i << "]: connection_id " << it->first << ", server sock " << it->second << std::endl;
    }
    std::cout << endl << endl;
  }

  if (tid_map.size() > 0) {
    std::cout << "conns_print tid <-> port pair size: " << tid_map.size() << std::endl;
    tid_to_server_port::iterator it = tid_map.begin();
    int i = 0;
    for (; it != tid_map.end(); it++, i++) {
      assert(server_port_map.find(it->second) != server_port_map.end());
      std::cout << "conns_print tid <-> port pair[" << i << "]: tid " << it->first << ", port " << it->second << std::endl;
    }
    std::cout << endl << endl;
  }
}

namespace bip = boost::interprocess;
typedef bip::allocator<paxos_op, bip::managed_shared_memory::segment_manager> ShmemAllocatorCB;
typedef boost::circular_buffer<paxos_op, ShmemAllocatorCB> MyCircularBuffer;
bip::managed_shared_memory *segment = NULL;
MyCircularBuffer *circbuff = NULL;
int lockFileFd = 0;
const char *paxq_op_str[5] = {"PAXQ_INVALID", "PAXQ_CONNECT", "PAXQ_SEND", "PAXQ_CLOSE", "PAXQ_NOP"};

/** This function is called by the DMT runtime, because the eval.py starts it before the proxy. **/
void paxq_create_shared_mem() {
#ifdef DEBUG_PAXOS_OP_QUEUE
  std::cout << "Init shared memory " << (bip::shared_memory_object::remove(SEG_NAME) ?
    "cleaned up: " : "not exist: " ) << SEG_NAME << "\n";
#else
  bip::shared_memory_object::remove(SEG_NAME);
#endif
  // TBD: must pass the PROXY_NODE_ID env var.
  // TBD.

  segment = new bip::managed_shared_memory(bip::create_only, SEG_NAME, (ELEM_CAPACITY+DELTA)*sizeof(paxos_op));
  static const ShmemAllocatorCB alloc_inst (segment->get_segment_manager());
  circbuff = segment->construct<MyCircularBuffer>(CB_NAME)(ELEM_CAPACITY, alloc_inst);
  circbuff->clear();
  assert(circbuff->size() == 0);
  paxq_print();

  lockFileFd = open(LOCK_FILE_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (lockFileFd == -1) {
    std::cout << "paxq_create_shared_memory file lock " << LOCK_FILE_NAME " open failed, errno " << errno << ".\n";
    exit(1);
  }
}

/** This function is called by the proxy, because the eval.py starts the proxy after the server. . **/
void paxq_open_shared_mem(int node_id) {
  segment = new bip::managed_shared_memory(bip::open_only, SEG_NAME);
  circbuff = segment->find<MyCircularBuffer>(CB_NAME).first;
  lockFileFd = open(LOCK_FILE_NAME, O_RDWR, S_IRUSR | S_IWUSR);
  if (lockFileFd == -1) {
    std::cout << "paxq_open_shared_memory file lock " << LOCK_FILE_NAME " open failed, errno " << errno << ".\n";
    exit(1);
  }
}

void paxq_insert_front(int with_lock, uint64_t conn_id, uint64_t counter, PAXOS_OP_TYPE t, unsigned port) {
  struct timeval tnow;
  gettimeofday(&tnow, NULL);
  if (with_lock) paxq_lock();
  if (paxq_size() == ELEM_CAPACITY) {
    std::cout << SEG_NAME << " is too small for this app. Please enlarge it in paxos-op-queue.h\n"; 
    if (with_lock) paxq_unlock();
    exit(1);
  }
  paxos_op op = {conn_id, counter, t, port}; // TBD: is this OK for IPC shared-memory?
  circbuff->insert(circbuff->begin(), op);      
  std::cout << "paxq_insert_front time <" << tnow.tv_sec << "." << tnow.tv_usec
    << ">, now size " << paxq_size() << "\n";
  paxq_print();
  if (with_lock) paxq_unlock();
}

void paxq_push_back(int with_lock, uint64_t conn_id, uint64_t counter, PAXOS_OP_TYPE t, unsigned port) {
  struct timeval tnow;
  gettimeofday(&tnow, NULL);
  if (with_lock) paxq_lock();
  if (paxq_size() == ELEM_CAPACITY) {
    std::cout << SEG_NAME << " is too small for this app. Please enlarge it in paxos-op-queue.h\n"; 
    if (with_lock) paxq_unlock();
    exit(1);
  }
  paxos_op op = {conn_id, counter, t, port}; // TBD: is this OK for IPC shared-memory?
  circbuff->push_back(op);      
  std::cout << "paxq_push_back time <" << tnow.tv_sec << "." << tnow.tv_usec
    << ">, now size " << paxq_size() << "\n";
  paxq_print();
  if (with_lock) paxq_unlock();
}

paxos_op paxq_front() {
  return circbuff->front();
}

unsigned paxq_dec_front_value() {
  assert(paxq_size() > 0);
  paxos_op &op = circbuff->front();
  assert(op.type == PAXQ_NOP);
  assert(op.value != 0);
  op.value--;
  return op.value;
}


paxos_op paxq_pop_front(int debugTag) {
  struct timeval tnow;
  gettimeofday(&tnow, NULL);
  paxos_op op = paxq_front();
  circbuff->pop_front();
  std::cout << "DEBUG TAG " << debugTag
    << ": paxq_pop_front time <" << tnow.tv_sec << "." << tnow.tv_usec 
    << ">: (" << op.connection_id
    << ", " << op.counter << ", " << paxq_op_str[op.type]
    << ", " << op.value << ")" << std::endl;
  return op;
} 

size_t paxq_size() {
  size_t t = circbuff->size();
  return t;       
}

void paxq_lock() {
  lockf(lockFileFd, F_LOCK, 0);
}

void paxq_unlock() {
  lockf(lockFileFd, F_ULOCK, 0);
}

void paxq_test() {
#ifdef DEBUG_PAXOS_OP_QUEUE
  std::cout << "Circular Buffer Size before push_back: " << circbuff->size() << "\n";
  for (int i = 0; i < ELEM_CAPACITY*2+123; i++) {
    paxos_op op = {i, i, PAXQ_SEND};
    circbuff->push_back(op);
    //push_back(i, i, SEND); This code will trigger the circular buffer 
    // full exit, which is good.
  }
  std::cout << "Circular Buffer Size after push_back: " << circbuff->size() << "\n";

  for (int i = 0; i < 10; i++) {
    std::cout << "Child got: " << (*circbuff)[i].connection_id
      << ", " << (*circbuff)[i].counter << ", " << (*circbuff)[i].type << "\n";
  }

  paxos_op op0 = paxq_front();
  std::cout << "\n\nHead op: " << op0.connection_id << ", " << op0.counter << "\n";

  paxos_op op1 = paxq_pop_front();
  std::cout << "\n\nPopped op: " << op1.connection_id << ", " << op1.counter << "\n";
  paxos_op op2 = paxq_pop_front();
  std::cout << "\n\nPopped op: " << op2.connection_id << ", " << op2.counter << "\n";
  paxos_op op3 = paxq_pop_front();
  std::cout << "\n\nPopped op: " << op3.connection_id << ", " << op3.counter << "\n";

  std::cout << "\n\nCircular Buffer Size after pop: " << circbuff->size() << "\n";
  for (int i = 0; i < 10; i++) {
    std::cout << "Child got: " << (*circbuff)[i].connection_id
      << ", " << (*circbuff)[i].counter << "\n";
  }  
#endif
}

void paxq_print() {
  if (paxq_size() == 0)
    return;
  //boost::circular_buffer::iterator itr = circbuff->begin();
  //std::cout << "paxq_print circbuff now " << circbuff << ", pself " << pthread_self() << std::endl;
  struct timeval tnow;
  gettimeofday(&tnow, NULL);
  std::cout << "paxq_print size now time (" << tnow.tv_sec << "." << tnow.tv_usec << "), size " << paxq_size() << std::endl;
  for (size_t i = 0; i < paxq_size(); i++) {
    paxos_op &op = (*circbuff)[i];
    std::cout << "paxq_print [" << i << "]: (" << op.connection_id
    << ", " << op.counter << ", " << paxq_op_str[op.type] << ", " << op.value << ")\n";
  }
}

#ifdef __cplusplus
}
#endif

