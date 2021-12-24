#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/aig/aig.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <iostream>

static int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);
static int debugLevel = 0;

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

/**************************/
/* Command Registeration */
/**************************/

void pa2_init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
}

void pa2_destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t pa2_frame_initializer = {pa2_init, pa2_destroy};

struct PA2PackageRegistrationManager {
  PA2PackageRegistrationManager() { Abc_FrameAddInitializer(&pa2_frame_initializer); }
} pa2PackageRegistrationManager;

/****************************/
/* Alogirthm Implementation */
/****************************/
sat_solver* Lsv_OrBidecInitSatSolver( Cnf_Dat_t* pCnf, lit* Sat_ACtrl_Vars, lit* Sat_BCtrl_Vars, lit* Sat_AssumpList ) {

  sat_solver * pSat;
  bool Sat_ret_val;

  // Cnf Clauses iteration
  Aig_Obj_t * pAigObj;
  lit Aig_Co_Lit;
  lit * Cnf_pBeg, * Cnf_pEnd; 
  int Cnf_Clause_i;

  // Controlling variable (alpha & beta)
  const int Cnf_Vars_Num = pCnf->nVars;
  int Aig_Ci_i;
  int Cnf_Ci_VarNum;

  // Controlling clauses (alpha & beta)
  lit Sat_XNOR_If[3];
  lit Sat_XNOR_Of[3];


  /*************************/
  /* Push netlist function */
  /*************************/
  pAigObj = Aig_ManCo( pCnf->pMan, 0 ); // Aig Co
  // Construct SAT solver & Push f(X)
  pSat = (sat_solver*)Cnf_DataWriteIntoSolver( pCnf, 1, 0 );
  Aig_Co_Lit = Abc_Var2Lit( pCnf->pVarNums[pAigObj->Id], Aig_ObjFaninC0(pAigObj) );
  Sat_ret_val = sat_solver_addclause( pSat, &Aig_Co_Lit, &Aig_Co_Lit + 1 );

  // Push f(X')
  Cnf_DataLift( pCnf, Cnf_Vars_Num );

  Cnf_CnfForClause( pCnf, Cnf_pBeg, Cnf_pEnd, Cnf_Clause_i ) 
    Sat_ret_val = sat_solver_addclause( pSat, Cnf_pBeg, Cnf_pEnd );

  Aig_Co_Lit = Abc_Var2Lit( pCnf->pVarNums[pAigObj->Id], Aig_ObjFaninC0(pAigObj)^1 ); // ~f(X')
  Sat_ret_val = sat_solver_addclause( pSat, &Aig_Co_Lit, &Aig_Co_Lit + 1 );

  // f(X'')
  Cnf_DataLift( pCnf, Cnf_Vars_Num );

  Cnf_CnfForClause( pCnf, Cnf_pBeg, Cnf_pEnd, Cnf_Clause_i ) 
    Sat_ret_val = sat_solver_addclause( pSat, Cnf_pBeg, Cnf_pEnd );

  Aig_Co_Lit = Abc_Var2Lit( pCnf->pVarNums[pAigObj->Id], Aig_ObjFaninC0(pAigObj)^1 );  // ~f(X'')
  Sat_ret_val = sat_solver_addclause( pSat, &Aig_Co_Lit, &Aig_Co_Lit + 1 );

  /********************************************/
  /* Add controlling variables (alpha, beta) */
  /*******************************************/
  pAigObj = nullptr;

  Aig_ManForEachCi( pCnf->pMan, pAigObj, Aig_Ci_i ) {
    Cnf_Ci_VarNum = pCnf->pVarNums[pAigObj->Id];
    Sat_ACtrl_Vars[Aig_Ci_i] = sat_solver_addvar( pSat );
    Sat_BCtrl_Vars[Aig_Ci_i] = sat_solver_addvar( pSat );

    // (x_i = x_i') + alpha_i
    Sat_XNOR_If[0] = toLitCond( Cnf_Ci_VarNum - 2 * Cnf_Vars_Num, 0 ); // x
    Sat_XNOR_If[1] = toLitCond( Cnf_Ci_VarNum -     Cnf_Vars_Num, 1 ); // ~x'
    Sat_XNOR_If[2] = toLitCond( Sat_ACtrl_Vars[Aig_Ci_i]             , 0 );
    Sat_XNOR_Of[0] = toLitCond( Cnf_Ci_VarNum - 2 * Cnf_Vars_Num, 1 ); // x
    Sat_XNOR_Of[1] = toLitCond( Cnf_Ci_VarNum -     Cnf_Vars_Num, 0 ); // ~x'
    Sat_XNOR_Of[2] = toLitCond( Sat_ACtrl_Vars[Aig_Ci_i]             , 0 );
    Sat_ret_val = sat_solver_addclause( pSat, Sat_XNOR_If, Sat_XNOR_If + 3); // (x_i + ~x_i' + alpha_i)
    Sat_ret_val = sat_solver_addclause( pSat, Sat_XNOR_Of, Sat_XNOR_Of + 3); // (~x_i + x_i' + alpha_i)

    // (x_i = x_i'') + beta_i
    Sat_XNOR_If[0] = toLitCond( Cnf_Ci_VarNum - 2 * Cnf_Vars_Num, 0 ); // x
    Sat_XNOR_If[1] = toLitCond( Cnf_Ci_VarNum                   , 1 ); // ~x''
    Sat_XNOR_If[2] = toLitCond( Sat_BCtrl_Vars[Aig_Ci_i]             , 0 );
    Sat_XNOR_Of[0] = toLitCond( Cnf_Ci_VarNum - 2 * Cnf_Vars_Num, 1 ); // ~x
    Sat_XNOR_Of[1] = toLitCond( Cnf_Ci_VarNum                   , 0 ); // x''
    Sat_XNOR_Of[2] = toLitCond( Sat_BCtrl_Vars[Aig_Ci_i]             , 0 );
    Sat_ret_val = sat_solver_addclause( pSat, Sat_XNOR_If, Sat_XNOR_If + 3); // (x_i + ~x_i'' + beta_i)
    Sat_ret_val = sat_solver_addclause( pSat, Sat_XNOR_Of, Sat_XNOR_Of + 3); // (~x_i + x_i'' + beta_i)

    // Init all vars in assumption list to 0
    Sat_AssumpList[ Sat_ACtrl_Vars[Aig_Ci_i] - Sat_ACtrl_Vars[0] ] = toLitCond( Sat_ACtrl_Vars[Aig_Ci_i], 1 );
    Sat_AssumpList[ Sat_BCtrl_Vars[Aig_Ci_i] - Sat_ACtrl_Vars[0] ] = toLitCond( Sat_BCtrl_Vars[Aig_Ci_i], 1 );

  }

  return pSat;
}

std::string Lsv_OrBidecCore( Cnf_Dat_t* pCnf ) {

  sat_solver * pSat;
  lit * Sat_ACtrl_Vars;
  lit * Sat_BCtrl_Vars;
  lit * Sat_AssumpList;

  const int Aig_Ci_Num = Aig_ManCiNum(pCnf->pMan);

  int Sat_Ctrl_floor;
  lit * Sat_conf_final;
  int Sat_conf_final_size;
  
  int Sat_ret_val;

  int Aig_AVar_Num, Aig_BVar_Num;
  int Sat_Assump_idx;
  int Sat_Assump_ACtrl_pol, Sat_Assump_BCtrl_pol;
  std::string Partition;

  /******************************************/
  /* Initialze sat solver & assumption list */
  /******************************************/
  Sat_ACtrl_Vars = ABC_ALLOC( lit, Aig_Ci_Num     );
  Sat_BCtrl_Vars = ABC_ALLOC( lit, Aig_Ci_Num     );
  Sat_AssumpList = ABC_ALLOC( lit, Aig_Ci_Num * 2 );
  pSat = Lsv_OrBidecInitSatSolver( pCnf, Sat_ACtrl_Vars, Sat_BCtrl_Vars, Sat_AssumpList );

  Sat_Ctrl_floor = Sat_ACtrl_Vars[0];
  Partition = "";
  Aig_AVar_Num = Aig_BVar_Num = 0;

  /****************************/
  /* Incremental sat solving  */
  /****************************/
  for ( int Aig_Ci_i = 0 ; Aig_Ci_i < Aig_Ci_Num ; ++Aig_Ci_i ) {
    Sat_AssumpList[ Sat_ACtrl_Vars[Aig_Ci_i] - Sat_Ctrl_floor ] = toLitCond( Sat_ACtrl_Vars[Aig_Ci_i], 0 ); // x -> XA

    for( int Aig_Ci_j = Aig_Ci_i+1 ; Aig_Ci_j < Aig_Ci_Num ; ++Aig_Ci_j ) {
      Sat_AssumpList[ Sat_BCtrl_Vars[Aig_Ci_j] - Sat_Ctrl_floor ] = toLitCond( Sat_BCtrl_Vars[Aig_Ci_j], 0 ); // x -> XB

      Sat_ret_val = sat_solver_solve(
          pSat, Sat_AssumpList, Sat_AssumpList + 2 * Aig_Ci_Num, 
          (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );

      if ( Sat_ret_val == l_False ) { // Unsat: Partition found
        Sat_conf_final_size = sat_solver_final( pSat, &Sat_conf_final );
        Sat_AssumpList[ Sat_ACtrl_Vars[Aig_Ci_i] - Sat_Ctrl_floor ] = toLitCond( Sat_ACtrl_Vars[Aig_Ci_i], 1 ); 
        Sat_AssumpList[ Sat_BCtrl_Vars[Aig_Ci_j] - Sat_Ctrl_floor ] = toLitCond( Sat_BCtrl_Vars[Aig_Ci_j], 1 );
        goto PARTITION;
      }

      Sat_AssumpList[ Sat_BCtrl_Vars[Aig_Ci_j] - Sat_Ctrl_floor ] = toLitCond( Sat_BCtrl_Vars[Aig_Ci_j], 1 ); // x -> XB
    } 
    Sat_AssumpList[ Sat_ACtrl_Vars[Aig_Ci_i] - Sat_Ctrl_floor ] = toLitCond( Sat_ACtrl_Vars[Aig_Ci_i], 1 ); // x -> XA
  }
  
  return Partition;

PARTITION:
  for ( int Sat_conf_final_i = 0 ; Sat_conf_final_i < Sat_conf_final_size ; ++ Sat_conf_final_i ) {
    assert( !lit_sign( Sat_conf_final[Sat_conf_final_i] ) && "The literal in final conflict clause should be positive" );
    Sat_Assump_idx = (Sat_conf_final[Sat_conf_final_i] >> 1) - Sat_Ctrl_floor;

    Sat_Assump_ACtrl_pol = lit_sign( Sat_AssumpList[ ((Sat_Assump_idx>>1)<<1) + 0 ] );
    Sat_Assump_BCtrl_pol = lit_sign( Sat_AssumpList[ ((Sat_Assump_idx>>1)<<1) + 1 ] );
    if      ( !Sat_Assump_ACtrl_pol ) {
      Aig_AVar_Num -= 1;
    }
    else if ( !Sat_Assump_BCtrl_pol ) {
      Aig_BVar_Num -= 1;
    } 
    else  {
      if ( Sat_Assump_idx ^ 1 ) Aig_AVar_Num += 1;
      else Aig_BVar_Num += 1;
    }

    Sat_AssumpList[ Sat_Assump_idx ] = Sat_conf_final[Sat_conf_final_i];
  }

  for ( int Aig_Ci_i = 0 ; Aig_Ci_i < Aig_Ci_Num ; ++Aig_Ci_i ) {
    Sat_Assump_ACtrl_pol = lit_sign( Sat_AssumpList[ Sat_ACtrl_Vars[Aig_Ci_i] - Sat_Ctrl_floor ] );
    Sat_Assump_BCtrl_pol = lit_sign( Sat_AssumpList[ Sat_BCtrl_Vars[Aig_Ci_i] - Sat_Ctrl_floor ] );
    if      ( Sat_Assump_ACtrl_pol == 0 && Sat_Assump_BCtrl_pol == 0 ) { // alpha = 0, beta = 0
      Partition += "0";
    }
    else if ( Sat_Assump_ACtrl_pol == 1 && Sat_Assump_BCtrl_pol == 0 ) { // alpha = X, beta = 0
      Partition += "2";
      Aig_AVar_Num += 1;
    }
    else if ( Sat_Assump_ACtrl_pol == 0 && Sat_Assump_BCtrl_pol == 1 ) { // alpha = 0, beta = X
      Partition += "1";
      Aig_BVar_Num += 1;
    }
    else {
      if ( Aig_AVar_Num == 0 ) {
        Partition += "2";
        Aig_AVar_Num += 1;
      } else {
        Partition += "1";
        Aig_BVar_Num += 1;
      }
    }
  }
  return Partition;
}



void Lsv_AigOrBidec(Abc_Ntk_t* pNtk) {
  Abc_Obj_t * pObj;
  int Ntk_Co_i;
  Abc_Ntk_t * pNtkCone;
  Aig_Man_t * pAig;
  Cnf_Dat_t * pCnf;
  

  Abc_NtkForEachCo(pNtk, pObj, Ntk_Co_i) {
    pNtkCone = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0 );

    pAig = Abc_NtkToDar(pNtkCone, 0, 1);
    pCnf = Cnf_Derive( pAig, Aig_ManCoNum(pAig) );
    if ( pObj->fCompl0 ) Aig_ObjChild0Flip(Aig_ManCo(pCnf->pMan, 0));

    std::string Partition = Lsv_OrBidecCore( pCnf );
    if ( Partition.size() == 0 ) {
      printf("PO %s support partition: %d\n", Abc_ObjName(pObj), 0);
    } else {
      printf("PO %s support partition: %d\n", Abc_ObjName(pObj), 1);
      printf("%s\n", Partition.c_str());
    }
  }

  return ;
}

/* Command Entry Point */
int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "hv")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      case 'v':
        debugLevel = 1; break;
      default:
        goto usage;
    }
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  Lsv_AigOrBidec(pNtk);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        determine whether each PO of a given AIG\n");
  Abc_Print(-2, "\t        is OR bi-decomposable, if yes print the \n");
  Abc_Print(-2, "\t        three variable set. \n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  Abc_Print(-2, "\t-v    : print the debug message\n");
  return 1;
}
