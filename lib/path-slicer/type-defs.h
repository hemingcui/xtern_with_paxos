#ifndef __TERN_PATH_SLICER_TYPE_DEFS_H
#define __TERN_PATH_SLICER_TYPE_DEFS_H

#include <vector>
#include "llvm/Value.h"

typedef std::pair<long, long> LongPair;
typedef std::pair<LongPair, LongPair> LongLongPair;
typedef std::pair< std::vector<int> *, const llvm::Value * > CtxVPair;
typedef llvm::DenseSet< CtxVPair > CtxVDenseSet;
typedef std::vector<int> CallCtx;
typedef llvm::DenseSet<const llvm::Instruction * > InstrDenseSet;

#endif