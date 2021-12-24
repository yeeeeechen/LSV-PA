#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include "aig/aig/aig.h"

#include <vector>
#include <algorithm>
#include <cassert>
#include <climits>
#define DEBUG 0

enum Set{
  XC,
  XB,
  XA,
  XAB
};

extern "C" Aig_Man_t *  Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

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

bool mycompare(Abc_Obj_t*& a, Abc_Obj_t*& b){
  return Abc_ObjId(a) < Abc_ObjId(b);
} 

bool mycompare2(std::vector<Abc_Obj_t*>& a, std::vector<Abc_Obj_t*>& b){
  assert(!a.empty());
  assert(!b.empty());
  return Abc_ObjId(a[0]) < Abc_ObjId(b[0]);
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  // get reverse dfs
  Vec_Ptr_t* DfsList = Abc_NtkDfsReverse(pNtk);

  // get the number of internal nodes
  int numNodes = DfsList->nSize;
  
  // count the number of nodes
  if(DEBUG)
    printf("#nodes: %d\n",numNodes);

  // print the reverse dfs result and their #fanouts
  if(DEBUG){
    printf("-----dfs----\n");
    for(int i = 0; i < numNodes; i++){
      Abc_Obj_t* pNode = (Abc_Obj_t *) DfsList->pArray[i];
      printf("Node: %d  #Fanouts: %d\n", Abc_ObjId(pNode), Abc_ObjFanoutNum(pNode));
    }
    printf("------------\n");
  }

  // change all the internal nodes' iTemp(visited or not) to 0
  for(int i = 0; i < numNodes; i++){
    Abc_Obj_t* pNode = (Abc_Obj_t *) DfsList->pArray[i];
    pNode->iTemp = 0;
  }

  Abc_Obj_t* const1Node = Abc_AigConst1(pNtk);
  const1Node->iTemp = 0;

  // change all the pi nodes' iTemp to 1
  for(int i = 0; i < Abc_NtkPiNum(pNtk); i++){
    Abc_Obj_t* pNode = Abc_NtkPi(pNtk, i);
    pNode->iTemp = 1;
  }


  // use dfs to find msfc
  std::vector<Abc_Obj_t*> S;
  std::vector<std::vector<Abc_Obj_t*> > res;
  for(int i = 0; i < numNodes; i++){
  
    Abc_Obj_t* pNode = (Abc_Obj_t *) DfsList->pArray[i];
    if(pNode->iTemp == 1) continue;

    std::vector<Abc_Obj_t*> group;
    S.push_back(pNode);
    while(!S.empty()){
      Abc_Obj_t* curNode = S.back();
      S.pop_back();
      if(curNode == pNode){
        group.push_back(curNode);
        Abc_Obj_t* child0Node = Abc_ObjFanin0(curNode);
        Abc_Obj_t* child1Node = Abc_ObjFanin1(curNode);
        if(child0Node->iTemp == 0) S.push_back(child0Node);
        if(child1Node->iTemp == 0) S.push_back(child1Node);
        curNode->iTemp = 1;
      }
      else if(Abc_ObjFanoutNum(curNode) == 1){
        group.push_back(curNode);
        if(!Abc_AigNodeIsConst(curNode)){
          Abc_Obj_t* child0Node = Abc_ObjFanin0(curNode);
          Abc_Obj_t* child1Node = Abc_ObjFanin1(curNode);
          if(child0Node->iTemp == 0) S.push_back(child0Node);
          if(child1Node->iTemp == 0) S.push_back(child1Node);
        }
        curNode->iTemp = 1;
      }
    }
    res.push_back(group);
  }

  // process const 1 node
  if(const1Node->iTemp == 0 && Abc_ObjFanoutNum(const1Node) > 0){
    std::vector<Abc_Obj_t*> group;
    group.push_back(const1Node);
    res.push_back(group);
  }

  // sort the id
  for(int i = 0; i < res.size(); ++i){
    sort(res[i].begin(), res[i].end(), mycompare);
  }

  sort(res.begin(), res.end(), mycompare2);

  // print the msfc result
  for(int i = 0; i < res.size(); ++i){
    printf("MSFC %d: ", i);
    for(int j = 0; j < res[i].size(); ++j){
      printf("%s", Abc_ObjName(res[i][j]));
      if(j != res[i].size()-1) printf(",");
    }
    printf("\n");
  }
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
  if(!Abc_NtkIsStrash(pNtk)){
    Abc_Print(-1, "Current network is not an AIG.\n");
    return 1;
  }
  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the MSFC in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
  
}

int my_sat_solver_add_buffer_enable( sat_solver * pSat, int iVarA, int iVarB, int iVarEn, int fCompl )
{
    lit Lits[3];
    int Cid;
    assert( iVarA >= 0 && iVarB >= 0 && iVarEn >= 0 );

    Lits[0] = toLitCond( iVarA, 0 );
    Lits[1] = toLitCond( iVarB, !fCompl );
    Lits[2] = toLitCond( iVarEn, 0 );
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );

    Lits[0] = toLitCond( iVarA, 1 );
    Lits[1] = toLitCond( iVarB, fCompl );
    Lits[2] = toLitCond( iVarEn, 0 );
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );
    return 2;
}

int Lsv_NtkOrBidec(Abc_Ntk_t* pNtk){

  Abc_Obj_t* pPo = NULL;
  int iPo = -1;
  Abc_NtkForEachPo(pNtk, pPo, iPo){
    Abc_Ntk_t* pSubNtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 0);
    if ( Abc_ObjFaninC0(pPo) )
      Abc_ObjXorFaninC( Abc_NtkPo(pSubNtk, 0), 0 );

    Aig_Man_t* pAig = Abc_NtkToDar(pSubNtk, 0, 0);

    Cnf_Dat_t* pCnf = Cnf_Derive(pAig, 1);

    Aig_Obj_t* pObj = NULL;
    int iObj = -1;

    if(DEBUG){
      // Print the mapping between Aig Obj id and its variable
      printf("----Variable mapping----\n");
      printf("#Variables: %d \n", pCnf->nVars);
      Aig_ManForEachCi(pAig, pObj, iObj){
        printf("CI id:%d Variable:%d\n", Aig_ObjId(pObj), pCnf->pVarNums[Aig_ObjId(pObj)]);
      }
      
      Aig_ManForEachCo(pAig, pObj, iObj){
        printf("Output id:%d Varaible:%d\n", Aig_ObjId(pObj), pCnf->pVarNums[Aig_ObjId(pObj)]);
      }

      // Print Cnf
      printf("----CNF----\n");
      printf("#Clauses: %d \n", pCnf->nClauses);
      
      for(int i = 0; i < pCnf->nClauses; ++i){
        for(int* pLit = pCnf->pClauses[i]; pLit !=  pCnf->pClauses[i+1]; pLit++){
          printf("%s%d ", Abc_LitIsCompl(*pLit) ? "-":"", Abc_Lit2Var(*pLit) );
        }
        printf("\n");
      }
    }

    // initialize sat solver
    sat_solver* solver = sat_solver_new();
    if(DEBUG){
      solver->fPrintClause = 1;
      printf("----Added Clauses----\n");
    }
    sat_solver_setnvars(solver, 3*pCnf->nVars);

    // write circuit X cnf into solver
    for(int i = 0; i < pCnf->nClauses; ++i)
      sat_solver_addclause(solver, pCnf->pClauses[i], pCnf->pClauses[i+1]);

    // write circuit X' cnf into solver
    Cnf_DataLift(pCnf, pCnf->nVars);
    for(int i = 0; i < pCnf->nClauses; ++i)
      sat_solver_addclause(solver, pCnf->pClauses[i], pCnf->pClauses[i+1]);

    // write circuit X'' cnf into solver
    Cnf_DataLift(pCnf, pCnf->nVars);
    for(int i = 0; i < pCnf->nClauses; ++i)
      sat_solver_addclause(solver, pCnf->pClauses[i], pCnf->pClauses[i+1]);

    Cnf_DataLift(pCnf, (-2) * pCnf->nVars);

    // f(X), not f(X'), not f(X'')   
    assert(Aig_ManCoNum(pAig) == 1);

    Aig_ManForEachCo(pAig , pObj, iObj ){
      int var = pCnf->pVarNums[Aig_ObjId(pObj)];
      lit lits[1];
      lits[0] = var*2;
      sat_solver_addclause(solver, lits, lits+1);
      lits[0] = (var + pCnf->nVars) * 2 + 1;
      sat_solver_addclause(solver, lits, lits+1);
      lits[0] = (var + 2 * pCnf->nVars) * 2 + 1;
      sat_solver_addclause(solver, lits, lits+1);
    }

    // add control variables
    std::vector<int> ctrAVars;
    std::vector<int> ctrBVars;
    for(int i = 0; i < Aig_ManCiNum(pAig); i++){
      ctrAVars.push_back(sat_solver_addvar(solver));
    }
    for(int i = 0; i < Aig_ManCiNum(pAig); i++){
      ctrBVars.push_back(sat_solver_addvar(solver));
    }
    
    for(int i = 0; i < Aig_ManCiNum(pAig); i++){
      int var = pCnf->pVarNums[Aig_ObjId(Aig_ManCi(pAig, i))];
      my_sat_solver_add_buffer_enable(solver, var, var + pCnf->nVars, ctrAVars[i], 0);
      my_sat_solver_add_buffer_enable(solver, var, var + 2 * pCnf->nVars, ctrBVars[i], 0);
    }

    std::vector<enum Set> bestParti;
    int bestXC = INT_MAX;

    // enumerate partition seed
    for(int ctrA = 0; ctrA < Aig_ManCiNum(pAig) -1; ctrA++){
      for(int ctrB = ctrA+1 ; ctrB < Aig_ManCiNum(pAig); ctrB++){
        // unit assumption
        lit assump[2*Aig_ManCiNum(pAig)];
        for(int i = 0; i < Aig_ManCiNum(pAig); i++){
          assump[2*i] = (i == ctrA)? 2*ctrAVars[i] : 2*ctrAVars[i]+1;
          assump[2*i+1] = (i == ctrB)? 2*ctrBVars[i] : 2*ctrBVars[i]+1;
        }
      
        // solve
        int result = sat_solver_solve(solver, assump, assump + 2*Aig_ManCiNum(pAig), 0, 0, 0, 0);
        if(result == 1) continue;
        else if(result != -1) assert(0);
        
        // find partition
        int* conf_cls = NULL;
        int conf_cls_num = sat_solver_final(solver, &conf_cls);

        //********* TODO: minimize conflict clause *********


        bool partition[2][Aig_ManCiNum(pAig)];
        for(int i = 0; i < Aig_ManCiNum(pAig); i++){
          partition[0][i] = partition[1][i] = 1;
        }
        for(int i = 0; i < conf_cls_num; i++){
          int var = conf_cls[i]/2 - 3*pCnf->nVars;
          if(var >= Aig_ManCiNum(pAig)) partition[1][var-Aig_ManCiNum(pAig)] = 0;
          else partition[0][var] = 0;
        }
        
        std::vector<enum Set> vParti;
        int numXA = 0, numXB = 0, numXC = 0, numXAB = 0;
        for(int i = 0; i < Aig_ManCiNum(pAig); i++){
          if(partition[0][i] == 0 && partition[1][i] == 0){
            vParti.push_back(XC);
            numXC++;
          }
          else if(partition[0][i] == 1 && partition[1][i] == 0){
            vParti.push_back(XA);
            numXA++;
          }
          else if(partition[0][i] == 0 && partition[1][i] == 1){
            vParti.push_back(XB);
            numXB++;
          }
          else{
            vParti.push_back(XAB);
            numXAB++;
          }
        }

        if(DEBUG){
          printf("----Partition----\n");
          for(int i = 0; i < Aig_ManCiNum(pAig); i++){
            if(vParti[i] == XC) printf("%d: XC\n", i);
            else if(vParti[i] == XA) printf("%d: XA\n", i);
            else if(vParti[i] == XB) printf("%d: XB\n", i);
            else printf("%d: XAB\n", i);
          }
        }

        // update best partition
        // if find minimal, break
        if(numXC == bestXC){
          ctrA = Aig_ManCiNum(pAig);
          ctrB = Aig_ManCiNum(pAig);
        }
        else if(numXC < bestXC){
          bestXC = numXC;
          bestParti = vParti;
          if(bestXC == 0){
            ctrA = Aig_ManCiNum(pAig);
            ctrB = Aig_ManCiNum(pAig);
          }
        }
      }
    }

    if(DEBUG){
      printf("----Best Partition----\n");
          for(int i = 0; i < Aig_ManCiNum(pAig); i++){
            if(bestParti[i] == XC) printf("%d: XC\n", i);
            else if(bestParti[i] == XA) printf("%d: XA\n", i);
            else if(bestParti[i] == XB) printf("%d: XB\n", i);
            else printf("%d: XAB\n", i);
      }
    }
    if(bestXC != INT_MAX){
      printf("PO %s support partition: 1\n", Abc_ObjName(pPo));
      for(int i = 0; i < Aig_ManCiNum(pAig); i++){
        printf("%d", bestParti[i]);
      }
      printf("\n");
    }
    else printf("PO %s support partition: 0\n", Abc_ObjName(pPo));
  }
  return 0;
}

int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  if(!Abc_NtkIsStrash(pNtk)){
    Abc_Print(-1, "Current network is not an AIG.\n");
    return 1;
  }

  Lsv_NtkOrBidec(pNtk);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        prints or bidecomposition result\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
  
}
