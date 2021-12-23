#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>

#include "sat/cnf/cnf.h"

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
extern "C" struct Cnf_Dat_t_;
typedef struct Cnf_Dat_t_ Cnf_Dat_t;
extern "C" Cnf_Dat_t * Cnf_Derive( Aig_Man_t * pAig, int nOutputs );
extern "C" void *      Cnf_DataWriteIntoSolver( Cnf_Dat_t * p, int nFrames, int fInit );
extern void            Cnf_DataLift( Cnf_Dat_t * p, int nVarsPlus );

using namespace std;

namespace {
static int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_OrBidec(Abc_Ntk_t* pNtk) {
  int i, j, num_Pi;
  Abc_Obj_t* pPo;
  Abc_Ntk_t* pNtkCone;
  Aig_Man_t* pAig;
  Cnf_Dat_t* pCnf;
  Aig_Obj_t* pObj;
  sat_solver * pSat;
  int * pBeg, * pEnd;

  Abc_NtkForEachPo(pNtk, pPo, i) { 
    // printf("pPo: %d\n", i);
    pNtkCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 0);
    if (Abc_ObjFaninC0(pPo)) Abc_ObjXorFaninC( Abc_NtkPo(pNtkCone, 0), 0 );
    pAig = Abc_NtkToDar(pNtkCone, 0 , 1);
    pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));

    // store varnum(xi), (f(xi)) and varshift
    num_Pi = Aig_ManCiNum(pAig);
    int* _Pi_ID = new int[num_Pi];
    Aig_ManForEachCi(pAig, pObj, j) {
      _Pi_ID[j] = pCnf->pVarNums[Aig_ObjId(pObj)];
    }
    int _Po_ID = pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pCnf->pMan, 0))];
    int _varShift = pCnf->nVars;
    lit Lits[1]; // for output
    
    // f(x)
    pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    Lits[0] = toLitCond(_Po_ID, 0);
    sat_solver_addclause( pSat, Lits, Lits+1);

    // not f(x')
    Cnf_DataLift(pCnf, _varShift);
    sat_solver_setnvars( pSat, sat_solver_nvars(pSat) + _varShift );
    Cnf_CnfForClause( pCnf, pBeg, pEnd, j ) {
      sat_solver_addclause( pSat, pBeg, pEnd );
    }
    Lits[0] = toLitCond( _Po_ID + _varShift, 1);
    sat_solver_addclause( pSat, Lits, Lits+1);

    // not f(x'')
    Cnf_DataLift(pCnf, _varShift);
    sat_solver_setnvars( pSat, sat_solver_nvars(pSat) + _varShift );
    Cnf_CnfForClause( pCnf, pBeg, pEnd, j ) {
      sat_solver_addclause( pSat, pBeg, pEnd );
    }
    Lits[0] = toLitCond( _Po_ID + 2*_varShift, 1);
    sat_solver_addclause( pSat, Lits, Lits+1);
    // variable number: 3*_varShift
    int alpha_offset = 3*_varShift, beta_offset = 3*_varShift+num_Pi;

    // unit assumption
    sat_solver_setnvars(pSat, sat_solver_nvars(pSat)+(num_Pi*2));
    // for (int k = 0; k < num_Pi; k++) {
    //   // alpha & beta
    //   sat_solver_add_buffer_enable(pSat, _Pi_ID[k], _Pi_ID[k]+_varShift, alpha_offset+k, 0);
    //   sat_solver_add_buffer_enable(pSat, _Pi_ID[k], _Pi_ID[k]+_varShift, beta_offset+k, 0);
    // }
    lit Lits3[3];
    for (int k = 0; k < num_Pi; k++) {
      Lits3[0] = toLitCond(_Pi_ID[k], 0);
      Lits3[1] = toLitCond(_Pi_ID[k]+_varShift, 1);
      Lits3[2] = toLitCond(alpha_offset+k, 0);
      sat_solver_addclause(pSat, Lits3, Lits3+3);

      Lits3[0] = toLitCond(_Pi_ID[k], 1);
      Lits3[1] = toLitCond(_Pi_ID[k]+_varShift, 0);
      Lits3[2] = toLitCond(alpha_offset+k, 0);
      sat_solver_addclause(pSat, Lits3, Lits3+3);

      Lits3[0] = toLitCond(_Pi_ID[k], 0);
      Lits3[1] = toLitCond(_Pi_ID[k]+_varShift*2, 1);
      Lits3[2] = toLitCond(beta_offset+k, 0);
      sat_solver_addclause(pSat, Lits3, Lits3+3);

      Lits3[0] = toLitCond(_Pi_ID[k], 1);
      Lits3[1] = toLitCond(_Pi_ID[k]+_varShift*2, 0);
      Lits3[2] = toLitCond(beta_offset+k, 0);
      sat_solver_addclause(pSat, Lits3, Lits3+3);
    }
    // printf("final number of clause: %d \n", sat_solver_nclauses(pSat));

    int* assumpList = new int[num_Pi*2];
    bool unsat = false;
    int nFinal, *pFinal;
    vector<bool> _fccLit(num_Pi*2, false);
    for (int a = 0; a < num_Pi && !unsat; a++) {
      for (int b = a+1; b < num_Pi && !unsat; b++) {
        // set assumptiom list 
        for (int p = 0; p < num_Pi; p++) {
          if (p == a) assumpList[p] = toLitCond(alpha_offset+p, 0);
          else assumpList[p] = toLitCond(alpha_offset+p, 1);

          if (p == b) assumpList[p+num_Pi] = toLitCond(beta_offset+p, 0);
          else assumpList[p+num_Pi] = toLitCond(beta_offset+p, 1);
        }

        int status = sat_solver_solve(pSat, assumpList, (assumpList+num_Pi*2), 0, 0, 0, 0);
        if (status == l_False) { // UNSAT
          // printf("unsat!");
          nFinal = sat_solver_final(pSat, &pFinal);
          for (int r = 0; r < nFinal; r++) {
            _fccLit[Abc_Lit2Var(pFinal[r])-alpha_offset] = true;
          }
          unsat = true;
          break;
        }
      }
    }

    printf("PO %s support partition: %d\n", Abc_ObjName(pPo), (int)unsat);
    if (unsat) {
      for (int p = 0; p < num_Pi; p++) {
        if (_fccLit[p] == true && _fccLit[p+num_Pi] == true) printf("0"); //Xc
        else if (_fccLit[p] == false && _fccLit[p+num_Pi] == true) printf("1"); // Xa
        else if (_fccLit[p] == true && _fccLit[p+num_Pi] == false) printf("2"); //Xb
        else printf("\nwrong :<\n");
      }
      printf("\n");
    }

    delete [] assumpList;
    Abc_NtkDelete(pNtkCone);
  }  
}

int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  // print helping message
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  // empty network
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  // call main function
  Lsv_OrBidec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        find maximum single-fanout cones (MSFCs) that covers all nodes (excluding PIs and POs) of a given AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

}