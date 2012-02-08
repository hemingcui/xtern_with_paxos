#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
#include "common/util.h"
using namespace llvm;

#include "intra-slicer.h"
#include "util.h"
#include "macros.h"
#include "path-slicer.h"
using namespace tern;

using namespace klee;

IntraSlicer::IntraSlicer() {

}

IntraSlicer::~IntraSlicer() {}

/* Core function for intra-thread slicer. */
void IntraSlicer::detectInputDepRaces(uchar tid) {
  DynInstr *cur = NULL;
  while (!empty()) {
    cur = delTraceTail(tid);
    if (!cur)
      return;
    Instruction *instr = idMgr->getOrigInstr(cur);
    if (Util::isPHI(instr)) {
      handlePHI(cur);
    } else if (Util::isBr(instr)) {
      handleBranch(cur);
    } else if (Util::isRet(instr)) {
      handleRet(cur);
    } else if (Util::isCall(instr)) { /* Invoke is lowered to call. */
      handleCall(cur);
    } else if (Util::isMem(instr)) {
      handleMem(cur);
    } else { /* Handle all the other non-memory instructions. */
      handleNonMem(cur);
    }
  }
}

bool IntraSlicer::empty() {
  return curIndex != SIZE_T_INVALID;
}

DynInstr *IntraSlicer::delTraceTail(uchar tid) {
  ASSERT(curIndex < trace->size());
  DynInstr *dynInstr = NULL;
  while (!empty()) {
    if (trace->at(curIndex)->getTid() == tid) {
      dynInstr = trace->at(curIndex);
      curIndex--;
      break;
    } else
      curIndex--;
  }
  if (!dynInstr)
    stat->printDynInstr(dynInstr, "IntraSlicer::delTraceTail");
  return dynInstr;
}

void IntraSlicer::init(klee::ExecutionState *state, OprdSumm *oprdSumm, FuncSumm *funcSumm,
      InstrIdMgr *idMgr, const DynInstrVector *trace, size_t startIndex) {
  this->state = state;
  this->oprdSumm = oprdSumm;
  this->funcSumm = funcSumm;
  this->idMgr = idMgr;
  this->trace = trace;
  curIndex = startIndex;
  live.clear();
  live.init(aliasMgr, idMgr);
}

void IntraSlicer::takeNonMem(DynInstr *dynInstr, uchar reason) {
  delRegOverWritten(dynInstr);
  live.addUsedRegs(dynInstr);
  slice->add(dynInstr, reason);
}

void IntraSlicer::delRegOverWritten(DynInstr *dynInstr) {
  DynOprd destOprd(dynInstr, -1);
  live.delReg(&destOprd);
}

bool IntraSlicer::regOverWritten(DynInstr *dynInstr) {
  DynOprd destOprd(dynInstr, -1);
  return live.regIn(&destOprd);
}

DynInstr *IntraSlicer::getCallInstrWithRet(DynInstr *retDynInstr) {
  return NULL;
}

bool IntraSlicer::retRegOverWritten(DynInstr *dynInstr) {
  DynInstr *callDynInstr = getCallInstrWithRet(dynInstr);
  return regOverWritten(callDynInstr);
}

bool IntraSlicer::eventBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr) {
  Instruction *prevInstr = idMgr->getOrigInstr((DynInstr *)dynBrInstr);
  BranchInst *branch = dyn_cast<BranchInst>(prevInstr);
  assert(branch);
  Instruction *postInstr = idMgr->getOrigInstr((DynInstr *)dynPostInstr);
  return funcSumm->eventBetween(branch, postInstr);
}

bool IntraSlicer::writtenBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr) {
  bdd bddBetween = bddfalse;
  oprdSumm->getStoreSummBetween((DynInstr *)dynBrInstr, dynPostInstr, bddBetween);
  const bdd bddOfLive = live.getAllLoadMem();
  return (bddBetween & bddOfLive) != bddfalse;
}

bool IntraSlicer::mayWriteFunc(DynRetInstr *dynRetInstr, Function *func) {
  DynCallInstr *dynCallInstr = dynRetInstr->getDynCallInstr();
  bdd bddOfCall = bddfalse;
  oprdSumm->getStoreSummInFunc(dynCallInstr, bddOfCall);
  const bdd bddOfLive = live.getAllLoadMem();
  return (bddOfCall & bddOfLive) != bddfalse;
}

bool IntraSlicer::mayCallEvent(DynInstr *dynInstr, Function *func) {
  return funcSumm->mayCallEvent(dynInstr);
}

void IntraSlicer::handlePHI(DynInstr *dynInstr) {
  DynPHIInstr *phiInstr = (DynPHIInstr*)dynInstr;
  if (regOverWritten(phiInstr)) {
    delRegOverWritten(phiInstr);
    uchar index = phiInstr->getIncomingIndex();
    DynOprd oprd(phiInstr, index);
    if (oprd.isConstant()) {
      live.addReg(&oprd);
    } else {
      DynInstr *prevInstr = prevDynInstr(phiInstr);
      prevInstr->setTaken(INTRA_PHI_BR_CTRL_DEP);
    }
    slice->add(phiInstr, INTRA_PHI);
  }
}

void IntraSlicer::handleBranch(DynInstr *dynInstr) {
  DynBrInstr *brInstr = (DynBrInstr*)dynInstr;
  if (brInstr->isTaken() || brInstr->isTarget()) {
    takeNonMem(brInstr);
  } else {
    DynInstr *head = slice->getHead();
    if (!postDominate(head, brInstr) || eventBetween(brInstr, head) ||
      writtenBetween(brInstr, head)) {
      takeNonMem(brInstr);
    }
  }
}

void IntraSlicer::handleRet(DynInstr *dynInstr) {
  DynRetInstr *retInstr = (DynRetInstr*)dynInstr;
  Instruction *instr = idMgr->getOrigInstr(retInstr);
  Function *calledFunc = instr->getParent()->getParent();
  if (retRegOverWritten(retInstr)) {
    delRegOverWritten(retInstr);
    live.addUsedRegs(retInstr);
    slice->add(retInstr, INTRA_RET_REG_OW);
  } else {
    bool reason1 = mayCallEvent(retInstr, calledFunc);
    bool reason2 = mayWriteFunc(retInstr, calledFunc);
    if (reason1 && reason2)
      slice->add(retInstr, INTRA_RET_BOTH);
    else if (reason1 && !reason2)
      slice->add(retInstr, INTRA_RET_CALL_EVENT);
    else if (!reason1 && reason2)
      slice->add(retInstr, INTRA_RET_WRITE_FUNC);
    else
      removeRange(retInstr);
  }
}

void IntraSlicer::handleCall(DynInstr *dynInstr) {
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
  if (funcSumm->isInternalCall(callInstr)) {
    takeNonMem(callInstr);
  } else {
    // TBD: QUERY FUNCTION SUMMARY.
  }
}

void IntraSlicer::handleNonMem(DynInstr *dynInstr) {
  if (regOverWritten(dynInstr))
    takeNonMem(dynInstr);
}

void IntraSlicer::handleMem(DynInstr *dynInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::isLoad(instr)) {
    DynMemInstr *loadInstr = (DynMemInstr*)dynInstr;
    bool reason1 = loadInstr->isTarget();
    bool reason2 = regOverWritten(loadInstr);
    if (reason1 || reason2) {
      if (reason2)
        delRegOverWritten(loadInstr);
      DynOprd loadPtrOprd(loadInstr, 0);
      live.addReg(&loadPtrOprd);
      live.addLoadMem(loadInstr);
      if (reason1 /* no matter whether reason2 is true */)
        slice->add(loadInstr, INTER_LOAD_TGT);
      else if (reason2)
        slice->add(loadInstr, INTRA_LOAD_OW);
    }
  } else {
    DynMemInstr *storeInstr = (DynMemInstr*)dynInstr;
    DynOprd storeValue(storeInstr, 0);
    DynOprd storePtrOprd(storeInstr, 1);
    if (storeInstr->isTarget()) {
      live.addReg(&storePtrOprd);
      slice->add(storeInstr, INTER_STORE_TGT);
    }
    DenseSet<DynInstr *> loadInstrs = live.getAllLoadInstrs();
    DenseSet<DynInstr *>::iterator itr(loadInstrs.begin());
    for (; itr != loadInstrs.end(); ++itr) {
      DynMemInstr *loadInstr = (DynMemInstr*)(*itr);
      DynOprd loadPtrOprd(loadInstr, 0);
      if (mustAlias(&loadPtrOprd, &storePtrOprd)) {
        if (loadInstr->isAddrSymbolic() || storeInstr->isAddrSymbolic()) {
          addMemAddrEqConstr(loadInstr, storeInstr);
        }
        live.delLoadMem(loadInstr);
        live.addReg(&storeValue);
        if (!storeInstr->isTaken()) {
          live.addReg(&storePtrOprd);
          slice->add(storeInstr, INTRA_STORE_OW);
        }
      } else if (aliasMgr->mayAlias(&loadPtrOprd, &storePtrOprd)) {
        live.addReg(&storeValue);
        if (!storeInstr->isTaken()) {
          live.addReg(&storePtrOprd);
          slice->add(storeInstr, INTRA_STORE_ALIAS);
        }
      }
    }
  }
}

bool IntraSlicer::mustAlias(DynOprd *oprd1, DynOprd *oprd2) {
  const Value *v1 = oprd1->getStaticValue();
  const Value *v2 = oprd2->getStaticValue();
  return isa<Constant>(v1) && isa<Constant>(v2) && v1 == v2;
}

DynInstr *IntraSlicer::prevDynInstr(DynInstr *dynInstr) {
  uchar tid = dynInstr->getTid();
  size_t tmpIndex = curIndex;
  DynInstr *prevInstr = NULL;
  while (tmpIndex != SIZE_T_INVALID) {
    if (trace->at(tmpIndex)->getTid() == tid) {
      prevInstr = trace->at(tmpIndex);
      break;
    } else
      tmpIndex--;
  }
  return prevInstr;
}

bool IntraSlicer::postDominate(DynInstr *dynPostInstr, DynInstr *dynPrevInstr) {
  return cfgMgr->postDominate(idMgr->getOrigInstr(dynPrevInstr),
    idMgr->getOrigInstr(dynPostInstr));
}

void IntraSlicer::removeRange(DynRetInstr *dynRetInstr) {
  size_t callIndex = 0;
  DynCallInstr *call = dynRetInstr->getDynCallInstr();
  if (call)
    callIndex = call->getIndex();
  while (curIndex != callIndex) {
    assert(!trace->at(curIndex)->isTaken());
    curIndex--;
  }
  assert(!trace->at(curIndex)->isTaken());
  curIndex--;
}

void IntraSlicer::addMemAddrEqConstr(DynMemInstr *loadInstr,
  DynMemInstr *storeInstr) {
  // Shall we add the constraint to constraints of current ExecutionState?
}


