#ifndef __TERN_PATH_SLICER_EVENT_MANAGER_H
#define __TERN_PATH_SLICER_EVENT_MANAGER_H

#include <set>

#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/BasicBlock.h"
#include "llvm/Pass.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/BasicInliner.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/ADT/DenseSet.h"

#include "common/callgraph-fp.h"
#include "common/typedefs.h"
#include "klee/BasicCheckers.h"

namespace tern {
  struct EventMgr: public llvm::ModulePass {	
  public:
    llvm::Module *module;
    static char ID;

  protected:
    typedef llvm::DenseMap<const llvm::Instruction *, FuncList> SiteFuncMapping;
    typedef llvm::DenseMap<const llvm::Function *, InstList> FuncSiteMapping;
		
    llvm::DenseMap<llvm::Function *, llvm::Function *> parent; // Used in DFS
    llvm::DenseSet<llvm::Function *> mayCallEventFuncs;
    DenseMap<BasicBlock *, bool> bbVisited;
    llvm::DenseSet<llvm::Function *> eventFuncs;
    llvm::DenseSet<llvm::Instruction *> eventCallSites;
    klee::Checker *checker;
    llvm::CallGraphFP *CG;

    bool is_exit_block(llvm::BasicBlock *bb);
    void DFSCollectMayCallEventFuncs(llvm::Function *f);
    void DFS(llvm::BasicBlock *x, llvm::BasicBlock *sink);
    void traverse_call_graph(llvm::Module &M);
    void setupEvents(llvm::Module &M);
    void collectStaticEventCalls(llvm::Function *event);
    static bool isIgnoredEventCall(llvm::Instruction *call, llvm::Function *event);
    static bool isStdErrOrOut(llvm::BasicBlock *curBB, llvm::Value *v);

  public:
    EventMgr();
    ~EventMgr();
    void initCallGraph(llvm::CallGraphFP *CG);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual bool runOnModule(llvm::Module &M);
    bool mayCallEvent(llvm::Function *f);
    bool eventBetween(llvm::BranchInst *prevInstr, llvm::Instruction *postInstr);
    bool isEventCall(llvm::Instruction *instr);
    void output(const llvm::Module &M) const;
    size_t numEventCallSites();
    void printEventCalls();
  };
}

#endif
