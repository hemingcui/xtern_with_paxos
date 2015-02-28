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

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum {
    CONNECT = 0,
    SEND,
    CLOSE
  } PAXOS_OP_TYPE;

  typedef struct {
    uint64_t connection_id;
    uint64_t counter;
    PAXOS_OP_TYPE type;
  } paxos_op;  

  void conns_init();
  uint64_t conns_get_conn_id(int server_sock);
  int conns_get_server_sock(uint64_t conn_id);
  void conns_erase_by_conn_id(uint64_t conn_id);
  void conns_add_pair(uint64_t conn_id, int server_sock);
  size_t conns_get_num_conn();

   void paxq_create_shared_mem();
    void paxq_open_shared_mem(int node_id);
    void paxq_push_back(uint64_t conn_id, uint64_t counter, PAXOS_OP_TYPE t);
    paxos_op paxq_front();
    paxos_op paxq_pop_front();
    size_t paxq_size();
    void paxq_lock();
    void paxq_unlock();
    void paxq_test();

  /*struct paxos_sched {
    unsigned numThdsWaitSockOp;
    proxy_server_sock_pair conns;
    paxos_op_queue paxos_queue;

    paxos_sched() {
      numThdsWaitSockOp = 0;
    }
  };*/

#ifdef __cplusplus
}
#endif

#endif

