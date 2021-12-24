#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <vector>
#include <iostream>

using namespace std;

namespace lsv_pa2
{
static int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

extern "C" {
  extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}




void Lsv_AddEqualClause( sat_solver * pSat, int a, int b, int c)
{
    lit Lits[3];
    int Cid;
    Lits[0] = toLitCond( a, 0 );
    Lits[1] = toLitCond( b, 1 );
    Lits[2] = toLitCond( c, 0 );
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    Lits[0] = toLitCond( a, 1 );
    Lits[1] = toLitCond( b, 0 );
    Lits[2] = toLitCond( c, 0 );
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
}

void Lsv_NtkOrBidec(Abc_Ntk_t* pNtk) { 
  Abc_Obj_t* pObj;
  int i_po;
  Abc_NtkForEachPo(pNtk, pObj, i_po) {
    Abc_Ntk_t* pNtkCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0);
    if (Abc_ObjFaninC0(pObj)) Abc_ObjXorFaninC(Abc_NtkPo(pNtkCone, 0), 0);

    Aig_Man_t* pMan = Abc_NtkToDar(pNtkCone, 0, 1);

    Cnf_Dat_t* pCnf = Cnf_Derive(pMan, 1);
    sat_solver* pSat = (sat_solver *)Cnf_DataWriteIntoSolver(pCnf, 1, 0);

    int Cid, i_out;
    lit Lits[3];
    Aig_Obj_t* pObj_out;
    Aig_ManForEachCo( pMan, pObj_out, i_out ) {
      int k = (pCnf->pVarNums[pObj_out->Id]);
      Lits[0] = toLitCond( k, 0 );
      Cid = sat_solver_addclause( pSat, Lits, Lits + 1 );
    }

    // renamed version X'
    Cnf_DataLift(pCnf, pCnf->nVars);
    Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
    Aig_ManForEachCo( pMan, pObj_out, i_out ) {
      int k = (pCnf->pVarNums[pObj_out->Id]);
      Lits[0] = toLitCond( k, 1 );
      Cid = sat_solver_addclause( pSat, Lits, Lits + 1 );
    }

    // renamed version X'
    Cnf_DataLift(pCnf, pCnf->nVars); 
    Cnf_DataWriteIntoSolverInt(pSat, pCnf, 1, 0);
    Aig_ManForEachCo( pMan, pObj_out, i_out ) {
      int k = (pCnf->pVarNums[pObj_out->Id]);
      Lits[0] = toLitCond( k, 1 );
      Cid = sat_solver_addclause( pSat, Lits, Lits + 1 );
    }

    
    int pos  = 3*(pCnf->nVars)+1;
    Aig_Obj_t* pObj_in;
    int i_in;
    Aig_ManForEachCi( pMan, pObj_in, i_in ){
      int k = pCnf->pVarNums[pObj_in->Id];
      
      Lsv_AddEqualClause( pSat, k-2*(pCnf->nVars), k-1*(pCnf->nVars), pos++);
      Lsv_AddEqualClause( pSat, k-2*(pCnf->nVars), k, pos++);
      // lit Lits[3];
      // int Cid;
      // Lits[0] = toLitCond( k-2*(pCnf->nVars), 0 );
      // Lits[1] = toLitCond( k-1*(pCnf->nVars), 1 );
      // Lits[2] = toLitCond( pos++, 0 );
      // Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
      // Lits[0] = toLitCond( k-2*(pCnf->nVars), 1 );
      // Lits[1] = toLitCond( k-1*(pCnf->nVars), 0 );
      // Lits[2] = toLitCond( pos++, 0 );
      // Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
      // Lits[0] = toLitCond( k-2*(pCnf->nVars), 0 );
      // Lits[1] = toLitCond( k, 1 );
      // Lits[2] = toLitCond( pos++, 0 );
      // Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
      // Lits[0] = toLitCond( k-2*(pCnf->nVars), 1 );
      // Lits[1] = toLitCond( k, 0 );
      // Lits[2] = toLitCond( pos++, 0 );
      // Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    }


    // make assumption and solve sat
    vector<int> a_vec;
    vector<int> b_vec;
    lit assumpList[2*Aig_ManCiNum(pMan)]; // assumption

    int status = 0;
    if (Aig_ManCiNum(pMan)>0) {
      for (int m=0; m<Aig_ManCiNum(pMan)-1; m++) {
        for (int n=m+1; n<Aig_ManCiNum(pMan); n++) {
          for (int r=0; r<Aig_ManCiNum(pMan); r++) {
            assumpList[2*r] = toLitCond( (3*(pCnf->nVars) + 2*r + 1), 1 );
            a_vec.push_back(3*(pCnf->nVars) + 2*r + 1);
            assumpList[2*r+1] = toLitCond((3*(pCnf->nVars) + 2*r + 1 + 1), 1 );
            b_vec.push_back(3*(pCnf->nVars) + 2*r + 1 + 1);
          }
          assumpList[2*m] = 0;
          assumpList[2*n+1] = 0;
          status = sat_solver_solve(pSat, assumpList, assumpList+2*Aig_ManCiNum(pMan), 0, 0, 0, 0);
          if (status == -1) goto output;
        }
      }
    }

    output:
    vector<int> out_vec;
    if (status == -1) {
      for(int j=0; j<2*Aig_ManCiNum(pMan); j++) assumpList[j] = 1;
      int nFinal, *pFinal;
      nFinal = sat_solver_final( pSat, &pFinal );
      for (int j=0; j<nFinal; j++) assumpList[pFinal[j]/2-(pCnf->nVars)*3-1] = 0;
      int cnt_1 = 0;
      int cnt_2 = 0;
      for (int j=0; j<Aig_ManCiNum(pMan); j++) {
        if ((assumpList[2*j] == 1) && (assumpList[2*j+1] == 0)) out_vec.push_back(2);
        else if ((assumpList[2*j] == 0) && (assumpList[2*j+1] == 1)) out_vec.push_back(1);
        else if ((assumpList[2*j] == 0) && (assumpList[2*j+1] == 0)) out_vec.push_back(0);
        else if (cnt_2 > cnt_1) {
          out_vec.push_back(1); 
          cnt_1++;
        }
        else {
          cout<<"2"; 
          cnt_2++;
        }          
      }
      printf("PO %s support partition: %d\n", Abc_ObjName(pObj), 1);
      for (int i=0; i<out_vec.size(); i++) cout << out_vec[i];
      cout << endl;
    }
    else printf("PO %s support partition: %d\n", Abc_ObjName(pObj), 0);
  }
}

int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  assert(  Abc_NtkIsStrash( pNtk ) );
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
  Abc_Print(-2, "\t        decide whether each circuit PO is OR bi-decomposable\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
}