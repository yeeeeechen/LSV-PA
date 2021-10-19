#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
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



/**********************************************************************
*                             PA1: MSFC                              *
**********************************************************************/

int Lsv_compareObjId( Abc_Obj_t** pNode1, Abc_Obj_t** pNode2 )
{
	return (int)Abc_ObjId(*pNode1) - (int)Abc_ObjId(*pNode2);
}
int Lsv_compareFirstObjId( Vec_Ptr_t** pList1, Vec_Ptr_t** pList2 )
{
	return (int)Abc_ObjId( (Abc_Obj_t*)Vec_PtrEntry( *pList1, 0 ) ) - (int)Abc_ObjId( (Abc_Obj_t*)Vec_PtrEntry( *pList2, 0 ) );
}

void Lsv_NtkPrintMsfc( Abc_Ntk_t* pNtk )
{
	Abc_Obj_t* pObj;
	Abc_Obj_t* pNode;
	Abc_Obj_t* pFanin;
	int i;
	Vec_Vec_t* pHeads;
	Vec_Ptr_t* pList;
	Vec_Ptr_t* pCheck;

	pHeads = Vec_VecAlloc(8);
	pCheck = Vec_PtrAlloc(8);

	Abc_NtkForEachNode( pNtk, pNode, i )
		pNode -> iTemp = 0;

 	Abc_NtkForEachPo( pNtk, pObj, i )
  	{
		pNode = Abc_ObjFanin0(pObj);
		pNode -> iTemp = 1;
		Vec_VecPush(pHeads, Vec_VecSize(pHeads), pNode );
  	}

	int idx = 0;

  	while( idx < Vec_VecSize( pHeads ) )
  	{
		pList = Vec_VecEntry( pHeads, idx );
		pNode = (Abc_Obj_t*)Vec_PtrEntry( pList, 0 );

		Vec_PtrPush( pCheck, pNode );

		while( Vec_PtrSize( pCheck ) )
		{
			pNode = (Abc_Obj_t*)Vec_PtrPop( pCheck );

			// check it's fanins, if has fanout = 1, add to list and check
			// else if iTemp = 0, set iTemp to 1 and add to heads
			Abc_ObjForEachFanin( pNode, pFanin, i )
			{
				if ( pFanin && ! Abc_ObjIsPi( pFanin) )
				{
					if ( Abc_ObjFanoutNum( pFanin ) == 1 )
					{
						Vec_PtrPush( pList, pFanin );
						Vec_PtrPush( pCheck, pFanin );
					}
					else if ( pFanin -> iTemp == 0 )
					{
						pFanin -> iTemp = 1;
						Vec_VecExpand( pHeads, 1 );
						Vec_VecPush(pHeads, Vec_VecSize(pHeads), pFanin );
						// printf("new head: %d\n", Abc_ObjId( pFanin) );
						// Vec_PtrPush( Vec_VecEntry(pHeads, Vec_VecSize(pHeads)-1), pNode );
					}
				}
			}
		}

		// sort current list
		Vec_PtrSort( pList, (int (*)(const void *, const void *))Lsv_compareObjId );

		// next head
		idx++;
	}


	Vec_Ptr_t* pLists = Vec_PtrAlloc(8);
	Vec_VecForEachLevel( pHeads, pList, i )
	{
		Vec_PtrPush( pLists, pList );
	}
	Vec_PtrSort( pLists, (int (*)(const void *, const void *))Lsv_compareFirstObjId );

	int j;

	Vec_PtrForEachEntry( Vec_Ptr_t*, pLists, pList, j )
	{
		printf("MSFC %d: ", j );
		Vec_PtrForEachEntry( Abc_Obj_t*, pList, pNode, i )
		{
			if ( i == 0 ) printf( "%s", Abc_ObjName(pNode) );
			else printf( ",%s", Abc_ObjName(pNode) );
		}
		
		printf("\n");
	}

}

int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  Lsv_NtkPrintMsfc(pNtk);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the MSFCs in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}