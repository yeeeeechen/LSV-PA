#include "base/abc/abc.h"
#include "sat/cnf/cnf.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "misc/util/abc_global.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <iostream>

using namespace std;

extern "C" {
  Aig_Man_t *  Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

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


// =========  LSV "lsv_print_nodes" example  ============

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


// ========== LSV "lsv_print_msfc"  ============

struct compareObjIDs {
  bool operator()(Abc_Obj_t *pObj1, Abc_Obj_t *pObj2) const { return Abc_ObjId(pObj1) < Abc_ObjId(pObj2); }
};
struct comaprePairs {
  bool operator()(vector<Abc_Obj_t *> &vec1, vector<Abc_Obj_t *> &vec2) const { return Abc_ObjId(vec1[0]) < Abc_ObjId(vec2[0]); }
};


/**Function*************************************************************
   Description 
   [ for given node v and a MSFC set C(v), recursively find its fanin u, if it belongs to Obj or const1 object in ABC and following conditions are satisfied
    1: u = v
      or
    2: |FO(u)|=1 and FO(u) belongs C(v) 
    then add u to set C ]
***********************************************************************/
void msfcTraversal(Abc_Obj_t* pObj, vector<Abc_Obj_t *> &msfcList, unordered_set<Abc_Obj_t *> &cover) {
  Abc_Obj_t *pFanin;
  int i;
  msfcList.push_back(pObj);
  cover.insert(pObj);
  Abc_ObjForEachFanin(pObj, pFanin, i) {    // for each fanin
    if ((pFanin->Type == ABC_OBJ_NODE) || (pFanin->Type == ABC_OBJ_CONST1)) {
      if (Abc_ObjFanoutNum(pFanin) == 1) {    // it has only one fanout
        // if it has only one fanout, u can only be discovered by v
        msfcTraversal(pFanin, msfcList, cover);
      }
    }
  }
}

int Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  // TODO
  Abc_Obj_t* pObj;
  Abc_Obj_t* const1 = Abc_AigConst1(pNtk);
  int i;
  vector<Abc_Obj_t *> nodeList;
  unordered_set<Abc_Obj_t *> cover;
  vector<vector<Abc_Obj_t *>> msfcs; 
  string line;
  if (Abc_ObjFanoutNum(const1) >= 1) nodeList.push_back(const1);
  Abc_NtkForEachNode(pNtk, pObj, i) {
    nodeList.push_back(pObj);
  }
  sort(nodeList.begin(), nodeList.end(), compareObjIDs());

  // reversed topological order, if a node is traversed, add it into cover
  // it can not belong to other node's msfc
  for (vector<Abc_Obj_t *>::reverse_iterator it=nodeList.rbegin(); it!=nodeList.rend(); ++it) { 
    if (cover.find(*it) != cover.end())
      continue;
    vector<Abc_Obj_t *> msfcList;
    msfcTraversal(*it, msfcList, cover);
    sort(msfcList.begin(), msfcList.end(), compareObjIDs());
    assert(msfcList.size());
    
    msfcs.push_back(msfcList);
  }
  sort(msfcs.begin(), msfcs.end(), comaprePairs());

  for (int idx=0; idx<msfcs.size(); ++idx) {
    line = ("MSFC " + to_string(idx) + ": ");
    for (int j=0; j<msfcs[idx].size(); ++j) {
      if (j==0) line += string(Abc_ObjName(msfcs[idx][j]));
      else line += ("," + string(Abc_ObjName(msfcs[idx][j])));
    }
    line += "\n";
    printf("%s", line.c_str());
  }
  return 0;
}

int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  assert(Abc_NtkIsStrash(pNtk));
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
  Abc_Print(-2, "\t        prints maximum single-fanout cones (MSFCs) that covers all nodes (excluding PIs and POs) of a given AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}


// ========= LSV or_bidec ============
static int Lsv_Or_Bidec(Abc_Ntk_t*);
static int Lsv_Po_Or_Bidec(Abc_Ntk_t);

int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h' :
        goto usage;
      default:
        goto usage;
    }
  }
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_Or_Bidec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        Decides whether each circuit POf(X)is OR bi-decomposabl\n          Format: 0,x belongs Xc;  1, Xb;  2, Xc\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_Or_Bidec(Abc_Ntk_t* pNtk) {
  // 1. For each PO, apply Abc_NtkCreateCone() to get a (a. cone) and (b. support set)
  static Abc_Obj_t* pNtkCo;
  static Aig_Obj_t* pAigCi, * pAigCo;
  static Abc_Ntk_t* pNtkCone;
  static Aig_Man_t* pAigCone;
  static Cnf_Dat_t* pFcnf;
  int Lits[3], status;

  int poId, piId;
  Abc_NtkForEachCo(pNtk, pNtkCo, poId) {

    sat_solver* pSat;

    pNtkCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pNtkCo), Abc_ObjName(pNtkCo), 0);
    if (Abc_ObjFaninC0(pNtkCo))// restore info of complementary edge
      Abc_ObjXorFaninC(Abc_NtkPo(pNtkCone, 0), 0);

    // 2. Abc_NtkToDar() to construct Aig_Man_t from Abc_Ntk_t
    pAigCone = Abc_NtkToDar(pNtkCone, 0, 1);
    pFcnf = Cnf_Derive(pAigCone, Aig_ManCoNum(pAigCone));

    // 3. Construct CNF and send it to SAT_Solver
    //    add formula of F, with positive output phase
    pSat = (sat_solver *)Cnf_DataWriteIntoSolver( pFcnf, 1, 0 );
    pAigCo = Aig_ManCo(pAigCone, 0); 
    Lits[0] = toLitCond(pFcnf->pVarNums[pAigCo->Id], Aig_IsComplement(pAigCo));
    sat_solver_addclause(pSat, Lits, Lits+1);


    //    add formula of F(X'), F(X''), as well as negative phase of each output
    for (int k=1; k<3; ++k) {
      Cnf_DataLift(pFcnf, pFcnf->nVars);
      Lits[0] = toLitCond(pFcnf->pVarNums[pAigCo->Id], (Aig_IsComplement(pAigCo)!=1));
      sat_solver_addclause(pSat, Lits, Lits+1);
      for (int i=0; i<pFcnf->nClauses; ++i) {
        sat_solver_addclause( pSat, pFcnf->pClauses[i], pFcnf->pClauses[i+1] );
      }
    }

    unordered_map<int, bool> ctrlAConflict;
    vector<int> ctrlA;
    unordered_map<int, bool> ctrlBConflict;
    vector<int> ctrlB;

    Aig_ManForEachCi(pAigCone, pAigCi, piId) {
      assert(Aig_ObjType(pAigCi) == AIG_OBJ_CI);
      int a = sat_solver_addvar(pSat);
      int b = sat_solver_addvar(pSat);
      ctrlA.push_back(a);
      ctrlB.push_back(b);
      ctrlAConflict[a] = false;
      ctrlBConflict[b] = false;
    }

    //    add clauses describing controlling variables, now all pFcnf are X''
    Aig_ManForEachCi(pAigCone, pAigCi, piId) {
      // cout << "(xi_prime2_list[i], VarShift, control_a[i]) = (" << pFcnf->pVarNums[pAigCi->Id] << ", " << pFcnf->nVars << ", " << ctrlA[piId] << ")\n";

      // ((X' equal X) or alpha)
      Lits[0] = toLitCond(pFcnf->pVarNums[pAigCi->Id] - pFcnf->nVars, 0);
      Lits[1] = toLitCond(pFcnf->pVarNums[pAigCi->Id] - pFcnf->nVars * 2, 1);
      Lits[2] = toLitCond(ctrlA[piId], 0);
      sat_solver_addclause(pSat, Lits, Lits+3);
      Lits[0] = toLitCond(pFcnf->pVarNums[pAigCi->Id] - pFcnf->nVars, 1);
      Lits[1] = toLitCond(pFcnf->pVarNums[pAigCi->Id] - pFcnf->nVars * 2, 0);
      sat_solver_addclause(pSat, Lits, Lits+3);

      // ((X'' equal X), beta)
      Lits[0] = toLitCond(pFcnf->pVarNums[pAigCi->Id], 0);
      Lits[1] = toLitCond(pFcnf->pVarNums[pAigCi->Id] - pFcnf->nVars * 2, 1);
      Lits[2] = toLitCond(ctrlB[piId], 0);
      sat_solver_addclause(pSat, Lits, Lits+3);
      Lits[0] = toLitCond(pFcnf->pVarNums[pAigCi->Id], 1);
      Lits[1] = toLitCond(pFcnf->pVarNums[pAigCi->Id] - pFcnf->nVars * 2, 0);
      sat_solver_addclause(pSat, Lits, Lits+3);      
    }
    status = sat_solver_simplify( pSat );
    assert( status != 0 );
    
    // 4. Solve a non-trival solution using Incremental SAT solving
    bool isExistPartition = false;
    int nAigCi = Aig_ManCiNum(pAigCone);

    if (nAigCi <= 1) 
      cout << "PO " << Abc_ObjName(pNtkCo) << " support partition: " << isExistPartition << endl;
    else {
    //    given fixed input index i, j as seed. 
      for (int i=0; i<nAigCi - 1; ++i) {
        for (int j=i+1; j<nAigCi; ++j) {
          int assumpArray[nAigCi*2];
          for (int k=0; k<nAigCi; ++k) {
            // (0, 1) belongs b
            if (i==k) {
              assumpArray[k] = toLitCond(ctrlA[k], 1);
              assumpArray[k + nAigCi] = toLitCond(ctrlB[k], 0);
            }
            // (1, 0) belongs a
            else if (j == k) {
              assumpArray[k] = toLitCond(ctrlA[k], 0);
              assumpArray[k + nAigCi] = toLitCond(ctrlB[k], 1);
            }
            // (0, 0) not belongs a, b
            else {
              assumpArray[k] = toLitCond(ctrlA[k], 1);
              assumpArray[k + nAigCi] = toLitCond(ctrlB[k], 1);
            }
          }
          status = sat_solver_solve(pSat, assumpArray, assumpArray + nAigCi*2, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);

          // If solver is UNSAT, means there exists a partition
          int nCoreLits;
          int* pCoreLits;
          vector<int> partition, ctrlvars;

          if (status == l_False) {
            isExistPartition = true;
            nCoreLits = sat_solver_final(pSat, &pCoreLits);

            // Analyze the UNSAT core
            // Only consider a, b
            // Every literal in the conflict clause is of positive phase because the conflict arises from a subset of the controlling variables (alpha, beta) set to 0
            for (int k = 0 ; k < nCoreLits ; ++k) {
              if (ctrlAConflict.find(pCoreLits[k]/2) != ctrlAConflict.end())
                ctrlAConflict[pCoreLits[k]/2] = true;
              if (ctrlBConflict.find(pCoreLits[k]/2) != ctrlBConflict.end())
                ctrlBConflict[pCoreLits[k]/2] = true;
            }
            /*
            if (alpha,beta) = (0,0), x_i belongs C
            if (alpha,beta) = (X,0), x_i belongs A, (1 means conflict, since we assume they all being 0)
            if (alpha,beta) = (0,X), x_i belongs B
            if (alpha,beta) = (1,1), x_i belongs either A or B (here we put it in A)

            partition: {0: xC, 1: xB, 2:xA}
            */
            for (int k=0; k<nAigCi; ++k) {
              if (k==i)
                partition.push_back(1);
              else if (k==j)
                partition.push_back(2);
              else if (ctrlAConflict[ctrlA[k]] && ctrlBConflict[ctrlB[k]])
                partition.push_back(0);
              else if (ctrlAConflict[ctrlA[k]] && !ctrlBConflict[ctrlB[k]])
                partition.push_back(1);
              else if (!ctrlAConflict[ctrlA[k]] && ctrlBConflict[ctrlB[k]])
                partition.push_back(2);
              else if (!ctrlAConflict[ctrlA[k]] && !ctrlBConflict[ctrlB[k]])
                partition.push_back(2);
            }
            cout << "PO " << Abc_ObjName(pNtkCo) << " support partition: " << isExistPartition << endl;
            for (int k = 0 ; k < partition.size() ; ++k) { cout << partition[k]; }
            cout << endl;
          }
          if (isExistPartition) break;
        }
        if (isExistPartition) break;
      }
      if (!isExistPartition)
        cout << "PO " << Abc_ObjName(pNtkCo) << " support partition: " << isExistPartition << endl;
    }
    // end
    Cnf_DataFree(pFcnf);
    sat_solver_delete(pSat);
  }
  return 0;
}