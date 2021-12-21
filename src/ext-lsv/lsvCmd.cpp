#include "base/abc/abc.h"
#include "sat/cnf/cnf.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <string>

using namespace std;

extern Aig_Man_t *  Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

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
      case "h" :
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
  Abc_Obj_t* pObj;
  Abc_Ntk_t* pNtkCone;
  Aig_Man_t* pAigCone;
  Cnf_Dat_t* pFcnf;

  int i;
  Abc_NtkForEachPo(pNtk, pObj, i) {

    sat_solver* pSat;
    int Lits[3];

    pNtkCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 1);
    if (Abc_ObjFaninC0(pObj))// restore info of complementary edge
      Abc_ObjXorFaninC(Abc_NtkPo(pNtkCone, 0), 0);

    // 2. Abc_NtkToDar() to construct Aig_Man_t from Abc_Ntk_t
    pAigCone = Abc_NtkToDar(pNtkCone, 0, 0);
    pFcnf = Cnf_Derive(pAigCone, Aig_ManCoNum(pAigCone));

    // 3. Construct CNF and send it to SAT_Solver
    pSat = sat_solver_new();
    //    add F(X), F(X'), F(X'')
    sat_solver_setnvars(pSat, pSat->nVars * 3);
    for (int k=0; k<3; ++k) {
      if (k>0)
        Cnf_DataList(pFcnf, pFcnf->nVars);
      for (int i=0; i<pFcnf->nClauses; ++i) {
        if ( !sat_solver_addclause( pSat, pFcnf->pClauses[i], pFcnf->pClauses[i+1] ) )
          {
              Cnf_DataFree( pFcnf );
              sat_solver_delete( pSat );
              return;
          }
      }
    }
    //    add alpha, beta variables
    for (int i=0; i<pFcnf->nVars; ++i) {
      sat_solver_addvar(pSat);
      sat_solver_addvar(pSat);
    }

    // 4. Solve a non-trival solution
    

    // end
    Cnf_DataFree(pCnfOn);
    sat_solver_delete(pSat);
  }
}