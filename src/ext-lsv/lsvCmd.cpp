#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include "aig/aig/aig.h"
#include "sat/cnf/cnf.h"
#include <vector>

extern "C" {
	Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
	void Abc_NtkShow( Abc_Ntk_t * pNtk0, int fGateNames, int fSeq, int fUseReverse );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandPrintMsfc(Abc_Frame_t *pAbc, int argc, char **argv);
static int Lsv_CommandOrBidec(Abc_Frame_t *pAbc, int argc, char **argv);

void init(Abc_Frame_t *pAbc)
{
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
}

void destroy(Abc_Frame_t *pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager
{
	PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintNodes(Abc_Ntk_t *pNtk)
{
	Abc_Obj_t *pObj;
	int i;
	Abc_NtkForEachNode(pNtk, pObj, i)
	{
		printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
		Abc_Obj_t *pFanin;
		int j;
		Abc_ObjForEachFanin(pObj, pFanin, j)
		{
			printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
				   Abc_ObjName(pFanin));
		}
		if (Abc_NtkHasSop(pNtk))
		{
			printf("The SOP of this node:\n%s", (char *)pObj->pData);
		}
	}
}

int Lsv_CommandPrintNodes(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
	{
		switch (c)
		{
		case 'h':
			goto usage;
		default:
			goto usage;
		}
	}
	if (!pNtk)
	{
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

/**********************************************************************
*                             PA1: MSFC                              *
**********************************************************************/

int Lsv_compareObjId(Abc_Obj_t **pNode1, Abc_Obj_t **pNode2)
{
	return (int)Abc_ObjId(*pNode1) - (int)Abc_ObjId(*pNode2);
}
int Lsv_compareFirstObjId(Vec_Ptr_t **pList1, Vec_Ptr_t **pList2)
{
	return (int)Abc_ObjId((Abc_Obj_t *)Vec_PtrEntry(*pList1, 0)) - (int)Abc_ObjId((Abc_Obj_t *)Vec_PtrEntry(*pList2, 0));
}
void Lsv_NtkPrintMsfc(Abc_Ntk_t *pNtk)
{
	Abc_Obj_t *pObj;
	Abc_Obj_t *pNode;
	Abc_Obj_t *pFanin;
	int i;
	Vec_Vec_t *pHeads;
	Vec_Ptr_t *pList;
	Vec_Ptr_t *pCheck;

	pHeads = Vec_VecAlloc(0);
	pCheck = Vec_PtrAlloc(8);

	Abc_NtkForEachNode(pNtk, pNode, i)
		pNode->iTemp = 0;

	Abc_NtkForEachPo(pNtk, pObj, i)
	{
		pNode = Abc_ObjFanin0(pObj);
		if (!Abc_ObjIsPi(pNode) && pNode->iTemp == 0)
		{
			pNode->iTemp = 1;
			Vec_VecPush(pHeads, Vec_VecSize(pHeads), pNode);
			// printf( "node %d pushed to list %d\n", Abc_ObjId(pNode), Vec_VecSize(pHeads)-1 );
		}
	}
	// printf( "total heads from po: %d\n", Vec_VecSize(pHeads) );

	int idx = 0;

	while (idx < Vec_VecSize(pHeads))
	{
		pList = Vec_VecEntry(pHeads, idx);
		// printf( "list %d have size %d\n", idx, Vec_PtrSize( pList ) );

		pNode = (Abc_Obj_t *)Vec_PtrEntry(pList, 0);

		Vec_PtrPush(pCheck, pNode);

		while (Vec_PtrSize(pCheck))
		{
			pNode = (Abc_Obj_t *)Vec_PtrPop(pCheck);

			// check it's fanins, if has fanout = 1, add to list and check
			// else if iTemp = 0, set iTemp to 1 and add to heads
			Abc_ObjForEachFanin(pNode, pFanin, i)
			{
				if (pFanin && !Abc_ObjIsPi(pFanin))
				{
					if (Abc_ObjFanoutNum(pFanin) == 1)
					{
						Vec_PtrPush(pList, pFanin);
						Vec_PtrPush(pCheck, pFanin);
					}
					else if (pFanin->iTemp == 0)
					{
						pFanin->iTemp = 1;
						Vec_VecPush(pHeads, Vec_VecSize(pHeads), pFanin);
						// printf("new %d head: %d\n", Vec_VecSize(pHeads)-1 ,Abc_ObjId( pFanin) );
						// Vec_PtrPush( Vec_VecEntry(pHeads, Vec_VecSize(pHeads)-1), pNode );
					}
				}
			}
		}

		// sort current list
		Vec_PtrSort(pList, (int (*)(const void *, const void *))Lsv_compareObjId);

		// next head
		idx++;
	}

	Vec_Ptr_t *pLists = Vec_PtrAlloc(8);
	Vec_VecForEachLevel(pHeads, pList, i)
	{
		Vec_PtrPush(pLists, pList);
	}
	Vec_PtrSort(pLists, (int (*)(const void *, const void *))Lsv_compareFirstObjId);

	int j;

	Vec_PtrForEachEntry(Vec_Ptr_t *, pLists, pList, j)
	{
		printf("MSFC %d: ", j);
		Vec_PtrForEachEntry(Abc_Obj_t *, pList, pNode, i)
		{
			if (i == 0)
				printf("%s", Abc_ObjName(pNode));
			else
				printf(",%s", Abc_ObjName(pNode));
		}

		printf("\n");
	}
}
int Lsv_CommandPrintMsfc(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
	{
		switch (c)
		{
		case 'h':
			goto usage;
		default:
			goto usage;
		}
	}
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}

	Lsv_NtkPrintMsfc(pNtk);

	return 0;

usage:
	Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
	Abc_Print(-2, "\t        prints the MSFCs in the network\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}

/**********************************************************************
*                             PA2                                     *
**********************************************************************/


void Lsv_OrBidec( Abc_Ntk_t *pNtk ) {

	Abc_Obj_t *pCo;

	int nPI = Abc_NtkPiNum( pNtk );

	int i;
	Abc_NtkForEachCo( pNtk, pCo, i) {

		Abc_Ntk_t *pCone; 
		Aig_Man_t *pAig;
		Cnf_Dat_t *pCnf;
		sat_solver* pSat;

		printf( "PO %s support partition: ", Abc_ObjName(pCo) );

		// convert CNF
		pCone = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pCo), Abc_ObjName(pCo), 1 );
        if ( Abc_ObjFaninC0(pCo) )
            Abc_ObjXorFaninC( Abc_NtkPo(pCone, 0), 0 );

		pAig = Abc_NtkToDar( pCone, 0, 0 );
		pCnf = Cnf_Derive( pAig, 1 );
		
		// get PI PO var
		int varPO = Vec_IntGetEntry( Cnf_DataCollectCoSatNums( pCnf, pAig ), 0 );
		Vec_Int_t* pVarPIs = Cnf_DataCollectCiSatNums( pCnf, pAig );

		// create sat solver
		pSat = (sat_solver*)Cnf_DataWriteIntoSolver( pCnf, 1, 0 );
		int nVarF = sat_solver_nvars( pSat );

		Cnf_DataLift( pCnf, nVarF );
		int i_c;
		lit *pBeg, *pEnd;
		Cnf_CnfForClause( pCnf, pBeg, pEnd, i_c ) sat_solver_addclause( pSat, pBeg, pEnd );
		Cnf_DataLift( pCnf, nVarF );
		Cnf_CnfForClause( pCnf, pBeg, pEnd, i_c ) sat_solver_addclause( pSat, pBeg, pEnd );

		//  add x=x', x=x''
		sat_solver_setnvars( pSat, sat_solver_nvars(pSat) + 2 * nPI );

		int j, varPI;
		int varAlpha = 3 * nVarF + 1;
		int varBeta = varAlpha + nPI;
		Vec_IntForEachEntry( pVarPIs, varPI, j) {

			sat_solver_add_buffer_enable( pSat, varPI, varPI + nVarF, varAlpha+j, 0 );
			sat_solver_add_buffer_enable( pSat, varPI, varPI + nVarF + nVarF, varBeta+j, 0 );

		}

		// assume F(X), -F(X'), -F(X''), set partition seed
		lit assumpList[nPI*2+3];
		assumpList[nPI*2] = toLitCond(varPO, 0);
		assumpList[nPI*2+1] = toLitCond(varPO + nVarF, 1);
		assumpList[nPI*2+2] = toLitCond(varPO + 2*nVarF, 1);

		for( int k = 0; k < nPI; k++ ) {
			assumpList[k] = toLitCond( varAlpha + k, 0 );
			assumpList[k+nPI] = toLitCond( varBeta + k, 0 );
		}

		int status = l_True;
		int seed_a, seed_b;
		for( int k = 0; k < nPI-1; k++ ) {

			// move xk to A
			assumpList[k] = toLitCond( varAlpha + k, 1 );

			for ( int l = k+1; l < nPI; l++ ) {

				// move xl to B
				assumpList[l+nPI] = toLitCond( varBeta + l, 1);

				// solve
				status = sat_solver_solve( pSat, assumpList, assumpList + nPI + nPI + 3, 0, 0, 0, 0);
				if ( status == l_False ) {
					seed_b = l;
					break;
				}

				// move xl back to C
				assumpList[l+nPI] = toLitCond( varBeta + l, 0);
			}
			if ( status == l_False ) {
				seed_a = k;
				break;
			}

			// move xk back to C
			assumpList[k] = toLitCond( varAlpha + k, 0 );
		}


		// if unsat
		if ( status == l_False ) {

			int nFinal;
			int* pFinal;

			bool alpha[nPI];
			bool beta[nPI];
			for( int k = 0; k < nPI; k++ ) {
				alpha[k] = k == seed_a;
				beta[k] = k == seed_b;
			}

			nFinal = sat_solver_final( pSat, &pFinal );
			printf( "1\n");
			// printf( "nPI=%d   nVarF=%d   varPO=%d   seedA=%d  seedB=%d  nFinal=%d\n pFinal: ", nPI, nVarF, varPO, seed_a, seed_b, nFinal );

			for( int k = 0; k < nFinal; k++ )
			{
				if ( lit_var( pFinal[k] ) - nVarF * 3 < 1 ) continue;
				bool isAlpha = ((lit_var(pFinal[k])-nVarF*3-1) / nPI) == 0;
				int idx = (lit_var(pFinal[k])-nVarF*3-1)%nPI;
				if ( isAlpha ) {
					alpha[idx] = !lit_sign( pFinal[k] );
				}
				else {
					beta[idx] = !lit_sign( pFinal[k] );
				}

				// printf( "%3s%d=%1d",  isAlpha ? "a" : "b", idx , lit_sign( pFinal[k] )  );

			}
			// printf("\n");

			for( int k = 0; k < nPI; k++ ) {

				if ( alpha[k] ) {
					printf("2");
				}
				else {
					if ( beta[k] ) printf("1");
					else printf("0");
				}
			}
			printf("\n");

		}
		else {
			printf( "0\n");
		}

	}
}


// register command
int Lsv_CommandOrBidec(Abc_Frame_t *pAbc, int argc, char **argv)
{
	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	int c;
	Extra_UtilGetoptReset();
	while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
	{
		switch (c)
		{
		case 'h':
			goto usage;
		default:
			goto usage;
		}
	}
	if (!pNtk)
	{
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}

	Lsv_OrBidec( pNtk );

	return 0;

usage:
	Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
	Abc_Print(-2, "\t        prints the MSFCs in the network\n");
	Abc_Print(-2, "\t-h    : print the command usage\n");
	return 1;
}