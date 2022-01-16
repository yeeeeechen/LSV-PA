#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/aig/aig.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <vector>

namespace{
static int Lsv_CommandOrBiDecomposit(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBiDecomposit, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

extern "C" {
  Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

//////////////////////////////////////////////////////////////
int Lsv_CommandOrBiDecomposit(Abc_Frame_t* pAbci, int argc, char** argv) {  
  Abc_Ntk_t * pAbc = Abc_FrameReadNtk(pAbci);
  int j;
  //int p = 1;
  Abc_Obj_t* pObjCo;

  Abc_NtkForEachCo(pAbc, pObjCo, j)
  { 
    Abc_Ntk_t* pNtk = Abc_NtkCreateCone(pAbc, Abc_ObjFanin0(pObjCo), Abc_ObjName(pObjCo), 0 );
    if ( Abc_ObjFaninC0(pObjCo) ) {
       Abc_ObjXorFaninC( Abc_NtkPo(pNtk, 0), 0);
    }
    //Abc_Ntk_t* pNtk = pAbc;
    //std::vector<char> v_ntk;
    //v_ntk = pNtk->vPis;
    Aig_Man_t* pMan = Abc_NtkToDar(pNtk,0,0);
    Cnf_Dat_t* pCnf = Cnf_Derive(pMan, Aig_ManCoNum(pMan));
	int nVars = pCnf->nVars;
    Cnf_Dat_t* pCnf_dup = Cnf_DataDup(pCnf);
    Cnf_DataLift(pCnf_dup, nVars);
    Cnf_Dat_t* pCnf_dup_dup = Cnf_DataDup(pCnf);
    Cnf_DataLift(pCnf_dup_dup, 2*nVars);

    //initilize sat solver
    sat_solver* pSat = sat_solver_new();

    for ( int i = 0; i < pCnf->nClauses; i++ )
      {
      if ( !sat_solver_addclause( pSat, pCnf->pClauses[i], pCnf->pClauses[i+1] ) )
      {
        sat_solver_delete( pSat );
      }
    }

    for ( int i = 0; i < pCnf_dup->nClauses; i++ )
      {
      if ( !sat_solver_addclause( pSat, pCnf_dup->pClauses[i], pCnf_dup->pClauses[i+1] ) )
      {
        sat_solver_delete( pSat );
      }
    }

    for ( int i = 0; i < pCnf_dup_dup->nClauses; i++ )
    {
      if ( !sat_solver_addclause( pSat, pCnf_dup_dup->pClauses[i], pCnf_dup_dup->pClauses[i+1] ) )
      {
        sat_solver_delete( pSat );
      }
    }
    int numPi = Aig_ManCiNum(pMan);
    lit as [2*numPi + 3];
    int alpha[numPi];
    int beta[numPi];
    int Lits[3];
    int i;
    Aig_Obj_t* pObjCi;
    Aig_ManForEachCi(pMan, pObjCi, i){
      alpha[i] = sat_solver_addvar(pSat);
      Lits[0] = toLitCond( pCnf->pVarNums[Aig_ObjId(pObjCi)], 0 );
      Lits[1] = toLitCond( pCnf_dup->pVarNums[Aig_ObjId(pObjCi)], 1 );
      Lits[2] = toLitCond( alpha[i] ,0 );
      sat_solver_addclause( pSat, Lits, Lits + 3 );

      Lits[0] = toLitCond( pCnf->pVarNums[Aig_ObjId(pObjCi)], 1 );
      Lits[1] = toLitCond( pCnf_dup->pVarNums[Aig_ObjId(pObjCi)], 0 );
      Lits[2] = toLitCond( alpha[i],0 );
      sat_solver_addclause( pSat, Lits, Lits + 3 );      

      beta[i] = sat_solver_addvar(pSat);
	Lits[0] = toLitCond( pCnf->pVarNums[Aig_ObjId(pObjCi)], 0 );
        Lits[1] = toLitCond( pCnf_dup_dup->pVarNums[Aig_ObjId(pObjCi)], 1 );
        Lits[2] = toLitCond( beta[i] ,0 );
        sat_solver_addclause( pSat, Lits, Lits + 3 );
 
        Lits[0] = toLitCond( pCnf->pVarNums[Aig_ObjId(pObjCi)], 1 );
        Lits[1] = toLitCond( pCnf_dup_dup->pVarNums[Aig_ObjId(pObjCi)], 0 );
        Lits[2] = toLitCond( beta[i],0 );
        sat_solver_addclause( pSat, Lits, Lits + 3 );
    }

	for (int i=0; i < numPi; i++){
          as[numPi+i+3] 	= toLitCond(beta[i],  1);
          as[i+3]       = toLitCond(alpha[i], 1);
        }

    //assign f(x)=1, f'(x)=0, f''(x)=0
	as[0] = toLitCond(pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pMan, 0))], 0);
	as[1] = toLitCond(pCnf_dup->pVarNums[Aig_ObjId(Aig_ManCo(pMan, 0))], 1);
	as[2] = toLitCond(pCnf_dup_dup->pVarNums[Aig_ObjId(Aig_ManCo(pMan, 0))], 1);
    int unsat = 0;

   	for( int l=0; l<numPi; ++l){ 
      if (unsat == 1) break;
      for(int k=l+1; k<numPi; ++k){
        
        as[l+3]       = toLitCond(alpha[l], 0);
        as[numPi+k+3] = toLitCond(beta[k],  0);
        
		int nLits, * pLits;
		int p;
        p = sat_solver_solve(pSat,as ,as + 2*numPi + 3 ,0 ,0 ,0 ,0);
        nLits = sat_solver_final( pSat, &pLits );
		as[l+3]       = toLitCond(alpha[l], 1);
        as[numPi+k+3] = toLitCond(beta[k],  1);


        if (p == l_False){
          printf("PO %s support partition: 1\n", Abc_ObjName(pObjCo));
          for (int m=0; m < numPi; m++){
          if (m == l){
            printf("2");
          }
          else if(m == k){
            printf("1");
          }
          else{
            printf("0");
          }
        }
		  unsat = 1;
          printf("\n");
          break;
        }
      }
    }

    if (unsat != 1) { 
       printf("PO %s support partition: 0\n", Abc_ObjName(pObjCo));
    }
  }
  return 0;
}
}
