#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include "sat/cnf/cnf.h"
#include <vector>
#include <algorithm>
#include <iostream>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBiDecom(Abc_Frame_t* pAbc, int argc, char** argv);

extern "C" struct Cnf_Dat_t_;
typedef struct Cnf_Dat_t_ Cnf_Dat_t;

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
extern "C" Cnf_Dat_t * Cnf_Derive( Aig_Man_t * pAig, int nOutputs );
extern "C" void *      Cnf_DataWriteIntoSolver( Cnf_Dat_t * p, int nFrames, int fInit );
extern void            Cnf_DataLift( Cnf_Dat_t * p, int nVarsPlus );

static inline int sat_solver_xnor_enable( sat_solver * pSat, int iVarA, int iVarB, int iVarEn)
{
    lit Lits[3];
    int Cid;
    assert( iVarA >= 0 && iVarB >= 0 && iVarEn >= 0 );

    Lits[0] = toLitCond( iVarA, 0 );
    Lits[1] = toLitCond( iVarB, 1 );
    Lits[2] = toLitCond( iVarEn, 0 );
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );

    Lits[0] = toLitCond( iVarA, 1 );
    Lits[1] = toLitCond( iVarB, 0 );
    Lits[2] = toLitCond( iVarEn, 0 );
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );
    return 2;
}

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBiDecom, 0);
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

//////////////////////////////////////////////////////////////////////
// my code
//////////////////////////////////////////////////////////////////////

Abc_Obj_t * find_daminating_node(Abc_Obj_t *pObj)
{
  if (Abc_ObjFanoutNum(pObj) == 1)
  {
    if (Abc_ObjIsPo(Abc_ObjFanout(pObj, 0)))
    {
      // printf("find_daminating_node reaches PO.\n");
      return pObj;
    }
    else
    {
      return find_daminating_node(Abc_ObjFanout(pObj, 0));
    }
  }
  else
  {
    return pObj;
  }
}

void find_MSFC(Abc_Obj_t *node, std::vector<Abc_Obj_t *>& MSFC)
{
  if (node->fMarkA == false)
  {
    int j = 0;
    Abc_Obj_t *pFanin;
    node->fMarkA = true;
    MSFC.push_back(node);
    Abc_ObjForEachFanin(node, pFanin, j)
    {
      if (Abc_ObjIsPi(pFanin) == false && Abc_ObjFanoutNum(pFanin) == 1)
      {
        find_MSFC(pFanin, MSFC);
      }
    }
  }
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;

  std::vector<std::vector<Abc_Obj_t *>> MSFCs;
  Abc_NtkCleanMarkA(pNtk);

  Abc_NtkForEachObj(pNtk, pObj, i) {
    if (!Abc_ObjIsNode(pObj) && pObj->Type != ABC_OBJ_CONST1)
    {
      continue;
    }

    if (Abc_ObjFanoutNum(pObj) == 0)
    {
      continue;
    }

    if (pObj->fMarkA == true)
    {
      continue;
    }

    // find the daminating node (node with multi-fanout)
    Abc_Obj_t* daminating_node = find_daminating_node(pObj);
    // find all of the daminated nodes (nodes that have single-fanout and has a single-fanout node path to the daminating node)
    std::vector<Abc_Obj_t*> MSFC;
    find_MSFC(daminating_node, MSFC);
    MSFCs.push_back(MSFC);
  }

  for (int i = 0; i < MSFCs.size(); i++)
  {
    std::sort(MSFCs[i].begin(), MSFCs[i].end(), 
    [](const Abc_Obj_t *a, const Abc_Obj_t *b)
    {
      return a->Id < b->Id;
    });
  }
  std::sort(MSFCs.begin(), MSFCs.end(), 
  [](const std::vector<Abc_Obj_t *> &a, const std::vector<Abc_Obj_t *> &b)
  {
    return a[0]->Id < b[0]->Id;
  });

  for (int i = 0; i < MSFCs.size(); i++)
  {
    printf("MSFC %d: ", i);
    for (int j = 0; j < MSFCs[i].size(); j++)
    {
      if (j == 0)
      {
        printf("%s", Abc_ObjName(MSFCs[i][j]));
      }
      else
      {
        printf(",%s", Abc_ObjName(MSFCs[i][j]));
      }
    }
    printf("\n");
  }

  Abc_NtkCleanMarkA(pNtk);
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

  assert(Abc_NtkIsStrash(pNtk));

  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        maximum single-fanout cones (MSFCs) that covers all nodes (excluding PIs and POs) of a given AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

//////////////////////////////////////////////////////////////////////
// PA2
//////////////////////////////////////////////////////////////////////

void Lsv_NtkOrBiDecom(Abc_Ntk_t* pNtk) {
//////////////////////////////////////////////////////////////////////
// ntk ID -> ntk PO cone ID -> aig ID -> cnf ID -> sat ID?
//////////////////////////////////////////////////////////////////////

  Abc_Obj_t* pPo;
  Abc_Ntk_t* pNtkPoCone;
  Aig_Man_t* pAig;
  Aig_Obj_t* pObj;
  Cnf_Dat_t* pCnf;
  sat_solver* pSat;
  int i, j, nPi;
  int* pBeg, * pEnd;

  Abc_NtkForEachPo(pNtk, pPo, i) {
    pNtkPoCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 0);
    pAig = Abc_NtkToDar(pNtkPoCone, 0, 1);
    nPi = Aig_ManCiNum(pAig);
    pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));

    int* cnf_PI_IDs = new int[nPi];
    int cnf_PO_ID = 0;

    // aig ID -> cnf ID
    Aig_ManForEachCi(pCnf->pMan, pObj, j) {
      cnf_PI_IDs[j] = pCnf->pVarNums[Aig_ObjId(pObj)];
    }
    cnf_PO_ID = pCnf->pVarNums[Aig_ObjId((Aig_Obj_t *)Vec_PtrEntry(pCnf->pMan->vCos, 0))];

    int varshift = pCnf->nVars;

    // f(x)
    pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    lit Lits[1];
    Lits[0] = toLitCond( cnf_PO_ID, 0 );
    sat_solver_addclause( pSat, Lits, Lits + 1 );

    // f(x')
    Cnf_DataLift(pCnf, varshift);
    sat_solver_setnvars( pSat, sat_solver_nvars(pSat) + varshift );
    Cnf_CnfForClause( pCnf, pBeg, pEnd, j ) {
      sat_solver_addclause( pSat, pBeg, pEnd );
    }
    Lits[0] = toLitCond( cnf_PO_ID + varshift, 1 );
    sat_solver_addclause( pSat, Lits, Lits + 1 );

    // f(x'')
    Cnf_DataLift(pCnf, varshift);
    sat_solver_setnvars( pSat, sat_solver_nvars(pSat) + varshift );
    Cnf_CnfForClause( pCnf, pBeg, pEnd, j ) {
      sat_solver_addclause( pSat, pBeg, pEnd );
    }
    Lits[0] = toLitCond( cnf_PO_ID + 2 * varshift, 1 );
    sat_solver_addclause( pSat, Lits, Lits + 1 );

    // incremental SAT solving
    // add alphas and betas, total nPi for each
    sat_solver_setnvars( pSat, sat_solver_nvars(pSat) + nPi * 2 );
    for (int p = 0; p < nPi; p++) {
      // alpha : f(x)=f(x')&alpha, if alpha=j, it's 1
      sat_solver_xnor_enable(pSat, cnf_PI_IDs[p], cnf_PI_IDs[p] + varshift, p + varshift * 3);
      // beta : f(x)=f(x'')&beta, if beta=k, it's 1
      sat_solver_xnor_enable(pSat, cnf_PI_IDs[p], cnf_PI_IDs[p] + varshift * 2, p + varshift * 3 + nPi);
    }

    // total varnum = pCnf->nVars/varshift * 3 + nPi * 2

    // make assumptionlist
    // alpha : f(x)=f(x')&alpha, if alpha=j, it's 1
    // beta : f(x)=f(x'')&beta, if beta=k, it's 1
    lit* assumption = new lit[nPi * 2];
    int assumption_counter = 1;
    bool done = 0;
    bool* hit_list = new bool[nPi * 2];
    for (int p = 0; p < nPi * 2; p++) {
      hit_list[p] = 0;
    }
    
    for (int p = 0; p < nPi && !done; p++) {
      for (int q = 0; q < nPi && !done; q++) {
        if (p < q) {
          for (int r = 0; r < nPi; r++) {
            if (r == p) {
              assumption[r] = toLitCond( varshift * 3 + r, 0 );
            } else {
              assumption[r] = toLitCond( varshift * 3 + r, 1 );
            }
          }
          for (int r = nPi; r < nPi * 2; r++) {
            if (r == (q + nPi)) {
              assumption[r] = toLitCond( varshift * 3 + r, 0 );
            } else {
              assumption[r] = toLitCond( varshift * 3 + r, 1 );
            }
          }

          int status = sat_solver_solve(pSat, assumption, assumption + nPi * 2, 0, 0, 0, 0);
          if (status == l_False) {
            // unsat
            int nFinal, * pFinal;
            nFinal = sat_solver_final(pSat, &pFinal);
            for (int r = 0; r < nFinal; r++) {
              hit_list[Abc_Lit2Var(pFinal[r]) - varshift * 3] = 1;
            }
            done = 1;
            break;
          }
          assumption_counter++;
        }
      }
    }

    std::cout << "PO " << Abc_ObjName(pPo) << " support partition: " << done << std::endl;
    if (done) {
      for (int p = 0; p < nPi; p++) {
        if (hit_list[p] == 1 && hit_list[p + nPi] == 1) {
          std::cout << "0";
        } else if (hit_list[p] == 0 && hit_list[p + nPi] == 1) {
          std::cout << "1";
        } else if (hit_list[p] == 1 && hit_list[p + nPi] == 0) {
          std::cout << "2";
        } else {
          std::cout << "*error at PI #" << p << "*";
        }
      }
      std::cout << std::endl;
    }

    delete [] assumption;
    delete [] cnf_PI_IDs;
    delete [] hit_list;
    Abc_NtkDelete(pNtkPoCone);
  }
}

int Lsv_CommandOrBiDecom(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  assert(Abc_NtkIsStrash(pNtk));

  Lsv_NtkOrBiDecom(pNtk);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        decides whether each circuit PO f(X) is OR bi-decomposable\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}