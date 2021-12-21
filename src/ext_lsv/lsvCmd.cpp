#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"

#include <string>
#include <vector>
#include <list>

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBiDec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBiDec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// ---------------------------------------
// lsv_print_msfc functions (PA1)
// ---------------------------------------
void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int aFanout[Abc_NtkNodeNum(pNtk)], aGroupId[Abc_NtkNodeNum(pNtk)];
  std::vector<std::string> vPrint;
  std::list<int> lOrder;

  // complete aFanout
  int i, shift = -1;
  Abc_NtkForEachNode(pNtk, pObj, i){
    if(shift == -1) shift = Abc_ObjId(pObj);
    // printf("Object Id = %d, name = %s, fanoutNum = %d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), Abc_ObjFanoutNum(pObj));
    if(Abc_ObjFanoutNum(pObj) == 1 && Abc_ObjIsNode(Abc_ObjFanout0(pObj))){
      aFanout[Abc_ObjId(pObj)-shift] = Abc_ObjId(Abc_ObjFanout0(pObj));
    }
    else{
      aFanout[Abc_ObjId(pObj)-shift] = Abc_ObjId(pObj);
    }
  }

  // complete aGroupId
  int maxGroupId = -1, groupId;
  Abc_NtkForEachNodeReverse(pNtk, pObj, i){
    if(aFanout[i-shift] == i){
      groupId = ++maxGroupId;
      vPrint.push_back(std::string(Abc_ObjName(pObj)) + "\n");
    }
    else{
      groupId = aGroupId[aFanout[i-shift]-shift];
      vPrint[groupId] = std::string(Abc_ObjName(pObj)) + "," + vPrint[groupId];
    }
    aGroupId[i-shift] = groupId;

    if(groupId != lOrder.front() || lOrder.empty()){
      if(groupId <= maxGroupId){
        lOrder.remove(groupId);
      }
      lOrder.push_front(aGroupId[i-shift]);
    }
  }
  
  i = 0;
  // handle Const1 case
  if(Abc_ObjFanoutNum(Abc_AigConst1(pNtk)) > 0){
    printf("MSFC %d: %s\n", i, Abc_ObjName(Abc_AigConst1(pNtk)));
    ++i;
  }
  
  for(std::list<int>::iterator it = lOrder.begin(); it != lOrder.end(); ++it){
    printf("MSFC %d: %s", i, vPrint[(*it)].c_str());
    ++i;
  }
  
  // pObj = Abc_AigConst1(pNtk);
  // printf("Object Id = %d, name = %s, fanoutNum = %d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), Abc_ObjFanoutNum(pObj));
}

int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
// ---------------------------------------
// lsv_or_bidec functions (PA2)
// ---------------------------------------
void Lsv_NtkOrBiDec(Abc_Ntk_t* pNtk) {
  Abc_Ntk_t *pCone;
  Abc_Obj_t *pPo, *pPoNode;
  Aig_Man_t *pAigMan;
  Aig_Obj_t *pAigObj;
  Cnf_Dat_t *pCnf;
  sat_solver_t *pSat;
  lit clause[3];
  lit *assumpList;
  int sat;
  int *pFinalConf;
  int FinalConfSize;
  int *resultList;

  int i, j;
  int *pBeg, *pEnd;
  int POvarId, varShift;
  int supportNum, aBegId, bBegId;
  int varId;

  // pNtk = Abc_NtkStrash(pNtk, 1, 1, 0);

  Abc_NtkForEachPo(pNtk, pPo, i) {
    pPoNode = Abc_ObjFanin0(pPo);
    pCone = Abc_NtkCreateCone(pNtk, pPoNode, Abc_ObjName(pPo), 0);
    if (Abc_ObjFaninC0(pPo))
      Abc_ObjXorFaninC(Abc_NtkPo(pCone, 0), 0);
    
    // pCone = Abc_NtkStrash(pCone, 1, 1, 0);
    pAigMan = Abc_NtkToDar(pCone, 0, 0);

    // f(X)
    pCnf = Cnf_Derive(pAigMan, 1);
    pSat = (sat_solver_t*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    varShift = pCnf->nVars;
    POvarId = pCnf->pVarNums[Aig_ManCo(pAigMan, 0)->Id];
    clause[0] = toLitCond(POvarId, 0);
    sat_solver_addclause(pSat, clause, clause+1);

    printf("PO %s support partition: ", Abc_ObjName(pPo));
    /*
    printf("VarNum(varShift) = %d\n", varShift);
    printf("Solver Size = %d\n", pSat->size);
    */
    // f(X')
    Cnf_DataLift(pCnf, varShift);
    Cnf_CnfForClause(pCnf, pBeg, pEnd, j) {
      sat_solver_addclause(pSat, pBeg, pEnd);
    }
    clause[0] = toLitCond(POvarId + varShift, 1);
    sat_solver_addclause(pSat, clause, clause+1);
    /*
    printf("Solver Size = %d\n", pSat->size);
    */
    // f(X'')
    Cnf_DataLift(pCnf, varShift);
    Cnf_CnfForClause(pCnf, pBeg, pEnd, j) {
      sat_solver_addclause(pSat, pBeg, pEnd);
    }
    clause[0] = toLitCond(POvarId + 2*varShift, 1);
    sat_solver_addclause(pSat, clause, clause+1);
    /*
    printf("Solver Size = %d\n", pSat->size);
    */
    // a & b
    supportNum = Aig_ManCiNum(pAigMan);
    aBegId = 3*varShift;
    bBegId = 3*varShift + supportNum;
    sat_solver_setnvars(pSat, 3*varShift + 2*supportNum);
    /*
    printf("SupportNum = %d\n", supportNum);
    printf("Solver Size = %d\n", pSat->size);
    */
    
    Aig_ManForEachCi(pAigMan, pAigObj, j) {
      varId = pCnf->pVarNums[pAigObj->Id] - 2*varShift;
      // x equ x' or a
      clause[0] = toLitCond(varId, 1);
      clause[1] = toLitCond(varId + varShift, 0);
      clause[2] = toLitCond(aBegId + j, 0);
      sat_solver_addclause(pSat, clause, clause+3);
      clause[0] = toLitCond(varId + varShift, 1);
      clause[1] = toLitCond(varId, 0);
      clause[2] = toLitCond(aBegId + j, 0);
      sat_solver_addclause(pSat, clause, clause+3);
      // x equ x'' or b
      clause[0] = toLitCond(varId, 1);
      clause[1] = toLitCond(varId + 2*varShift, 0);
      clause[2] = toLitCond(bBegId + j, 0);
      sat_solver_addclause(pSat, clause, clause+3);
      clause[0] = toLitCond(varId + 2*varShift, 1);
      clause[1] = toLitCond(varId, 0);
      clause[2] = toLitCond(bBegId + j, 0);
      sat_solver_addclause(pSat, clause, clause+3);
    }

    // set assumptions
    assumpList = new lit[2*supportNum];
    resultList = new int[supportNum];
    for (int k = 0; k < 2*supportNum; k++) {
      assumpList[k] = toLitCond(aBegId + k, 1);
    }
    for (int k = 0; k < supportNum; k++) {
      resultList[k] = 3;
    }

    sat = 0;
    for (int a = 0; a < supportNum; a++) {
      assumpList[a] = lit_neg(assumpList[a]);
      for (int b = a+1+supportNum; b < 2*supportNum; b++) {
        assumpList[b] = lit_neg(assumpList[b]);
        /*
        for (int k = 0; k < 2*supportNum; k++) {
          printf("%d ",lit_print(assumpList[k]));
          if(k%supportNum == supportNum-1){printf("\n");}
        }
        printf("\n");
        */

        // sat solve
        sat = sat_solver_solve(pSat, assumpList, assumpList + 2*supportNum, 0, 0, 0, 0);
        if (sat == -1) {
          FinalConfSize = sat_solver_final(pSat, &pFinalConf);
          for (int k = 0; k < FinalConfSize; k++){
            assert(lit_var(pFinalConf[k]) >= aBegId);
            if (lit_var(pFinalConf[k]) < bBegId){
              resultList[lit_var(pFinalConf[k])-aBegId] -= 2;
            }
            else {
              resultList[lit_var(pFinalConf[k])-bBegId] -= 1;
            }
          }

          printf("1\n");
          for (int k = 0; k < supportNum; k++) {
            printf("%d",resultList[k]);
          }
          /* getchar(); */
          break;
        }
        assumpList[b] = lit_neg(assumpList[b]);
      }
      assumpList[a] = lit_neg(assumpList[a]);
      if (sat == -1) break;
    }
    if (sat == -1) printf("\n");
    else printf("0\n");

    delete [] resultList;
    delete [] assumpList;
  }
}

int Lsv_CommandOrBiDec(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkOrBiDec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ---------------------------------------
// lsv_print_nodes functions (example)
// ---------------------------------------
void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }
    if (Abc_NtkHasSop(pNtk)) {
      printf("The SOP of this node:\n%s", (char*)pObj->pData);
    }
  }
}

int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintNodes(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}