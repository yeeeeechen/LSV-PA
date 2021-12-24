#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include "sat/cnf/cnf.h"
#include "misc/util/abc_global.h"
#include <iostream>
#include <vector>
#include <algorithm>

extern "C" {
  Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;


bool int_in_vector(std::vector<int>& v, int i) {
  return std::find(v.begin(), v.end(), i) != v.end();
}


void Lsv_NtkOrBidec(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pNtk_PO;
  int ntk_PO;
  
  Abc_NtkForEachCo(pNtk, pNtk_PO, ntk_PO) {
    sat_solver* pSat;
    
    // Extract the cone of the PO and its support set
    Abc_Ntk_t* pNtk_support;
    pNtk_support = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pNtk_PO), Abc_ObjName(pNtk_PO), 0);
    if (Abc_ObjFaninC0(pNtk_PO)) {
      Abc_ObjXorFaninC(Abc_NtkPo(pNtk_support, 0), 0);
    }
    pNtk_support = Abc_NtkStrash(pNtk_support, 0, 1, 0);

    // Derives an equivalent Aig_Man_t
    Aig_Man_t* pAig = Abc_NtkToDar(pNtk_support, 0, 1);

    // Construct CNF
    Cnf_Dat_t* pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
    pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);

    Aig_Obj_t* pObj;
    int i_Obj, id_PO = 0;
    int VarShift = 0;
    std::vector<int> list_PI;
    Aig_ManForEachObj(pAig, pObj, i_Obj) {
      int v = pCnf->pVarNums[pObj->Id];
      if (v > VarShift) VarShift = v;
      if (Aig_ObjIsCi(pObj)) {
        list_PI.push_back(pObj->Id);
      } else if (Aig_ObjIsCo(pObj)) {
        id_PO = pObj->Id;
      }

    }
    int VarShift2 = 2 * VarShift;

    std::vector<int> list_xi, list_xi_p, list_xi_pp;
    for (int i_PI : list_PI) {
      list_xi.push_back(pCnf->pVarNums[i_PI]);
      list_xi_p.push_back(pCnf->pVarNums[i_PI] + VarShift);
      list_xi_pp.push_back(pCnf->pVarNums[i_PI] + VarShift2);
    }

    // f(X)
    int fX_var = pCnf->pVarNums[id_PO];
    int tmp_lit = Abc_Var2Lit(fX_var, 0);
    int *tmp_f = &tmp_lit;
    sat_solver_addclause(pSat, tmp_f, tmp_f+1);

    // ¬f(X')
    Cnf_DataLift(pCnf, VarShift);
    tmp_lit = Abc_Var2Lit(fX_var + VarShift, 1);
    sat_solver_addclause(pSat, tmp_f, tmp_f+1);
    for (int i = 0; i < pCnf->nClauses; ++i) {
      sat_solver_addclause(pSat, pCnf->pClauses[i], pCnf->pClauses[i+1]);
    }

    // ¬f(X'')
    Cnf_DataLift(pCnf, VarShift);
    tmp_lit = Abc_Var2Lit(fX_var + VarShift2, 1);
    sat_solver_addclause(pSat, tmp_f, tmp_f+1);
    for (int i = 0; i < pCnf->nClauses; ++i) {
      sat_solver_addclause(pSat, pCnf->pClauses[i], pCnf->pClauses[i+1]);
    }

    // control variables
    std::vector<int> ctrl_a, ctrl_b;
    int L_PI = list_PI.size();
    // double
    for (int i = 0; i < 2 * L_PI; ++i) {
      sat_solver_addvar(pSat);
    }
    int VarShift3 = 3 * VarShift;
    int L = VarShift3 + L_PI + 1;
    for (int i = VarShift3 + 1; i < L; ++i) {
      ctrl_a.push_back(i);
    }
    L += L_PI + 1;
    for (int i = VarShift3 + L_PI + 2; i < L; ++i) {
      ctrl_b.push_back(i);
    }

    for (int i = 0; i < L_PI; ++i) {
      int tmp_clause[3];
      tmp_clause[0] = Abc_Var2Lit(list_xi[i], 1);
      tmp_clause[1] = Abc_Var2Lit(list_xi_p[i], 0);
      tmp_clause[2] = Abc_Var2Lit(ctrl_a[i], 0);
      sat_solver_addclause(pSat, tmp_clause, tmp_clause + 3);
      tmp_clause[0] = Abc_Var2Lit(list_xi[i], 0);
      tmp_clause[1] = Abc_Var2Lit(list_xi_p[i], 1);
      tmp_clause[2] = Abc_Var2Lit(ctrl_a[i], 0);
      sat_solver_addclause(pSat, tmp_clause, tmp_clause + 3);
      tmp_clause[0] = Abc_Var2Lit(list_xi[i], 1);
      tmp_clause[1] = Abc_Var2Lit(list_xi_pp[i], 0);
      tmp_clause[2] = Abc_Var2Lit(ctrl_b[i], 0);
      sat_solver_addclause(pSat, tmp_clause, tmp_clause + 3);
      tmp_clause[0] = Abc_Var2Lit(list_xi[i], 0);
      tmp_clause[1] = Abc_Var2Lit(list_xi_pp[i], 1);
      tmp_clause[2] = Abc_Var2Lit(ctrl_b[i], 0);
      sat_solver_addclause(pSat, tmp_clause, tmp_clause + 3);
    }

    // Solve a non-trivial variable partition
    bool found_partition = false;
    L = list_xi.size();
    if (L > 1) {
      std::vector<int> ans;

      for (int i = 0; i < L-1; ++i) {
        for (int j = i+1; j < L; ++j) {
          found_partition = false;
          ans.clear();
          std::vector<int> list_assumption, candidate;

          for (int k = 0; k < L; ++k) {
            if (k == i) {
              list_assumption.push_back(toLitCond(ctrl_a[k], 1));
              list_assumption.push_back(toLitCond(ctrl_b[k], 0));
            } else if (k == j) {
              list_assumption.push_back(toLitCond(ctrl_a[k], 0));
              list_assumption.push_back(toLitCond(ctrl_b[k], 1));
            } else {
              list_assumption.push_back(toLitCond(ctrl_a[k], 1));
              list_assumption.push_back(toLitCond(ctrl_b[k], 1));
            }
          }

          int status;
          status = sat_solver_solve(pSat, list_assumption.data(), list_assumption.data() + list_assumption.size(), 0, 0, 0, 0);
          
          // only consider status == l_False
          if (status != l_False) continue;
          found_partition  = true;

          int nFinal, *pFinal;
          nFinal = sat_solver_final(pSat, &pFinal);
          for (int k = 0; k < nFinal; ++k) {
            int var = int(pFinal[k]/2);
            if (int_in_vector(ctrl_a, var) || int_in_vector(ctrl_b, var)) {
              candidate.push_back(var);
            }
          }

          for (int k = 0; k < L; ++k) {
            if (k == i) { ans.push_back(1); continue; }
            if (k == j) { ans.push_back(2); continue; }

            bool a = int_in_vector(candidate, ctrl_a[k]);
            bool b = int_in_vector(candidate, ctrl_b[k]);
            if (a && b) {
              ans.push_back(0);
            } else if (a && !b) {
              ans.push_back(1);
            } else if (!a && b) {
              ans.push_back(2);
            } else {
              ans.push_back(1);
            }
          }
          break;
        }

        if (found_partition) break;
      }
      Abc_Print(1, "PO %s support partition: %d\n", Abc_ObjName(pNtk_PO), found_partition);
      if (found_partition) {
        for (int ans_i : ans) {
          std::cout << ans_i;
        }
        std::cout << std::endl;
      }
    } else {
      Abc_Print(1, "PO %s support partition: 0\n", Abc_ObjName(pNtk_PO));
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
  Abc_Print(-2, "\t        for each PO, check if it is OR bi-decomposable\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// find maximum single-fanout cones (MSFCs) that covers all nodes
bool msfc_compare(Abc_Obj_t* pObjA, Abc_Obj_t* pObjB) {
  return Abc_ObjId(pObjA) < Abc_ObjId(pObjB);
}

bool msfc_compare_v(std::vector<Abc_Obj_t*> pA, std::vector<Abc_Obj_t*> pB) {
  return Abc_ObjId(pA[0]) < Abc_ObjId(pB[0]);
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  int len = Vec_PtrSize((pNtk)->vObjs);
  int* isCovered = new int[len];
  for (int i=0; i<len; ++i)
    isCovered[i] = false;
  std::vector<std::vector<Abc_Obj_t*> > msfcs;
  
  int i;
  Abc_Obj_t* pObj;
  // Traverse all nodes and constant1
  for (i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ ) {
    if ((pObj)==NULL || !(Abc_ObjIsNode(pObj) || (pObj->Type==ABC_OBJ_CONST1)) ) continue;
    if (isCovered[Abc_ObjId(pObj)] | (Abc_ObjFanoutNum(pObj) <= 1)) continue;

    // find all connected nodes that formed a msfc
    std::vector<Abc_Obj_t*> searchStack, msfc;
    searchStack.push_back(pObj);
    msfc.push_back(pObj);
    isCovered[Abc_ObjId(pObj)] = true;

    while (!searchStack.empty()) {
      Abc_Obj_t* pObjCurr = (Abc_Obj_t*)searchStack.back();
      searchStack.pop_back();

      Abc_Obj_t* pFanin;
      int j;
      Abc_ObjForEachFanin(pObjCurr, pFanin, j) {
        if (Abc_ObjIsPi(pFanin) || Abc_ObjFanoutNum(pFanin) != 1) continue;
        searchStack.push_back(pFanin);
        msfc.push_back(pFanin);
        isCovered[Abc_ObjId(pFanin)] = true;
      }
    }

    std::sort(msfc.begin(), msfc.end(), msfc_compare);
    msfcs.push_back(msfc);
  }

  // traverse from PO
  Abc_NtkForEachPo(pNtk, pObj, i) {
    if (isCovered[Abc_ObjId(pObj)] || Abc_ObjFanoutNum(pObj)==1) continue;

    // find all connected nodes that formed a msfc
    std::vector<Abc_Obj_t*> searchStack, msfc;
    int j;
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      if (Abc_ObjIsPi(pFanin) || Abc_ObjFanoutNum(pFanin) != 1) continue;
      searchStack.push_back(pFanin);
      msfc.push_back(pFanin);
      isCovered[Abc_ObjId(pFanin)] = true;
    }

    while (!searchStack.empty()) {
      Abc_Obj_t* pObjCurr = (Abc_Obj_t*)searchStack.back();
      searchStack.pop_back();
      Abc_ObjForEachFanin(pObjCurr, pFanin, j) {
        if (Abc_ObjIsPi(pFanin) || Abc_ObjFanoutNum(pFanin) != 1) continue;
        searchStack.push_back(pFanin);
        msfc.push_back(pFanin);
        isCovered[Abc_ObjId(pFanin)] = true;
      }
    }

    if (!msfc.empty()) {
      std::sort(msfc.begin(), msfc.end(), msfc_compare);
      msfcs.push_back(msfc);
    }
  }

  // print MSFCs
  std::sort(msfcs.begin(), msfcs.end(), msfc_compare_v);
  i = 0;
  for (; i < msfcs.size(); ++i) {
    printf("MSFC %d: %s", i, Abc_ObjName(msfcs[i][0]));
    for (int j=1; j < msfcs[i].size(); ++j) printf(",%s", Abc_ObjName(msfcs[i][j]));
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
  int fAll, fClean, fRecord;
  Abc_NtkStrash(pNtk, fAll, fClean, fRecord);
  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the MSFCs in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}