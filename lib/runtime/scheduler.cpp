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

/* Authors: Heming Cui (heming@cs.columbia.edu), Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code

#include "pthread.h"
#include "tern/runtime/scheduler.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include <stdlib.h>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "tern/options.h"

using namespace std;
using namespace tern;


Serializer::~Serializer()
{
  if (options::log_sync)
    fclose(logger);
  if (options::light_log_sync)
    fclose(loggerLight);
}

Serializer::Serializer(): 
  TidMap(pthread_self()), turnCount(0) 
{
  if (options::log_sync) {
    mkdir(options::output_dir.c_str(), 0777);
    std::string logPath = options::output_dir + "/serializer.log";
    logger = fopen(logPath.c_str(), "w");
    assert(logger);
  }
  if (options::light_log_sync) {
    mkdir(options::output_dir.c_str(), 0777);
    char buf[1024] = {0};
    snprintf(buf, 1024, "%s/serializer-light-pid-%d.log", 
      options::output_dir.c_str(), getpid());
    loggerLight = fopen(buf, "w");
    assert(loggerLight);
  }
}

unsigned Serializer::incTurnCount(const char *callerName, unsigned delta) { 
  unsigned newCnt = turnCount + 1 + delta;
  turnCount = newCnt;
  if (options::log_sync)
    fprintf(logger, "%d %u %s\n", (int) self(), turnCount, callerName);
  if (options::light_log_sync && self() != IdleThreadTid) {
    fprintf(loggerLight, "%d %u %s\n", self(), turnCount, callerName);
    fflush(loggerLight);
  }
  return turnCount;
}

unsigned Serializer::getTurnCount(void) { 
  return turnCount - 1; 
}
