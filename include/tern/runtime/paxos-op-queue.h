/* Copyright (c) 2013,  Regents of the Columbia University 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other 
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __TERN_COMMON_PAXOS_OP_QUEUE_H
#define __TERN_COMMON_PAXOS_OP_QUEUE_H

//#ifdef __cplusplus
//extern "C" {
//#endif

#include <iostream>
#include <string>
#include <cstdlib>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <tr1/unordered_map>
#include <stdlib.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/circular_buffer.hpp>

#define ELEM_CAPACITY 10000
#define DELTA 100 // TBD: don't know why we couldn't get 100% of the shared mem.
#define SEG_NAME "PAXOS_OP_QUEUE"
#define CB_NAME "CIRCULAR_BUFFER"
#define SEM_NAME "/PAXOS_SEM"
#define DEBUG_PAXOS_OP_QUEUE


/** TBD: CURRENT WE ASSUME THE SERVER HAS ONLY ONE PROCESS TALKING TO THE PROXY.
IF THE SERVER HAVE MULTIPLE PROCESSES AND EACH OF THEM THEY CONNECT TO  
THE PROXY, THE PAXOS_OP NEEDS TO HAVE A PID FIELD. OR WE NEED TO SEPARATE THE 
PAXOS OP QUEUE TO A "PER SERVER PROCESS" BASED.**/
namespace tern {
  enum PAXOS_OP_TYPE {
    CONNECT,
    SEND,
    CLOSE
  };
  
  struct paxos_op {
    uint64_t connection_id;
    uint64_t counter;
    PAXOS_OP_TYPE type;

    paxos_op(uint64_t id, uint64_t cnt, PAXOS_OP_TYPE t) {
      connection_id = id;
      counter = cnt;
      type = t;
    }
  };  

  struct proxy_server_sock_pair {
    size_t num_conn;
    typedef std::tr1::unordered_map<uint64_t, int> conn_id_to_server_sock;
    typedef std::tr1::unordered_map<int, uint64_t> server_sock_to_conn_id;
    conn_id_to_server_sock conn_id_map;
    server_sock_to_conn_id server_sock_map;

    proxy_server_sock_pair() {
      num_conn = 0;
      conn_id_map.clear();
      server_sock_map.clear();
    }

    uint64_t get_conn_id(int server_sock) {
      server_sock_to_conn_id::iterator it = server_sock_map.find(server_sock);
      assert(it != server_sock_map.end());
      return it->second;
    }
    
    int get_server_sock(uint64_t conn_id) {
      conn_id_to_server_sock::iterator it = conn_id_map.find(conn_id);
      assert(it != conn_id_map.end());
      return it->second;
    }

    void erase_by_conn_id(uint64_t conn_id) {
      conn_id_to_server_sock::iterator it = conn_id_map.find(conn_id);
      assert(it != conn_id_map.end());
      int server_sock = it->second;
      conn_id_map.erase(it);

      server_sock_to_conn_id::iterator it2 = server_sock_map.find(server_sock);
      assert(it2 != server_sock_map.end());
      server_sock_map.erase(it2);

      num_conn--;
    }
    
    void add_pair(uint64_t conn_id, int server_sock) {
      assert(conn_id_map.find(conn_id) == conn_id_map.end());
      conn_id_map[conn_id] = server_sock;
      assert(server_sock_map.find(server_sock) == server_sock_map.end());
      server_sock_map[server_sock] = conn_id;
      num_conn++;
    }

    size_t get_num_conn() {
      assert(conn_id_map.size() == num_conn);
      assert(server_sock_map.size() == num_conn);
      return num_conn;
    }
  };

  namespace bip = boost::interprocess;
  typedef bip::allocator<paxos_op, bip::managed_shared_memory::segment_manager> ShmemAllocatorCB;
  typedef boost::circular_buffer<paxos_op, ShmemAllocatorCB> MyCircularBuffer;

  class paxos_op_queue {
  private:
    MyCircularBuffer *circbuff;
    sem_t *sem;
    
  public:
    paxos_op_queue() {
    }

    /** This function is called by the DMT runtime, because the eval.py starts it 
    before the proxy. **/
    void create_shared_mem() {
#ifdef DEBUG_PAXOS_OP_QUEUE
      std::cout << "Init shared memory " << (bip::shared_memory_object::remove(SEG_NAME) ?
        "cleaned up: " : "not exist: " ) << SEG_NAME << "\n";
#else
      bip::shared_memory_object::remove(SEG_NAME);
#endif
      // TBD: must pass the PROXY_NODE_ID env var.
      // TBD.

      bip::managed_shared_memory segment(bip::create_only, SEG_NAME, (ELEM_CAPACITY+DELTA)*sizeof(paxos_op));
      const ShmemAllocatorCB alloc_inst (segment.get_segment_manager());
      circbuff = segment.construct<MyCircularBuffer>(CB_NAME)(ELEM_CAPACITY, alloc_inst);
      test();

      // 1 means this is a binary semaphore, or a mutex.
      sem = sem_open(SEM_NAME, O_CREAT|O_EXCL, 0644, 1); 
      if (sem == SEM_FAILED) {
        std::cout << "Semaphore " << SEM_NAME " already exists, errno " << errno << ".\n";
        exit(1);
      }
      sem_unlink(SEM_NAME);
    }

    /** This function is called by the proxy, because the eval.py starts the 
    proxy after the server. . **/
    void open_shared_mem(int node_id) {
      bip::managed_shared_memory segment(bip::open_only, SEG_NAME);
      circbuff = segment.find<MyCircularBuffer>(CB_NAME).first;
      sem = sem_open(SEM_NAME, 1);
      if (sem == SEM_FAILED) {
        std::cout << "Semaphore " << SEM_NAME " does not exist, errno " << errno << ".\n";
        exit(1);
      }
    }

    virtual ~paxos_op_queue() {
#ifdef DEBUG_PAXOS_OP_QUEUE
      //std::cout << (bip::shared_memory_object::remove(SEG_NAME) ? "removed: " : "failed to remove: " )
        //<< SEG_NAME << "\n";
#endif
    }

    void push_back(uint64_t conn_id, uint64_t counter, PAXOS_OP_TYPE t) {
      if (size() == ELEM_CAPACITY) {
        std::cout << SEG_NAME << " is too small for this app. Please enlarge it in paxos-op-queue.h\n"; 
        exit(1);
      }
      //lock();
      paxos_op op(conn_id, counter, t); // TBD: is this OK for IPC shared-memory?
      circbuff->push_back(op);      
      //unlock();
    }

    paxos_op front() {
      //lock();
      return circbuff->front();
      //unlock();
      //return op;    
    }

    paxos_op pop_front() {
      //lock();
      paxos_op op = front();
      circbuff->pop_front();
      //unlock();
      return op;
    }

    size_t size() {
      //lock();
      size_t t = circbuff->size();
      //unlock();
      return t;       
    }

    void lock() {
      sem_wait(sem);  
    }

    void unlock() {
      sem_post(sem);  
    }

  protected:
    void test() {
#ifdef DEBUG_PAXOS_OP_QUEUE
      std::cout << "Circular Buffer Size before push_back: " << circbuff->size() << "\n";
      for (int i = 0; i < ELEM_CAPACITY*2+123; i++) {
        paxos_op op(i, i, SEND);
        circbuff->push_back(op);
        //push_back(i, i, SEND); This code will trigger the circular buffer 
        // full exit, which is good.
      }
      std::cout << "Circular Buffer Size after push_back: " << circbuff->size() << "\n";

      for (int i = 0; i < 10; i++) {
        std::cout << "Child got: " << (*circbuff)[i].connection_id
          << ", " << (*circbuff)[i].counter << ", " << (*circbuff)[i].type << "\n";
      }

      paxos_op op0 = front();
      std::cout << "\n\nHead op: " << op0.connection_id << ", " << op0.counter << "\n";

      paxos_op op1 = pop_front();
      std::cout << "\n\nPopped op: " << op1.connection_id << ", " << op1.counter << "\n";
      paxos_op op2 = pop_front();
      std::cout << "\n\nPopped op: " << op2.connection_id << ", " << op2.counter << "\n";
      paxos_op op3 = pop_front();
      std::cout << "\n\nPopped op: " << op3.connection_id << ", " << op3.counter << "\n";

      std::cout << "\n\nCircular Buffer Size after pop: " << circbuff->size() << "\n";
      for (int i = 0; i < 10; i++) {
        std::cout << "Child got: " << (*circbuff)[i].connection_id
          << ", " << (*circbuff)[i].counter << "\n";
      }
      
#endif
    }
  };

  /*struct paxos_sched {
    unsigned numThdsWaitSockOp;
    proxy_server_sock_pair conns;
    paxos_op_queue paxos_queue;

    paxos_sched() {
      numThdsWaitSockOp = 0;
    }
  };*/
}

//#ifdef __cplusplus
//}
//#endif

#endif

