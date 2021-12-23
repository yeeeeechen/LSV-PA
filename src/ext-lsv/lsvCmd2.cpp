#ifndef _LSV_BIOR_
#define _LSV_BIOR_

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "proof/int/intInt.h"
#include <vector>
#include <iostream>

//all *2 should be replaced by <<1, but I am too lazy :) 

//
extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
//

static int Lsv_CommandPrintBior(Abc_Frame_t* pAbc, int argc, char** argv);

void biorInit(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandPrintBior, 0);
}

void biorDestroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t biorFrameInitializer = {biorInit, biorDestroy};

struct biorPackageMgr {
  biorPackageMgr() { Abc_FrameAddInitializer(&biorFrameInitializer); }
} LsvBiorPackageMgr;

void Lsv_LiftAndAddClause(Cnf_Dat_t * pCnf, sat_solver* pSat, int nInitVars){
  Cnf_DataLift(pCnf, nInitVars);

  for (int i = 0; i < pCnf->nClauses; i++ ){
    if ( !sat_solver_addclause( pSat, pCnf->pClauses[i], pCnf->pClauses[i+1] ) ){
      sat_solver_delete( pSat );
      std::cout<<"addclause fail!"<<std::endl;
      assert(0);
    }
  }
}

void inline Lsv_AddClauseForPo(Cnf_Dat_t* pCnf, sat_solver* pSat, Aig_Man_t* pAigMgr, int isNegPhase){
  int Lits[2];
  Lits[0] = toLitCond( pCnf->pVarNums[Aig_ManCo(pAigMgr, 0)->Id], isNegPhase );
  //std::cout<<Lits[0]<<std::endl;

  sat_solver_addclause(pSat, Lits, Lits+1 );
  //std::cout<<pCnf->pVarNums[Aig_ManCo(pAigMgr, 0)->Id]<<std::endl;
}

void inline Lsv_AddClauseForEq( sat_solver * pSat, int iVarA, int iVarB, int iVarEn)
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
}

void inline Lsv_AddClauseForPi(Cnf_Dat_t* pCnf, sat_solver* pSat, Aig_Man_t* pAigMgr, int nInitVars){
  // num of PI = m
  // Var1 Var2 Var3 ... Varn alpha1 beta1 alpha2 beta2 alpha3 beta3 ... alpham betam
  int alpha  = 3*(nInitVars)+1,
      //let beta = alpha+1 
      id1st, id2nd, id3rd, j;

  Aig_Obj_t* pAigCi;
  Aig_ManForEachCi( pAigMgr, pAigCi, j ){
    id3rd = pCnf->pVarNums[pAigCi->Id];
    id2nd = id3rd - nInitVars;
    id1st = id2nd - nInitVars;

    //X X' alpha
    Lsv_AddClauseForEq( pSat, id1st, id2nd, alpha++);

    //X X" beta
    Lsv_AddClauseForEq( pSat, id1st, id3rd, alpha++);    
  }
}

int Lsv_CommandPrintBior(Abc_Frame_t* pAbc, int argc, char** argv) {

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc), *pPoNtk;
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  Aig_Man_t* pAigMgr;
  Cnf_Dat_t * pCnf;
  sat_solver* pSat;

  Abc_Obj_t* pPoNode;

  int i, j, k;
  Abc_NtkForEachPo(pNtk, pPoNode, i){
    //Create Cone for each Po
    pPoNtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPoNode), Abc_ObjName(pPoNode), 0);
    if( Abc_ObjFaninC0(pPoNode) ) Abc_ObjXorFaninC( Abc_NtkPo(pPoNtk, 0), 0 );
    
    //only for combinational ckt, hence fRegisters=0?
    pAigMgr = Abc_NtkToDar(pPoNtk, 0, 0);

    //cnf for f(X)
    pCnf = Cnf_Derive(pAigMgr, 1);
    //Cnf_DataPrint( pCnf, 1);

    //initialize sat solver with cnf of f(X)
    pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    
    const int nInitVars = pCnf->nVars,
              nCi = Aig_ManCiNum(pAigMgr);
    //std::cout<<"vars: "<<nInitVars<<" pi: "<<nCi<<std::endl;
    sat_solver_setnvars(pSat, 3*nInitVars+2*nCi);

    //aruge f(X)=1
    Lsv_AddClauseForPo(pCnf, pSat, pAigMgr, 0);

    //add f(X') to the solver and argue f(X')=0
    Lsv_LiftAndAddClause(pCnf, pSat, nInitVars);
    Lsv_AddClauseForPo(pCnf, pSat, pAigMgr, 1);

    //add f(X") to the solver and argue f(X")=0
    Lsv_LiftAndAddClause(pCnf, pSat, nInitVars);
    Lsv_AddClauseForPo(pCnf, pSat, pAigMgr, 1);

    //add controlling clauses with alpha[j] and beta[j]
    Lsv_AddClauseForPi(pCnf, pSat, pAigMgr, nInitVars);

    //initialize unit assumptions (all alpha,beta=0,0)
    int Lits[2*nCi];
    for(j=0; j<2*nCi; ++j){
      Lits[j] = toLitCond( (3*nInitVars + j + 1), 1 );
    }

    lbool status=l_Undef;
    for(j=1; j<nCi; ++j){
      for(k=0; k<j; ++k){
        //set unit assumption (one alpha,beta=1,0, one alpha,beta=0,1)
        Lits[ 2*j   ]--; //alphaj=1
        Lits[(2*k)+1]--; //betak=1

        //TODO: need to cancel assumption(i.e. change seed) if sat?
        status = sat_solver_solve(pSat, Lits, Lits+2*nCi, 0, 0, 0, 0);
        //std::cout<<"status: "<<(int)status<<std::endl;
    
        //reset unit assumption to all alpha,beta=0,0
        Lits[ 2*j   ]++; //alphaj=1
        Lits[(2*k)+1]++; //betak=1

        assert(status!=l_Undef);
        if(status==l_False){ //unsat -> found some XA | XB | XC
          break;
        }
      }
      if(status==l_False){ break; }
    }

    if(status==l_False){
      //unsat -> print
      std::cout<<"PO "<<Abc_ObjName(pPoNode)<<" support partition: 1"<<std::endl;

      //now Lits is alpha,beta
      //initialize to all 1
      for(j=0; j<2*nCi; ++j){ Lits[j] = 1; }

      int *pClauses, *pLit, *pStop;
      int nFinalClause = sat_solver_final(pSat, &pClauses);
      //std::cerr<<"nFinalClauses: "<<nFinalClause<<std::endl;
      //assert at least two 1 in Lits
      assert(nFinalClause<((2*nCi)-1));

      for(j=0; j<nFinalClause; ++j){
        for ( pLit = &pClauses[j], pStop = &pClauses[j+1]; pLit < pStop; pLit++ ){
          assert(!Abc_LitIsCompl(*pLit));
          assert(Abc_Lit2Var(*pLit)>nInitVars*3);
          Lits[Abc_Lit2Var(*pLit)-(nInitVars*3)-1]=0;
          //std::cout<<Abc_Lit2Var(*pLit)<<" ";
        } //std::cout<<std::endl;
      }

      //just try to make XA and XB balance
      int nXA=0, nXB=0;
      for(j=0; j<nCi; ++j){
        if     (Lits[(2*j)]==0 && Lits[(2*j)+1]==1) std::cout<<"1"; //alpha=0, beta=1 => XB
        else if(Lits[(2*j)]==1 && Lits[(2*j)+1]==0) std::cout<<"2"; //alpha=1, beta=0 => XA
        else if(Lits[(2*j)]==0 && Lits[(2*j)+1]==0) std::cout<<"0"; //alpha=0, beta=0 => XC
        else if(nXA>nXB){ std::cout<<"1"; ++nXB; } 
        else            { std::cout<<"2"; ++nXA; }  
      }
      std::cout<<std::endl;
    }
    else{
      //sat -> print
      assert(status==l_True || nCi<=1);
      std::cout<<"PO "<<Abc_ObjName(pPoNode)<<" support partition: 0"<<std::endl;
    }
  }
  return 0;
}

#endif
