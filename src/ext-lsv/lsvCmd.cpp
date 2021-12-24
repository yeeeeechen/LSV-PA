#include <stdio.h>
#include "base/main/main.h"
#include "aig/aig/aig.h"
#include "aig/saig/saig.h"
#include <string.h>
#include "base/main/mainInt.h"
#include "base/abc/abc.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include "sat/bsat/satSolver2.h"
#include <vector>
#include <queue>
#include <algorithm>

ABC_NAMESPACE_IMPL_START

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandBIDEC(Abc_Frame_t* pAbc, int argc, char** argv);

Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters )
{
    Vec_Ptr_t * vNodes;
    Aig_Man_t * pMan;
    Aig_Obj_t * pObjNew;
    Abc_Obj_t * pObj;
    int i, nNodes, nDontCares;
    // make sure the latches follow PIs/POs
    if ( fRegisters ) 
    { 
        assert( Abc_NtkBoxNum(pNtk) == Abc_NtkLatchNum(pNtk) );
        Abc_NtkForEachCi( pNtk, pObj, i )
            if ( i < Abc_NtkPiNum(pNtk) )
            {
                assert( Abc_ObjIsPi(pObj) );
                if ( !Abc_ObjIsPi(pObj) )
                    Abc_Print( 1, "Abc_NtkToDar(): Temporary bug: The PI ordering is wrong!\n" );
            }
            else
                assert( Abc_ObjIsBo(pObj) );
        Abc_NtkForEachCo( pNtk, pObj, i )
            if ( i < Abc_NtkPoNum(pNtk) )
            {
                assert( Abc_ObjIsPo(pObj) );
                if ( !Abc_ObjIsPo(pObj) )
                    Abc_Print( 1, "Abc_NtkToDar(): Temporary bug: The PO ordering is wrong!\n" );
            }
            else
                assert( Abc_ObjIsBi(pObj) );
        // print warning about initial values
        nDontCares = 0;
        Abc_NtkForEachLatch( pNtk, pObj, i )
            if ( Abc_LatchIsInitDc(pObj) )
            {
                Abc_LatchSetInit0(pObj);
                nDontCares++;
            }
        if ( nDontCares )
        {
            Abc_Print( 1, "Warning: %d registers in this network have don't-care init values.\n", nDontCares );
            Abc_Print( 1, "The don't-care are assumed to be 0. The result may not verify.\n" );
            Abc_Print( 1, "Use command \"print_latch\" to see the init values of registers.\n" );
            Abc_Print( 1, "Use command \"zero\" to convert or \"init\" to change the values.\n" );
        }
    }
    // create the manager
    pMan = Aig_ManStart( Abc_NtkNodeNum(pNtk) + 100 );
    pMan->fCatchExor = fExors;
    pMan->nConstrs = pNtk->nConstrs;
    pMan->nBarBufs = pNtk->nBarBufs;
    pMan->pName = Extra_UtilStrsav( pNtk->pName );
    pMan->pSpec = Extra_UtilStrsav( pNtk->pSpec );
    // transfer the pointers to the basic nodes
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)Aig_ManConst1(pMan);
    Abc_NtkForEachCi( pNtk, pObj, i )
    {
        pObj->pCopy = (Abc_Obj_t *)Aig_ObjCreateCi(pMan);
        // initialize logic level of the CIs
        ((Aig_Obj_t *)pObj->pCopy)->Level = pObj->Level;
    }

    // complement the 1-values registers
    if ( fRegisters ) {
        Abc_NtkForEachLatch( pNtk, pObj, i )
            if ( Abc_LatchIsInit1(pObj) )
                Abc_ObjFanout0(pObj)->pCopy = Abc_ObjNot(Abc_ObjFanout0(pObj)->pCopy);
    }
    // perform the conversion of the internal nodes (assumes DFS ordering)
//    pMan->fAddStrash = 1;
    vNodes = Abc_NtkDfs( pNtk, 0 );
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pObj, i )
//    Abc_NtkForEachNode( pNtk, pObj, i )
    {
        pObj->pCopy = (Abc_Obj_t *)Aig_And( pMan, (Aig_Obj_t *)Abc_ObjChild0Copy(pObj), (Aig_Obj_t *)Abc_ObjChild1Copy(pObj) );
//        Abc_Print( 1, "%d->%d ", pObj->Id, ((Aig_Obj_t *)pObj->pCopy)->Id );
    }
    Vec_PtrFree( vNodes );
    pMan->fAddStrash = 0;
    // create the POs
    Abc_NtkForEachCo( pNtk, pObj, i )
        Aig_ObjCreateCo( pMan, (Aig_Obj_t *)Abc_ObjChild0Copy(pObj) );
    // complement the 1-valued registers
    Aig_ManSetRegNum( pMan, Abc_NtkLatchNum(pNtk) );
    if ( fRegisters )
        Aig_ManForEachLiSeq( pMan, pObjNew, i )
            if ( Abc_LatchIsInit1(Abc_ObjFanout0(Abc_NtkCo(pNtk,i))) )
                pObjNew->pFanin0 = Aig_Not(pObjNew->pFanin0);
    // remove dangling nodes
    nNodes = (Abc_NtkGetChoiceNum(pNtk) == 0)? Aig_ManCleanup( pMan ) : 0;
    if ( !fExors && nNodes )
        Abc_Print( 1, "Abc_NtkToDar(): Unexpected %d dangling nodes when converting to AIG!\n", nNodes );
//Aig_ManDumpVerilog( pMan, "test.v" );
    // save the number of registers
    if ( fRegisters )
    {
        Aig_ManSetRegNum( pMan, Abc_NtkLatchNum(pNtk) );
        pMan->vFlopNums = Vec_IntStartNatural( pMan->nRegs );
//        pMan->vFlopNums = NULL;
//        pMan->vOnehots = Abc_NtkConverLatchNamesIntoNumbers( pNtk );
        if ( pNtk->vOnehots )
            pMan->vOnehots = (Vec_Ptr_t *)Vec_VecDupInt( (Vec_Vec_t *)pNtk->vOnehots );
    }
    if ( !Aig_ManCheck( pMan ) )
    {
        Abc_Print( 1, "Abc_NtkToDar: AIG check has failed.\n" );
        Aig_ManStop( pMan );
        return NULL;
    }
    return pMan;
}

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandBIDEC, 0);
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

void Lsv_PrintMSFC(Abc_Ntk_t* pNtk){
  std::queue<Abc_Obj_t*> QOO;
  std::vector<int> msfcs;
  Abc_Obj_t* pObj;
  int i;
  int count(0);
  //printf(" %d \n ",Vec_PtrSize((pNtk)->vObjs));
  Abc_NtkForEachNode(pNtk, pObj, i) {
    //printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    
	//printf("  Fanout num: %d \n",Abc_ObjFanoutNum(pObj));
    
    if (Abc_NtkHasSop(pNtk)) {
      printf("The SOP of this node:\n%s", (char*)pObj->pData);
    }
	
	if(Abc_ObjFanoutNum(pObj) != 1 || Abc_ObjType(Abc_ObjFanout(pObj, 0)) != 7){
		while(!QOO.empty()) {
			QOO.pop();
		}
		msfcs.clear();
		QOO.push(pObj);
		
		while(!QOO.empty()){
			int j;
			Abc_Obj_t* pFanin;
			msfcs.push_back( Abc_ObjId(QOO.front()));
			
			Abc_ObjForEachFanin( QOO.front(), pFanin, j ){
				if(Abc_ObjFanoutNum(pFanin) == 1 && Abc_ObjType(pFanin) == 7){
					QOO.push(pFanin);
				}
			}
			QOO.pop();
		}
		std::sort(msfcs.begin(), msfcs.end());
		printf("MSFC %d: %s",count++,Abc_ObjName(Abc_NtkObj(pNtk, msfcs[0])));
		for(int k=1;k<msfcs.size();k++){
			printf(",%s", Abc_ObjName(Abc_NtkObj(pNtk, msfcs[k])));
		}
		printf("\n");
	}
  }
}

int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_PrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

void Lsv_BIDEC(Abc_Ntk_t* pNtk){
	Aig_Man_t* pMan;
	Aig_Obj_t* pAig;
	Abc_Ntk_t* pthiscone;
	Cnf_Dat_t* pCnf;
	Abc_Obj_t* pObj;
	sat_solver * pSat;
	int i, j, g, PO, clause_temp[3], *assumpList, status, final_conflict_size, *final_conflict, **report, count(0);
	std::vector<int> PI_varnum;
	std::vector<int> alpha;
	std::vector<int> beta;
	
	Abc_NtkForEachPo( pNtk, pObj, i ){
		pthiscone = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0 );
        if ( Abc_ObjFaninC0(pObj) )
            Abc_ObjXorFaninC( Abc_NtkPo(pthiscone, 0), 0 );
		
		pMan = Abc_NtkToDar( pthiscone, 0, 0 );
		pCnf = Cnf_Derive(pMan, Aig_ManCoNum(pMan));
		
		Aig_ManForEachCi(pMan, pAig, j){
			PI_varnum.push_back(pCnf->pVarNums[pAig->Id]);
		}
		PO = pCnf->pVarNums[Aig_ManCo(pMan,0)->Id];
		
		
		
		
		// if(!i){
			// for(j=0;j<pCnf->nClauses;j++){
				// for(int *c = pCnf->pClauses[j]; c < pCnf->pClauses[j+1]; c++){
					// printf("%d ",*c);
				// }
				// printf("\n");
			// }
			// printf("\n");
			// Aig_ManForEachObj( pMan, pAig, g ){}
			// printf("%d\n", g);
			// for(int h=0;h<j;h++){
				// printf("%d ", PI_varnum[h]);
			// }
		// }
		//F(X)
		pSat = (sat_solver *)Cnf_DataWriteIntoSolver( pCnf, 1, 0 );
		if ( pSat == NULL )
		{
			Cnf_DataFree( pCnf );
			continue;
		}
		clause_temp[0] = PO*2;
		sat_solver_addclause(pSat, clause_temp, clause_temp+1);
		
		
		//F(X')'
		Cnf_DataLift(pCnf, pCnf->nVars);
		clause_temp[0] = PO*2 + pCnf->nVars*2 + 1;
		sat_solver_addclause(pSat, clause_temp, clause_temp+1);
		// for(int l=0; l < pCnf->nLiterals; l++){
			// if(pCnf->pClauses[0][l]%2){
				// pCnf->pClauses[0][l] -= 1;
			// }
			// else{
				// pCnf->pClauses[0][l] += 1;
			// }
		// }
		//debug
		// if(!i){
			// for(j=0;j<pCnf->nClauses;j++){
				// for(int *c = pCnf->pClauses[j]; c < pCnf->pClauses[j+1]; c++){
					// printf("%d ",*c);
				// }
				// printf("\n");
			// }
		// }
		
		for (int p = 0; p < pCnf->nClauses; p++) {
			sat_solver_addclause(pSat, pCnf->pClauses[p], pCnf->pClauses[p + 1]);
		}
		
		//F(X'')'
		Cnf_DataLift(pCnf, pCnf->nVars);
		clause_temp[0] = PO*2 + pCnf->nVars*4 + 1;
		sat_solver_addclause(pSat, clause_temp, clause_temp+1);
		
		for (int p = 0; p < pCnf->nClauses; p++) {
			sat_solver_addclause(pSat, pCnf->pClauses[p], pCnf->pClauses[p + 1]);
		}
		
		//debug
		// if(!i){
			// for(j=0;j<pCnf->nClauses;j++){
				// for(int *c = pCnf->pClauses[j]; c < pCnf->pClauses[j+1]; c++){
					// printf("%d ",*c);
				// }
				// printf("\n");
			// }
		// }
		
		//if(!i){printf("%d ",pMan->nTruePis);}
		//if(!i){printf("%d ",pSat->size);}
		// if(!i){printf("\n");}
		
		//xi==xi' v ai
		for (int in= 0; in < pMan->nTruePis; in++){
			if(PI_varnum[in] != -1){
				int temp = sat_solver_addvar(pSat)*2;
				clause_temp[0] = 2*PI_varnum[in]; //Literal of xi
				clause_temp[1] = 2*PI_varnum[in] + 2*pCnf->nVars + 1; //Literal of n-xi'
				clause_temp[2] = temp; //ai
				// if(!i){printf("%d %d %d\n", clause_temp[0], clause_temp[1], clause_temp[2]);}
				sat_solver_addclause( pSat, clause_temp, clause_temp+3);
				clause_temp[0] = 2*PI_varnum[in] + 1; //Literal of n-xi
				clause_temp[1] = 2*PI_varnum[in] + 2*pCnf->nVars; //Literal of xi'
				clause_temp[2] = temp; //ai
				sat_solver_addclause( pSat, clause_temp, clause_temp+3);
				alpha.push_back(clause_temp[2]);
				// if(!i){printf("%d %d %d\n", clause_temp[0], clause_temp[1], clause_temp[2]);}
			}
			else{
				alpha.push_back(0);
			}
		}
		
		// if(!i){printf("%d ",pSat->size);}
		//xi==xi'' v bi
		for (int in= 0; in < pMan->nTruePis; in++){
			if(PI_varnum[in] != -1){
				int temp = sat_solver_addvar(pSat)*2;
				clause_temp[0] = 2*PI_varnum[in]; //Literal of xi
				clause_temp[1] = 2*PI_varnum[in] + 4*pCnf->nVars + 1; //Literal of n-xi''
				clause_temp[2] = temp; //bi
				//if(!i){printf("%d %d %d\n", clause_temp[0], clause_temp[1], clause_temp[2]);}
				sat_solver_addclause( pSat, clause_temp, clause_temp+3);
				clause_temp[0] = 2*PI_varnum[in] + 1; //Literal of n-xi
				clause_temp[1] = 2*PI_varnum[in] + 4*pCnf->nVars; //Literal of xi''
				clause_temp[2] = temp;
				sat_solver_addclause( pSat, clause_temp, clause_temp+3);
				beta.push_back(clause_temp[2]);
				//if(!i){printf("%d %d %d\n", clause_temp[0], clause_temp[1], clause_temp[2]);}
			}
			else{
				beta.push_back(0);
			}
		}
		//if(!i){printf("%d \n",pSat->hLearnts);}
		
		//solve
		status = l_True;
		assumpList = new int[2*pMan->nTruePis];
		int ai,bi;
		for(ai=0; ai < pMan->nTruePis; ai++){
			for(bi=ai+1; bi < pMan->nTruePis; bi++){
				for(int in=0; in < pMan->nTruePis; in++){
					if(in==ai){assumpList[in] = alpha[in];}
					else{assumpList[in] = alpha[in]+1;}
					if(in==bi){assumpList[pMan->nTruePis + in] = beta[in];}
					else{assumpList[pMan->nTruePis + in] = beta[in]+1;}
				}
				// if(!i){
					// printf("\n");
					// for(int in=0;in<2*pMan->nTruePis;in++){
						// if(in == pMan->nTruePis){printf("\n");}
						// printf("%d ",*(assumpList+in));
					// }
				// }
				status = sat_solver_solve(pSat, assumpList, assumpList + 2 * pMan->nTruePis, 0,0,0,0);
				if(status == l_False){
					break;
				}
			}
			if(status == l_False){
				break;
			}
		}
		

		if(status == l_True){
			printf("PO <%s> support partition: 0\n",Abc_ObjName(pObj));
		}
		else{
			printf("PO <%s> support partition: 1\n",Abc_ObjName(pObj));
			final_conflict_size = sat_solver_final(pSat, &final_conflict);
			report = new int*[2];
			report[0] = new int[pMan->nTruePis]; //ai
			report[1] = new int[pMan->nTruePis]; //bi
			
			
			for(int cc=0; cc < pMan->nTruePis; cc++){
				report[0][cc] = 1;
				report[1][cc] = 1;
			}
			for(int k=0; k < final_conflict_size; k++){
				for(int cc=0; cc < pMan->nTruePis; cc++){
					if(alpha[cc] == final_conflict[k]){
						report[0][cc] = 0;
					}

					if(beta[cc] == final_conflict[k]){
						report[1][cc] = 0;
					}

					if(cc == ai){
						report[1][cc] = 0;
					}
					if(cc == bi){
						report[0][cc] = 0;
					}
				}
			}
			
			for(int cc=0; cc < pMan->nTruePis; cc++){
				if(report[0][cc]){printf("2");}
				else if(report[1][cc]){printf("1");}
				else{printf("0");}
			}
			// if(!i){
				// printf("\n");
				// for(int in=0; in<pMan->nTruePis; in++){
					// printf("%d",report[0][in]);
				// }
				// printf("\n");
				// for(int in=0; in<pMan->nTruePis; in++){
					// printf("%d",report[1][in]);
				// }
			// }
			printf("\n");
		}

		PI_varnum.clear();
		alpha.clear();
		beta.clear();
		count = 0;
		
	}
	printf("\n");
	
}

int Lsv_CommandBIDEC(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_BIDEC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
