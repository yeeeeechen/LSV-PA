#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include <set>
#include <vector>
#include <list>
#include <queue>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <map>
#include <typeinfo>

using namespace std;
extern "C"  Aig_Man_t * Abc_NtkToDar(Abc_Ntk_t * pNtk,int fExors,int fRegisters);
  

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_CommandA(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandA, 0);

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
bool cmp1(std::pair<int,int>a,std::pair<int,int>b)
{
    return a.first < b.first;
}


int Lsv_CommandA(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    
    Abc_Obj_t* pObj;
    Abc_Ntk_t * pNtkOn1;
    sat_solver * pSat;
    Cnf_Dat_t * pCnf;
    Aig_Man_t* pMan;
    int i;
    Abc_NtkForEachPo(pNtk,pObj,i){
      
      pNtkOn1 = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0 );
      if ( Abc_ObjFaninC0(pObj) )
          Abc_ObjXorFaninC( Abc_NtkPo(pNtkOn1, 0), 0 );
      pMan = Abc_NtkToDar(pNtkOn1,0,0);
      pCnf = Cnf_Derive( pMan, 1 );
      pSat = (sat_solver *)Cnf_DataWriteIntoSolver(pCnf,1,0);
      const int nInitVars = pCnf->nVars;//varshift
      const int nCi = Aig_ManCiNum(pMan);
      sat_solver_setnvars(pSat, 3*nInitVars+2*nCi);


      
      Aig_Obj_t * pObj_Ci;
      
      
      
      int j;
      
      
      
      
      
      lit Lits[3];
     
      int PO_var_num = pCnf->pVarNums[Aig_ManCo(  pMan,0 )->Id];
      Lits[0] = toLitCond(PO_var_num,0);
      sat_solver_addclause(pSat,Lits,Lits+1);//FX

      
      Cnf_DataLift(pCnf,nInitVars);
      for (int r = 0; r < pCnf->nClauses; r++ )
      sat_solver_addclause( pSat, pCnf->pClauses[r], pCnf->pClauses[r+1] );//FX
      int PO_var_num1 = pCnf->pVarNums[Aig_ManCo(  pMan,0 )->Id];
      Lits[0] = toLitCond(PO_var_num1,1);
      sat_solver_addclause(pSat,Lits,Lits+1);//FX'
      Cnf_DataLift(pCnf,nInitVars);
      for (int r = 0; r < pCnf->nClauses; r++ )
      sat_solver_addclause( pSat, pCnf->pClauses[r], pCnf->pClauses[r+1] );//FX'
      int PO_var_num2 = pCnf->pVarNums[Aig_ManCo(  pMan,0 )->Id];
      Lits[0] = toLitCond(PO_var_num2,1);
      sat_solver_addclause(pSat,Lits,Lits+1);//FX''
      int alpha  = 3*(nInitVars)+1,
          
          id1st, id2nd, id3rd;

      
      Aig_ManForEachCi( pMan, pObj_Ci, j ){//Ci
        id3rd = pCnf->pVarNums[pObj_Ci->Id];
        id2nd = id3rd - nInitVars;
        id1st = id2nd - nInitVars;

        
        int Cid;
        lit Lits2 [4];
        Lits2[0] = toLitCond( id1st, 0 );
        Lits2[1] = toLitCond( id2nd, 1 );
        Lits2[2] = toLitCond( alpha, 0 );
        Cid = sat_solver_addclause( pSat, Lits2, Lits2 + 3 );
        assert( Cid );

        Lits2[0] = toLitCond( id1st, 1 );
        Lits2[1] = toLitCond( id2nd, 0 );
        Lits2[2] = toLitCond( alpha, 0 );
        Cid = sat_solver_addclause( pSat, Lits2, Lits2 + 3 );
        assert( Cid );
        alpha++;
        
        Lits2[0] = toLitCond( id1st, 0 );
        Lits2[1] = toLitCond( id3rd, 1 );
        Lits2[2] = toLitCond( alpha, 0 );
        Cid = sat_solver_addclause( pSat, Lits2, Lits2 + 3 );
        assert( Cid );

        Lits2[0] = toLitCond( id1st, 1 );
        Lits2[1] = toLitCond( id3rd, 0 );
        Lits2[2] = toLitCond( alpha, 0 );
        Cid = sat_solver_addclause( pSat, Lits2, Lits2 + 3 );
        assert( Cid );
        alpha++;
      }
      // for ( int m = 0;m< varshift;m++){
      //       Lits[0] = unit_assumption[m].first;
      //       Lits[1] = toLitCond(PI_var_num[m],1);
      //       Lits[2] = toLitCond(PI_var_num[m]+varshift,0);
      //       sat_solver_addclause(pSat,Lits,Lits+3);
      //       Lits[0] = unit_assumption[m].first;
      //       Lits[1] = toLitCond(PI_var_num[m],0);
      //       Lits[2] = toLitCond(PI_var_num[m]+varshift,1);
      //       sat_solver_addclause(pSat,Lits,Lits+3);
      //     }
      //     for (int m = 0;m< varshift;m++){
      //       Lits[0] = unit_assumption[m].second;
      //       Lits[1] = toLitCond(PI_var_num[m],1);
      //       Lits[2] = toLitCond(PI_var_num[m]+2*varshift,0);
      //       sat_solver_addclause(pSat,Lits,Lits+3);
      //       Lits[0] = unit_assumption[m].second;
      //       Lits[1] = toLitCond(PI_var_num[m],0);
      //       Lits[2] = toLitCond(PI_var_num[m]+2*varshift,1);
      //       sat_solver_addclause(pSat,Lits,Lits+3);
      //     }
      //Aig_ManForEachCi( pMan, pObj_Ci,  j ){
      //  PI_var_num[z] = pCnf->pVarNums[pObj_Ci->Id];
      //  z++;
      //}

      lit Lits1 [2*nCi];    
      for (int m = 0;m<2*nCi;++m){
        Lits1[m] = toLitCond( (3*nInitVars + m + 1), 1 );
      }





      //int varshift = sat_solver_nvars(pSat);
      //
      //
      //
      //int PO_var_num1 = PO_var_num + varshift;
      //int PO_var_num2 = PO_var_num1 + varshift;
      
      
      
      
      //Cnf_Dat_t * pCnf2 = Cnf_DataDup(pCnf);
      //Cnf_DataLift(pCnf2,varshift);
      //Cnf_DataLift(pCnf2,varshift);
      //Lits[0] = toLitCond(PO_var_num2,1);
      //sat_solver_addclause(pSat,Lits,Lits+1);
      lbool status = l_Undef;
      //std::vector<std::pair<int,int>> unit_assumption(varshift, std::make_pair(0, 0));//first = alpha ,second = beta;
      for (int k = 1;k < nCi;++k){
        for (int l = 0;l < k; ++l){
          Lits1[ 2*k   ]--; 
          Lits1[(2*l)+1]--; 
          
          status = sat_solver_solve(pSat,Lits1,Lits1+2*nCi,0,0,0,0);
          Lits1[ 2*k   ]++; 
          Lits1[(2*l)+1]++; 
          if (status == l_False){
            break;
          }
          
          
        }
        if (status == l_False){
            break;
        }
      }

      if(status==l_False){
        
        std::cout<<"PO "<<Abc_ObjName(pObj)<<" support partition: 1"<<std::endl;

        for(j=0; j<2*nCi; ++j){ Lits1[j] = 1; }

        int *pClauses, *vartolit, *end;
        int nFinalClause = sat_solver_final(pSat, &pClauses);
        
        

        for(j=0; j<nFinalClause; j++){
          for ( vartolit = &pClauses[j], end = &pClauses[j+1]; vartolit < end; vartolit++ ){
            Lits1[Abc_Lit2Var(*vartolit)-(nInitVars*3)-1]=0;
          } 
        }

        
        for(j=0; j<nCi; ++j){
          if     (Lits1[(2*j)]==0 && Lits1[(2*j)+1]==1) std::cout<<"1"; 
          else if(Lits1[(2*j)]==1 && Lits1[(2*j)+1]==0) std::cout<<"2"; 
          else  std::cout<<"0"; 
        }
        std::cout<<std::endl;
      }
      else{
        std::cout<<"PO "<<Abc_ObjName(pObj)<<" support partition: 0"<<std::endl;
      }
    }
    
    return 0;
}
