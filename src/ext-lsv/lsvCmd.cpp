#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

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

void Lsv_BuildSFC(Abc_Obj_t* pObj, std::set<int>& sfc){
  sfc.insert(Abc_ObjId(pObj));
  Abc_Obj_t* pFanin;
  int j;
  Abc_ObjForEachFanin(pObj, pFanin, j) {
    if(Abc_ObjIsPo(pFanin) || Abc_ObjIsPi(pFanin)){
      continue;
    }
    if(Abc_ObjFanoutNum(pFanin) == 1){
      sfc.insert(Abc_ObjId(pFanin));
      Lsv_BuildSFC(pFanin, sfc);
    }
  }
}

struct compare_sfc{
  inline bool operator() (const std::set<int>& s1, const std::set<int>& s2){
    return(*s1.begin() < *s2.begin());
  }
};
void Lsv_PrintSFCS(std::vector< std::set<int> >& sfcs) {
  std::sort(sfcs.begin(), sfcs.end(), compare_sfc());
  for(size_t i=0; i<sfcs.size(); ++i){
    std::cout << "MSFC " << i << ": ";
    for(auto ite = sfcs[i].begin(); ite != sfcs[i].end(); ){
      std::cout << "n" << *ite;
      if(++ite != sfcs[i].end()){
        std::cout << ",";
      }
      else{
        std::cout << "\n";
      }
    }
  }
}

void Lsv_PrintMSFC(Abc_Ntk_t* pNtk){
  std::vector< std::set<int> > sfcs;
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachObj(pNtk, pObj, i){
    if(Abc_ObjType(pObj) == ABC_OBJ_CONST1 || Abc_ObjType(pObj) == ABC_OBJ_NODE){
      if(Abc_ObjFanoutNum(pObj) == 1){
        Abc_Obj_t* fanout = Abc_ObjFanout(pObj, 0);
        if(!Abc_ObjIsPo(fanout) && (fanout != pObj)){
          continue;
        }
      }
      if(Abc_ObjFanoutNum(pObj) == 0){
        continue;
      }
      std::set<int> sfc;
      Lsv_BuildSFC(pObj, sfc);
      sfcs.push_back(sfc);
    }
  }
  Lsv_PrintSFCS(sfcs);
}

int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_PrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the MSFCs in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void print_cnf(Cnf_Dat_t* cnf, Aig_Man_t* aig){
  std::cout << "----------CNF----------\n";
  std::cout << "Num. of variables: " << cnf->nVars << "\n";
  std::cout << "Num. of literals:  " << cnf->nLiterals << "\n";
  std::cout << "Num. of clauses:   " << cnf->nClauses << "\n";
  std::cout << "Clauses:\n";
  for(int i=0; i<cnf->nClauses; ++i){
    for(int* j=cnf->pClauses[i]; j<cnf->pClauses[i+1]; ++j){
      std::cout << *j << " ";
    }
    std::cout << "\n";
  }
  std::cout << "Node to variable:\n";
  Aig_Obj_t* obj;
  int i;
  Aig_ManForEachCi(aig, obj, i) {
    printf("Object Id = %d, ", Aig_ObjId(obj));
    printf("corresponding variable = %d\n", cnf->pVarNums[Aig_ObjId(obj)]);
  }
  std::cout << "\n";
}
void print_sat(sat_solver* sat){
  std::cout << "----------SAT----------\n";
  std::cout << "Num. of variables: " << sat->size << "\n";
  //std::cout << "Num. of literals:  " << cnf->nLiterals << "\n";
  std::cout << "Num. of clauses:   " << sat_solver_nclauses(sat) << "\n";
  //std::cout << "Clauses:\n";
}
void Lsv_OrBidec(Abc_Ntk_t* pNtk){
  Abc_Obj_t* po;
  int i;
  Abc_NtkForEachPo(pNtk, po, i){
    Abc_Ntk_t* currNtk;
    Aig_Man_t* currAig;
    Cnf_Dat_t* currCnf;
    sat_solver* currSat;
    lit outputLit[1];
    int varShift, poId;
    int* pBeg, * pEnd;
    int j;
    std::vector<int> supportVars;
    Aig_Obj_t* aigObj;
    //Create cone netlist and convert to aig
    currNtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(po), Abc_ObjName(po), 0);
    currAig = Abc_NtkToDar(currNtk, 0, 1);
    currCnf = Cnf_Derive(currAig, Aig_ManCoNum(currAig));
    
    //Add cnf for f(X)
    Aig_ManForEachCi(currAig, aigObj, j) {
      supportVars.push_back(currCnf->pVarNums[Aig_ObjId(aigObj)]);
    }
    poId = currCnf->pVarNums[Aig_ObjId(Aig_ManCo(currCnf->pMan, 0))];
    varShift = currCnf->nVars;
    currSat = (sat_solver*)Cnf_DataWriteIntoSolver(currCnf, 1, 0);
    outputLit[0] = toLitCond(poId, po->fCompl0);
    sat_solver_addclause(currSat, outputLit, outputLit + 1);
    
    //Add cnf for f(X')
    Cnf_DataLift(currCnf, varShift);
    poId += varShift;
    sat_solver_setnvars(currSat, sat_solver_nvars(currSat) + varShift);
    Cnf_CnfForClause(currCnf, pBeg, pEnd, j){
      sat_solver_addclause(currSat, pBeg, pEnd);
    }
    outputLit[0] = toLitCond(poId, !po->fCompl0);
    sat_solver_addclause(currSat, outputLit, outputLit + 1);
    
    //Add cnf for f(X'')
    Cnf_DataLift(currCnf, varShift);
    poId += varShift;
    sat_solver_setnvars(currSat, sat_solver_nvars(currSat) + varShift);
    Cnf_CnfForClause(currCnf, pBeg, pEnd, j){
      sat_solver_addclause(currSat, pBeg, pEnd);
    }
    outputLit[0] = toLitCond(poId, !po->fCompl0);
    sat_solver_addclause(currSat, outputLit, outputLit + 1);
    
    //Add cnf for control clauses
    sat_solver_setnvars(currSat, sat_solver_nvars(currSat) + 2*supportVars.size());
    for(int k=0; k<supportVars.size(); ++k){
      int xk      = supportVars[k];
      int x2k     = supportVars[k] + varShift;
      int x3k     = supportVars[k] + 2*varShift;
      int alphak  = k + 3*varShift;
      int betak   = k + supportVars.size() + 3*varShift;
      lit clause[3];
      clause[0] = toLitCond(xk, 1);
      clause[1] = toLitCond(x2k, 0);
      clause[2] = toLitCond(alphak, 0);
      sat_solver_addclause(currSat, clause, clause + 3);
      clause[0] = toLitCond(xk, 0);
      clause[1] = toLitCond(x2k, 1);
      clause[2] = toLitCond(alphak, 0);
      sat_solver_addclause(currSat, clause, clause + 3);
      clause[0] = toLitCond(xk, 1);
      clause[1] = toLitCond(x3k, 0);
      clause[2] = toLitCond(betak, 0);
      sat_solver_addclause(currSat, clause, clause + 3);
      clause[0] = toLitCond(xk, 0);
      clause[1] = toLitCond(x3k, 1);
      clause[2] = toLitCond(betak, 0);
      sat_solver_addclause(currSat, clause, clause + 3);
    }
    
    //Incremental sat solving
    bool found = false;
    for(int k=0; k<supportVars.size(); ++k){
      for(int l=k+1; l<supportVars.size(); ++l){
        int alphak = k + 3*varShift; //1
        int betak  = k + supportVars.size() + 3*varShift; //0
        int alphal = l + 3*varShift; //0
        int betal  = l + supportVars.size() + 3*varShift; //1
        lit* seed = new lit[2*supportVars.size()];
        for(int m=0; m<supportVars.size(); ++m){
          int alpha = m + 3*varShift;
          int beta = m + 3*varShift + supportVars.size();
          seed[m] = toLitCond(alpha, 1);
          seed[m+supportVars.size()] = toLitCond(beta, 1);
        }
        seed[k] = toLitCond(alphak, 0);
        seed[k+supportVars.size()] = toLitCond(betak, 1);
        seed[l] = toLitCond(alphal, 1);
        seed[l+supportVars.size()] = toLitCond(betal, 0);
        int status = sat_solver_solve(currSat, seed, seed+2*supportVars.size(), (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
        if(status == l_False){
          found = true;
          break;
        }
        delete seed;
      }
      if(found) break;
    }
    
    //Final conflict clause
    bool* partitions = new bool[2*supportVars.size()];
    for(int k=0; k<2*supportVars.size(); ++k){
      partitions[k] = 0;
    }
    if(found){
      std::cout << "PO " << Abc_ObjName(po) << " support partition: 1\n";
      int* confClauses;
      int  nConfClauses;
      nConfClauses = sat_solver_final(currSat, &confClauses);
      for(int k=0; k<nConfClauses; ++k){
        assert(confClauses[k] % 2 == 0);
        partitions[Abc_Lit2Var(confClauses[k]) - 3 * varShift] = 1;
      }
      for(int k=0; k<supportVars.size(); ++k){
        if(partitions[k] && partitions[k+supportVars.size()]){
          std::cout << "0";
        }
        else if(!partitions[k] && partitions[k+supportVars.size()]){
          std::cout << "1";
        }
        else if(partitions[k] && !partitions[k+supportVars.size()]){
          std::cout << "2";
        }
        else{
          std::cout << "0";
        }
      }
      std::cout << "\n";
    }
    else{
      std::cout << "PO " << Abc_ObjName(po) << " support partition: 0\n";
    }
    delete partitions;
  }
}
int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_OrBidec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        prints the partition of support variables if the function is or bidecomposable\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}