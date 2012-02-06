#ifndef __TERN_PATH_SLICER_DYN_INSTRS_H
#define __TERN_PATH_SLICER_DYN_INSTRS_H

#include <vector>
#include <list>
namespace tern {
  class DynInstr;
  /* Composed types of a dynamic instruction. */
  typedef std::vector<DynInstr *> DynInstrVector;
  typedef std::list<DynInstr *> DynInstrList;
  typedef DynInstrList::iterator DynInstrItr;
}

#include "klee/Expr.h"

#include "dyn-oprd.h"
#include "instr-region.h"

namespace tern {
  class InstrRegion;
  /* We must include as few as fields in this class in order to save memory. 
    The main idea to get memory efficiency is to keep as many as information in its region.
    Only keep fields that MUST belong to all dynamic instructions in this class. */
  class DynInstr {
  private:

  protected:
    /* A pointer to the region that this instr belongs to, the region contains as many as shared info
      of instructions within this region, such as thread id and vector clock. */
    //InstrRegion *region;
    // TBD: this should be here when I start to implement inter-thread phase.

    /* The total order of execution index of this dynamic instr.
        Question: does xtern still have this feature? */
    size_t index;

    /* In normal and max slicing mode, this is the normal and max sliced calling context, respectively. */
    CallCtx *callingCtx;

    /* This is only valid in simplified slicing mode. 
      For each dynamic instruction, there should be only one callstack in simplified bc, right?
      TODO: MUST THINK OF A NICE WAY TO ELIMINATE THIS MEMORY TOO. */
    CallCtx *simCallingCtx;

    /* The static instruction id from orig bc module. */
    int instrId;

    /* The xtern self-maintanied thread id. */
    uchar tid;

    /* Identify a dynamic instruction is taken or not; 0 is not taken; and !0 means taken reaons. */
    uchar takenFlag;

  public:
    DynInstr();
    ~DynInstr();

    /* TBD */
    void setTid(uchar tid);

    /* TBD */
    uchar getTid();

    /* TBD */
    void setIndex(size_t index);

    /* TBD */
    size_t getIndex();

    /* TBD */
    void setCallingCtx(std::vector<int> *ctx);

    /* TBD */
    CallCtx *getCallingCtx();

    /* TBD */
    void setSimCallingCtx(std::vector<int> *ctx);

    /* TBD */
    CallCtx *getSimCallingCtx();

    /* TBD */
    void setOrigInstrId(int instrId);

    /* TBD */
    int getOrigInstrId();

    /* TBD */
    void setTaken(uchar takenFlag);

    /* TBD */
    bool isTaken();

    /* TBD */
    const char *takenReason();

    /* Whether an instruction is a slicing target already, currently it is the same as taken. */
    bool isTarget();
  };

  class DynPHIInstr: public DynInstr {
  private:
    unsigned incomingIndex;

  protected:
    
  public:
    DynPHIInstr();
    ~DynPHIInstr();
    void setIncomingIndex(unsigned index);
    unsigned getIncomingIndex();
    
  };

  class DynBrInstr: public DynInstr {
  private:

  protected:

  public:
    DynBrInstr();
    ~DynBrInstr();
    
  };

  class DynCallInstr: public DynInstr {
  private:

  protected:
    /* This called function is the recorded one during execution, so it can solve the inambiguity
    in function pointers. Need to add constraint assertion to fix this called one. */
    llvm::Function *calledFunc;

  public:
    DynCallInstr();
    ~DynCallInstr();
    void setCalledFunc(llvm::Function *f);
    llvm::Function *getCalledFunc();    
  };

  class DynRetInstr: public DynInstr {
  private:
    /* The dynamic call instruction corresponds to this return instruction. */
    DynCallInstr *dynCallInstr;

  protected:

  public:
    DynRetInstr();
    ~DynRetInstr();
    void setDynCallInstr(DynCallInstr *dynInstr);
    DynCallInstr *getDynCallInstr();
    
  };

  class DynSpawnThreadInstr: public DynCallInstr {
  private:
    int childTid;

  protected:
    

  public:
    DynSpawnThreadInstr();
    ~DynSpawnThreadInstr();
    void setChildTid(int childTid);
    int getChildTid();
  };

  class DynMemInstr: public DynInstr {
  private:
    /* Loaded or stored memory address from the pointer operand. */
    klee::ref<klee::Expr> addr;

  protected:

  public:
    DynMemInstr();
    ~DynMemInstr();
    void setAddr(klee::ref<klee::Expr> symAddr);
    klee::ref<klee::Expr> getAddr();
    bool isAddrSymbolic();
  };
}

#endif


