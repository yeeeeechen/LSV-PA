#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/gia/giaAig.h"
#include "aig/gia/gia.h"
#include "sat/cnf/cnf.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <queue>
#include <algorithm>
using namespace std;

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

//----------------------------------------LSV Example----------------------------------------------//
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
//----------------------------------------------LSV PA1---------------------------------------------//
void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  vector<Abc_Obj_t*> v_coneFout;
  vector<vector<int> > v_msfc;
  Abc_NtkForEachPo(pNtk, pObj, i){
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      if(!Abc_ObjIsPi(pFanin)){
        if(find(v_coneFout.begin(),v_coneFout.end(),pFanin) == v_coneFout.end()){ // new msf of cone
          v_coneFout.push_back(pFanin);
          //printf(" New MSF of Cone - %d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
        }
      }
    }
  }
  // for each msf, travorsal
  for(i=0;i<v_coneFout.size();++i){
    pObj = v_coneFout[i];
    vector<int> v_node;
    queue<Abc_Obj_t*> q;
    q.push(pObj);
    while(!q.empty()){ // BFS
      pObj = q.front();
      v_node.push_back(Abc_ObjId(pObj)); // put the node to this cone
      Abc_Obj_t* pFanin;
      int j;
      Abc_ObjForEachFanin(pObj, pFanin, j) {
        if(!Abc_ObjIsPi(pFanin)){ // not a PI
          if(pFanin->vFanouts.nSize > 1){ // new msf of cone
            if(find(v_coneFout.begin(),v_coneFout.end(),pFanin)==v_coneFout.end()){
              v_coneFout.push_back(pFanin);
              //printf(" New MSF of Cone - %d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
            }
          }
          else{ // a node of cone
            if(find(v_node.begin(),v_node.end(),Abc_ObjId(pFanin))==v_node.end()){
              q.push(pFanin);
            }
          }
        }
      }
      q.pop();
    }
    v_msfc.push_back(v_node);
  }

  for(i=0;i<v_msfc.size();++i){
  	sort(v_msfc[i].begin(), v_msfc[i].end());
  }
  sort(v_msfc.begin(), v_msfc.end());

  for(i=0;i<v_msfc.size();++i){
    cout << "MSFC " << i << ":";
    cout << " n" << v_msfc[i][0];
    for(int j=1;j<v_msfc[i].size();++j){
      cout << ",n" << v_msfc[i][j];
    }
    cout << "\n";
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
  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the MSFCs in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
//--------------------------------------------LSV PA2----------------------------------------------------//
extern "C"{
  Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
void Lsv_NtkOrBidec(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj; // PO
  int po_i;
  Abc_NtkForEachPo(pNtk, pObj, po_i){
    string s_poName = Abc_ObjName(pObj);
    int supPart = 0; // 1: there is a valid non-trivial partition
    vector<int> v_alphaVar;
    vector<int> v_betaVar;
    vector<int> v_alpha_final;
    vector<int> v_beta_final;

    // Get the cone Cone of the PO
    Abc_Ntk_t* pNtkPoCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0); // PO's Fanin0 is the node of PO, 0 means no need for other PI
    if(Abc_ObjFaninC0(pObj)) Abc_ObjXorFaninC(Abc_NtkPo(pNtkPoCone, 0), 0); // deal with the complemental edge
    int n_PI = Abc_NtkPiNum(pNtkPoCone);
    // Get the AIG of this cone
    Aig_Man_t* pAigPoCone = Abc_NtkToDar(pNtkPoCone, 0, 0); // (ntk, 0, 1)
    // Get the CNF of f(X), f(X'), f(X'')
    Cnf_Dat_t* pCnf_fX = Cnf_Derive(pAigPoCone, 1); // (aig, nOutput)
    Cnf_Dat_t* pCnf_fXp = Cnf_DataDup(pCnf_fX);
    Cnf_DataLift(pCnf_fXp, pCnf_fX->nVars);
    Cnf_Dat_t* pCnf_fXpp = Cnf_DataDup(pCnf_fXp);
    Cnf_DataLift(pCnf_fXpp, pCnf_fXp->nVars);
    // put them into the sat_solver
    sat_solver* pSat = (sat_solver *)Cnf_DataWriteIntoSolver(pCnf_fX, 1, 0); // f(X)
    Cnf_DataWriteIntoSolverInt(pSat, pCnf_fXp, 1, 0);
    Cnf_DataWriteIntoSolverInt(pSat, pCnf_fXpp, 1, 0);
    // add the literal of f(X), -f(X'), -f(X'')
    lit Lits_F[1];
    Aig_Obj_t* pObjPo;
    int i_po_tmp;
    Aig_ManForEachCo(pAigPoCone, pObjPo, i_po_tmp){
      Lits_F[0] = toLitCond(pCnf_fX->pVarNums[pObjPo->Id], 0);; // literal of f(X)
      sat_solver_addclause(pSat, Lits_F, Lits_F+1);
      Lits_F[0] = toLitCond(pCnf_fXp->pVarNums[pObjPo->Id], 1); // literal of -f(X')
      sat_solver_addclause(pSat, Lits_F, Lits_F+1);
      Lits_F[0] = toLitCond(pCnf_fXpp->pVarNums[pObjPo->Id], 1); // literal of -f(X'')
      sat_solver_addclause(pSat, Lits_F, Lits_F+1);
    }
    // create variable alpha_i and beta_i
    Aig_Obj_t* pSup;
    int sup_i;
    Aig_ManForEachCi(pAigPoCone, pSup, sup_i){
      v_alphaVar.push_back(sat_solver_addvar(pSat));
      v_betaVar.push_back(sat_solver_addvar(pSat));
      v_alpha_final.push_back(1);
      v_beta_final.push_back(1);
    }
    // add 4 clauses for each support
    Aig_ManForEachCi(pAigPoCone, pSup, sup_i){
      int xVar = pCnf_fX->pVarNums[pSup->Id];
      int xpVar = pCnf_fXp->pVarNums[pSup->Id]; // actually the xpVar = xVar + pCnf_fX->nVars
      int xppVar = pCnf_fXpp->pVarNums[pSup->Id]; // actually the xppVar = xpVar + pCnf_fXp->nVars
      lit Lits[3];
      Lits[0] = toLitCond(xVar, 1); // -x
      Lits[1] = toLitCond(xpVar, 0); // x'
      Lits[2] = toLitCond(v_alphaVar[sup_i], 0); // alpha
      sat_solver_addclause(pSat, Lits, Lits+3);
      Lits[0] = toLitCond(xVar, 0); // x
      Lits[1] = toLitCond(xpVar, 1); // -x'
      Lits[2] = toLitCond(v_alphaVar[sup_i], 0); // alpha
      sat_solver_addclause(pSat, Lits, Lits+3);
      Lits[0] = toLitCond(xVar, 1); // -x
      Lits[1] = toLitCond(xppVar, 0); // x''
      Lits[2] = toLitCond(v_betaVar[sup_i], 0); // beta
      sat_solver_addclause(pSat, Lits, Lits+3);
      Lits[0] = toLitCond(xVar, 0); // x
      Lits[1] = toLitCond(xppVar, 1); // -x''
      Lits[2] = toLitCond(v_betaVar[sup_i], 0); // beta
      sat_solver_addclause(pSat, Lits, Lits+3);
    }
    // run C(n,2) for xa in XA and xb in XB and xi in XC with sat_solver
    for(int a=0;a<n_PI;++a){
      for(int b=a+1;b<n_PI;++b){
        lit assumpList[n_PI * 2];
        assumpList[0] = toLitCond(v_alphaVar[a], 0); // xa in XA
        assumpList[1] = toLitCond(v_betaVar[a], 1); // 
        assumpList[2] = toLitCond(v_alphaVar[b], 1); // xb in XB
        assumpList[3] = toLitCond(v_betaVar[b], 0); // 
        int count = 4;
        for(int i=0;i<n_PI;++i){
          if(i != a && i != b){
            assumpList[count] = toLitCond(v_alphaVar[i], 1); // xi in XC
            assumpList[count+1] = toLitCond(v_betaVar[i], 1); // 
            count += 2;
          }
        }
        int status = sat_solver_solve(pSat, assumpList, assumpList+n_PI*2, 0, 0, 0, 0);
        if(status == l_False){ // unsat, break and output
          supPart = 1;
          break;
        }
      }
      if(supPart){
        break;
      }
    }
    // get the final conflict clause
    int nFinal, * pFinal;
    nFinal = sat_solver_final(pSat, &pFinal);
    for(int i=0;i<nFinal;i++){
      vector<int>::iterator it;
      it = find(v_alphaVar.begin(),v_alphaVar.end(),Abc_Lit2Var(pFinal[i]));
      if(it != v_alphaVar.end()) v_alpha_final[it-v_alphaVar.begin()] = 0;
      it = find(v_betaVar.begin(),v_betaVar.end(),Abc_Lit2Var(pFinal[i]));
      if(it != v_betaVar.end()) v_beta_final[it-v_betaVar.begin()] = 0;
    }
    cout << "PO " << s_poName << " support partition: " << supPart << "\n";
    if(supPart){
      for(int i=0;i<n_PI;++i){
        if(v_alpha_final[i] == 1) // XA
          cout << 2;
        else if(v_beta_final[i] == 1) // XB
          cout << 1;
        else // XC
          cout << 0;
      }
      cout << "\n";
    }
  }
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
  Lsv_NtkOrBidec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        prints the OrBidec fanouts in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}