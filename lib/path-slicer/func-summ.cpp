
#include "func-summ.h"
//#include "tern/syncfuncs.h"
using namespace tern;

#include "llvm/Instructions.h"
using namespace llvm;

char tern::FuncSumm::ID = 0;

FuncSumm::FuncSumm(): ModulePass(&ID) {
  
}

FuncSumm::~FuncSumm() {
  fprintf(stderr, "FuncSumm::~FuncSumm\n");
}

void FuncSumm::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<EventMgr>();
  ModulePass::getAnalysisUsage(AU);
}

bool FuncSumm::runOnModule(Module &M) {
  collectInternalFunctions(M);
  return false;
}

bool FuncSumm::mayCallEvent(const llvm::Function *f) {
  long intF = (long)f;  
  EventMgr &EM = getAnalysis<EventMgr>();
  return EM.mayCallEvent((Function *)intF);
}

bool FuncSumm::mayCallEvent(DynInstr *dynInstr) {
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
  Function *f = callInstr->getCalledFunc();
  assert(f);
  return mayCallEvent(f);
}

bool FuncSumm::eventBetween(llvm::BranchInst *prevInstr, llvm::Instruction *postInstr) {
  EventMgr &EM = getAnalysis<EventMgr>();
  return EM.eventBetween(prevInstr, postInstr);
}

void FuncSumm::collectInternalFunctions(Module &M) {
  fprintf(stderr, "FuncSumm::collectInternalFunctions begin\n");
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {
    if (!f->isDeclaration()) {
      internalFunctions.insert(f);
      fprintf(stderr, "Function %s(%p) is internal.\n", 
        f->getNameStr().c_str(), (void *)f);
    }
  }
  fprintf(stderr, "FuncSumm::collectInternalFunctions end\n");
}


bool FuncSumm::isInternalCall(const Instruction *instr) {
  const CallInst *ci = dyn_cast<CallInst>(instr);
  assert(ci);
  const Function *f = ci->getCalledFunction();  
  if (!f) // If it is an indirect call (function pointer), return false conservatively.
    return false;
  else
    return isInternalFunction(f);
}

bool FuncSumm::isInternalCall(DynInstr *dynInstr) {
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
  Function *f = callInstr->getCalledFunc();
  return isInternalFunction(f);
}

bool FuncSumm::isInternalFunction(const Function *f) {
  // TBD: If the called function is a function pointer, would "f" be NULL?
  if (!f)
    return false;
  
  /*fprintf(stderr, "Function %s(%p) is isInternalFunction?\n", 
    f->getNameStr().c_str(), (void *)f);*/
  bool result = !f->isDeclaration() && internalFunctions.count(f) > 0;
  /*fprintf(stderr, "Function %s is isInternalFunction %d.\n", 
    f->getNameStr().c_str(), result);*/
  return result;
}

