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

#include <iterator>
#include <tr1/unordered_set>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

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

