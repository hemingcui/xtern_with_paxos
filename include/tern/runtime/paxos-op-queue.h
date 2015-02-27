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

#include <iostream>
#include <string>
#include <cstdlib>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/circular_buffer.hpp>

#define ELEM_CAPACITY 10000
#define DELTA 100 // TBD: don't know why we couldn't get 100% of the shared mem.
#define SEG_NAME "PAXOS_OP_QUEUE"
#define CB_NAME "CIRCULAR_BUFFER"
#define DEBUG_PAXOS_OP_QUEUE

  struct paxos_op {
    uint64_t connection_id;
    uint64_t counter;

    paxos_op(uint64_t id, uint64_t cnt) {
      connection_id = id;
      counter = cnt;
    }
  };

namespace bip = boost::interprocess;
typedef bip::allocator<paxos_op, bip::managed_shared_memory::segment_manager> ShmemAllocatorCB;
typedef boost::circular_buffer<paxos_op, ShmemAllocatorCB> MyCircularBuffer;

namespace tern
{
  

  class paxos_op_queue {
  private:
    MyCircularBuffer *circbuff;
    
  public:
    paxos_op_queue() {

    }

    void create_shared_mem() {
#ifdef DEBUG_PAXOS_OP_QUEUE
      std::cout << "Init shared memory " << (bip::shared_memory_object::remove(SEG_NAME) ?
        "cleaned up: " : "not exist: " ) << SEG_NAME << "\n";
#else
      bip::shared_memory_object::remove(SEG_NAME);
#endif
      bip::managed_shared_memory segment(bip::create_only, SEG_NAME, ELEM_CAPACITY*sizeof(paxos_op));
      const ShmemAllocatorCB alloc_inst (segment.get_segment_manager());
      circbuff = segment.construct<MyCircularBuffer>(CB_NAME)(ELEM_CAPACITY-DELTA, alloc_inst);
      test();
    }

    void open_shared_mem() {
      bip::managed_shared_memory segment(bip::open_only, SEG_NAME);
      circbuff = segment.find<MyCircularBuffer>(CB_NAME).first;
    }

    virtual ~paxos_op_queue() {
#ifdef DEBUG_PAXOS_OP_QUEUE
      //std::cout << (bip::shared_memory_object::remove(SEG_NAME) ? "removed: " : "failed to remove: " )
        //<< SEG_NAME << "\n";
#endif
    }

    void push_back(uint64_t conn_id, uint64_t counter) {
      paxos_op op(conn_id, counter); // TBD: is this OK for IPC shared-memory?
      circbuff->push_back(op);      
    }

    paxos_op& front() {
      return circbuff->front();      
    }

    paxos_op pop_front() {
      paxos_op op = front();
      circbuff->pop_front();
      return op;
    }

    size_t size() {
      return circbuff->size();      
    }

    void test() {
#ifdef DEBUG_PAXOS_OP_QUEUE
      std::cout << "Circular Buffer Size before push_back: " << circbuff->size() << "\n";
      for (int i = 0; i < ELEM_CAPACITY*2+123; i++) {
        paxos_op op(i, i);
        circbuff->push_back(op);
      }
      std::cout << "Circular Buffer Size after push_back: " << circbuff->size() << "\n";

      for (int i = 0; i < 10; i++) {
        std::cout << "Child got: " << (*circbuff)[i].connection_id
          << ", " << (*circbuff)[i].counter << "\n";
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
}

#if 0
namespace tern {
class paxos_op_queue {
public:
  /** Currently there are only three types of socket operations
  in the paxos operation queue, because we only need to replicate
  a server application's execution states. **/
  enum PAXOS_OP {
    CONNECT,
    SEND,
    CLOSE
  };
  
  struct paxos_op {
  public:
    int proxy_sock_fd;
    PAXOS_OP op;
    
    paxos_op(int proxy_sock_fd, PAXOS_OP op) {
      this->proxy_sock_fd = proxy_sock_fd;
      this->op = op;
    }
  };

private:
  /** TBD: a boost std list for shared memory. **/

public:

  paxos_op_queue() {
    
  }
  
  /** Called in the TimeAlgo() by DMT. **/
  bool empty(){ 
    return true;
  }
  
  /** Called by proxy after the proxy does the actual connect/
  send/close libevent operation. The "before/after" does not matter in 
  correctness because the !empty() check in TimeAlgo().
  I use after just for performance (the socket operation at server 
  side may return sooner 
  because the libevent operation has been done). **/
  void push(paxos_op op) {

  }

  /** Called by the DMT in TimeAlgo(). **/
  paxos_op pop() {
    paxos_op *op = new paxos_op(0, SEND);
    return *op;
  }

  /** Return head element without popping. **/
  paxos_op& head() {
    paxos_op *op = new paxos_op(0, SEND);
    return *op;
  }
}; 
}
#endif
#endif

