
#include <iostream>
#include <string>
#include <cstdlib>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <tr1/unordered_map>
#include <stdlib.h>
#include <semaphore.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/circular_buffer.hpp>

#include "tern/runtime/paxos-op-queue.h"


#define ELEM_CAPACITY 10000
#define DELTA 100 // TBD: don't know why we couldn't get 100% of the shared mem.
#define SEG_NAME "PAXOS_OP_QUEUE"
#define CB_NAME "CIRCULAR_BUFFER"
#define SEM_NAME "/PAXOS_SEM"
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
conn_id_to_server_sock conn_id_map;
server_sock_to_conn_id server_sock_map;
size_t num_conn = 0;

void conns_init() {
  num_conn = 0;
  conn_id_map.clear();
  server_sock_map.clear();
}

uint64_t conns_get_conn_id(int server_sock) {
  server_sock_to_conn_id::iterator it = server_sock_map.find(server_sock);
  assert(it != server_sock_map.end());
  return it->second;
}
    
int conns_get_server_sock(uint64_t conn_id) {
  conn_id_to_server_sock::iterator it = conn_id_map.find(conn_id);
  assert(it != conn_id_map.end());
  return it->second;
}

void conns_erase_by_conn_id(uint64_t conn_id) {
  conn_id_to_server_sock::iterator it = conn_id_map.find(conn_id);
  assert(it != conn_id_map.end());
  int server_sock = it->second;
  conn_id_map.erase(it);

  server_sock_to_conn_id::iterator it2 = server_sock_map.find(server_sock);
  assert(it2 != server_sock_map.end());
  server_sock_map.erase(it2);

  num_conn--;
}
    
void conns_add_pair(uint64_t conn_id, int server_sock) {
  assert(conn_id_map.find(conn_id) == conn_id_map.end());
  conn_id_map[conn_id] = server_sock;
  assert(server_sock_map.find(server_sock) == server_sock_map.end());
  server_sock_map[server_sock] = conn_id;
  num_conn++;
}

size_t conns_get_num_conn() {
  assert(conn_id_map.size() == num_conn);
  assert(server_sock_map.size() == num_conn);
  return num_conn;
}


namespace bip = boost::interprocess;
typedef bip::allocator<paxos_op, bip::managed_shared_memory::segment_manager> ShmemAllocatorCB;
typedef boost::circular_buffer<paxos_op, ShmemAllocatorCB> MyCircularBuffer;
bip::managed_shared_memory *segment = NULL;
MyCircularBuffer *circbuff = NULL;
sem_t *sem = NULL;
    
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

  // 1 means this is a binary semaphore, or a mutex.
  sem = sem_open(SEM_NAME, O_CREAT, 0600, 1); 
  if (sem == SEM_FAILED) {
    std::cout << "paxq_create_shared_memory semaphore " << SEM_NAME " open failed, errno " << errno << ".\n";
     exit(1);
  }
}

/** This function is called by the proxy, because the eval.py starts the proxy after the server. . **/
void paxq_open_shared_mem(int node_id) {
  segment = new bip::managed_shared_memory(bip::open_only, SEG_NAME);
  circbuff = segment->find<MyCircularBuffer>(CB_NAME).first;
  // 1 means this is a binary semaphore, or a mutex.
  sem = sem_open(SEM_NAME, O_CREAT, 0600, 1); 
  if (sem == SEM_FAILED) {
    std::cout << "paxq_open_shared_mem semaphore " << SEM_NAME " open failed, errno " << errno << ".\n";
     exit(1);
  }
}

void paxq_push_back_w_lock(uint64_t conn_id, uint64_t counter, PAXOS_OP_TYPE t) {
  //paxq_lock();
  if (paxq_size() == ELEM_CAPACITY) {
    std::cout << SEG_NAME << " is too small for this app. Please enlarge it in paxos-op-queue.h\n"; 
    //paxq_unlock();
    exit(1);
  }
  //lock();
  paxos_op op = {conn_id, counter, t}; // TBD: is this OK for IPC shared-memory?
  circbuff->push_back(op);      
  //paxq_unlock();
  std::cout << "paxq_push_back, now size " << paxq_size() << "\n";
  paxq_print();
  //unlock();
}

paxos_op paxq_front() {
  //lock();
  return circbuff->front();
  //unlock();
  //return op;    
}

paxos_op paxq_pop_front() {
  //lock();
  paxos_op op = paxq_front();
  circbuff->pop_front();
  //unlock();
  return op;
}

size_t paxq_size() {
  //lock();
  size_t t = circbuff->size();
  //unlock();
  return t;       
}

void paxq_lock() {
  sem_wait(sem);  
}

void paxq_unlock() {
  sem_post(sem);  
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
  const char *op_type[3] = {"CONNECT", "SEND", "CLOSE"};
  std::cout << "paxq_print size " << paxq_size() << std::endl;
  for (size_t i = 0; i < paxq_size(); i++) {
    paxos_op &op = (*circbuff)[i];
    std::cout << "paxq_print [" << i << "]: (" << op.connection_id
    << ", " << op.counter << ", " << op_type[op.type] << ")\n";
  }
}

#ifdef __cplusplus
}
#endif

