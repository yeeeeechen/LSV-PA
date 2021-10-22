#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
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
    printf("Object Id = %d, name = %s\n, #FO = %d", Abc_ObjId(pObj), Abc_ObjName(pObj), Abc_ObjFanoutNum(pObj));
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

// compare using the obj Id, assuming vector of Abc_Obj_t*
static int Lsv_VecPtrSortCompare1( void ** pp1, void ** pp2 ) {
  if ( Abc_ObjId((Abc_Obj_t*)(*pp1)) < Abc_ObjId((Abc_Obj_t*)(*pp2)))
    return -1;
  if ( Abc_ObjId((Abc_Obj_t*)(*pp1)) > Abc_ObjId((Abc_Obj_t*)(*pp2))) 
    return 1;
  return 0; 
}

// compare using the first obj Id, assuming vector of (vector of Abc_Obj_t*)
static int Lsv_VecPtrSortCompare2(void ** pp1, void ** pp2) {
  if( Abc_ObjId((Abc_Obj_t*)Vec_PtrEntry((Vec_Ptr_t*)(*pp1), 0)) <
      Abc_ObjId((Abc_Obj_t*)Vec_PtrEntry((Vec_Ptr_t*)(*pp2), 0))) {
    return -1;
  }
  if( Abc_ObjId((Abc_Obj_t*)Vec_PtrEntry((Vec_Ptr_t*)(*pp1), 0)) >
      Abc_ObjId((Abc_Obj_t*)Vec_PtrEntry((Vec_Ptr_t*)(*pp2), 0))) {
    return 1;
  }
  return 0; 
}


void Lsv_AigMSFC_rec(Abc_Obj_t * pNode, Vec_Ptr_t * vNodes, Vec_Ptr_t* msfc, bool isRoot) {
    Abc_Obj_t * pFanin;
    int i;

    if ( Abc_NodeIsTravIdCurrent(pNode) || pNode->fMarkA==1) { return; }
    // mark the node as visited
    Abc_NodeSetTravIdCurrent(pNode);
    // skip the PI
    if (Abc_ObjIsCi(pNode)) { return ;}
    // push node with multiple fanout to the back of vNodes
    if (Abc_ObjFanoutNum(pNode) > 1 && !isRoot) {
      //printf("%s has %d fanout\n", Abc_ObjName(pNode), Abc_ObjFanoutNum(pNode));
      Vec_PtrPush(vNodes, pNode);
      return;
    }

    // visit the transitive fanin of the node
    Abc_ObjForEachFanin(pNode, pFanin, i)
      Lsv_AigMSFC_rec(pFanin, vNodes, msfc, 0);
    // visit the equivalent nodes
    // if ( Abc_AigNodeIsChoice( pNode ) )
    //     for ( pFanin = (Abc_Obj_t *)pNode->pData; pFanin; pFanin = (Abc_Obj_t *)pFanin->pData )
    //         Abc_AigDfs_rec( pFanin, vNodes );
    // add the node after the fanins have been added
    if(Abc_ObjIsPo(pNode)) { return; }
    Vec_PtrPush(msfc, pNode);
    pNode->fMarkA = 1;
}

Vec_Ptr_t * Lsv_AigMSFC(Abc_Ntk_t * pNtk) {
  Vec_Ptr_t * msfcVec;
  Vec_Ptr_t * vNodes;
  Abc_Obj_t * pNode;
  int i;
  assert(Abc_NtkIsStrash(pNtk));
  Abc_NtkCleanMarkA(pNtk);

  vNodes = Vec_PtrAlloc(100);
  msfcVec = Vec_PtrAlloc(100);
  // go through the PO nodes and call for each of them
  Abc_NtkForEachCo(pNtk, pNode, i) {Vec_PtrPush(vNodes, pNode);}
  Vec_PtrForEachEntry(Abc_Obj_t*, vNodes, pNode, i) {
    Abc_NtkIncrementTravId(pNtk);
    Vec_Ptr_t * msfc = Vec_PtrAlloc(50);
    Lsv_AigMSFC_rec(pNode, vNodes, msfc, 1);
    Vec_PtrSort(msfc, (int (*)(const void *, const void *)) Lsv_VecPtrSortCompare1);
    if(Vec_PtrSize(msfc)>0)
      Vec_PtrPush(msfcVec, (void*)msfc);
  }
  Abc_NtkCleanMarkA(pNtk); 
  return msfcVec;
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  Vec_Ptr_t* msfcVec;
  Vec_Ptr_t* pVec;
  Abc_Obj_t* pNode;
  int i, j;
  msfcVec = Lsv_AigMSFC(pNtk);
  Vec_PtrSort(msfcVec, (int (*)(const void *, const void *)) Lsv_VecPtrSortCompare2);
  Vec_PtrForEachEntry(Vec_Ptr_t*, msfcVec, pVec, i) {
    printf("MSFC %d: ", i);
    Vec_PtrForEachEntry(Abc_Obj_t*, pVec, pNode, j) {
      printf("%s", Abc_ObjName(pNode));
      if(j != Vec_PtrSize(pVec) - 1) printf(",");
    }
    printf("\n");
  }
}

int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  assert(Abc_NtkIsStrash(pNtk));
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
  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the sorted MSFC group in the aig\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

