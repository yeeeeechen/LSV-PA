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

// sort method used by Vec_PtrSort
static int Vec_PtrSortCompareObjIds( void ** pp1, void ** pp2 ) {   
    Abc_Obj_t* pObj1 = (Abc_Obj_t*) *pp1;
    Abc_Obj_t* pObj2 = (Abc_Obj_t*) *pp2;
    
    if ( Abc_ObjId(pObj1) < Abc_ObjId(pObj2) )
        return -1;
    if ( Abc_ObjId(pObj1) > Abc_ObjId(pObj2) ) 
        return 1;
    return 0; 
}

static int Vec_PtrSortCompareFirstObjIdInCone( void ** pp1, void ** pp2 ) {   
    Abc_Obj_t* pObj1 = (Abc_Obj_t*) Vec_PtrEntry((Vec_Ptr_t*) *pp1, 0);
    Abc_Obj_t* pObj2 = (Abc_Obj_t*) Vec_PtrEntry((Vec_Ptr_t*) *pp2, 0);
    if ( Abc_ObjId(pObj1) < Abc_ObjId(pObj2) )
        return -1;
    if ( Abc_ObjId(pObj1) > Abc_ObjId(pObj2) ) 
        return 1;
    return 0; 
}

void Lsv_NodeMfsc_rec(Abc_Obj_t* pNode, Vec_Ptr_t* vIterateList, Vec_Ptr_t* vCone, int fTopmost) {
    Abc_Obj_t* pFanin;
    int i;

    if (pNode->fMarkA == 1) {
        return;
    }
    // Abc_NodeSetTravIdCurrent(pNode);

    if (Abc_ObjIsCi(pNode)) {
        return;
    }
    if (!fTopmost && Abc_ObjFanoutNum(pNode) > 1) {
        Vec_PtrPush(vIterateList, pNode);
        return;
    }

    Abc_ObjForEachFanin(pNode, pFanin, i) {
        Lsv_NodeMfsc_rec(pFanin, vIterateList, vCone, 0);
    }
    //
    if (vCone && !Abc_ObjIsCo(pNode)) Vec_PtrPush(vCone, pNode);
    pNode->fMarkA = 1; 
}

void Lsv_NodeMsfc(Abc_Obj_t* pNode, Vec_Ptr_t* vIterateList, Vec_Ptr_t* vCone) {
    // compute MSFC and store them in vCone
    assert(!Abc_ObjIsComplement(pNode));
    if (vCone) Vec_PtrClear(vCone);
    // Abc_NtkIncrementTravId(pNode->pNtk);
    Lsv_NodeMfsc_rec(pNode, vIterateList, vCone, 1);
}

void Lsv_NtkMsfc(Abc_Ntk_t* pNtk, Vec_Ptr_t* vMsfc) {
    Abc_Obj_t* pObj;
    Vec_Ptr_t* vObj = Vec_PtrAlloc(50);
    int i;
    Abc_NtkForEachCo(pNtk, pObj, i) {
        Vec_PtrPush(vObj, pObj);
    }
    Vec_PtrForEachEntry(Abc_Obj_t*, vObj, pObj, i) {
        Vec_Ptr_t* vCone = Vec_PtrAlloc(100);
        Lsv_NodeMsfc(pObj, vObj, vCone);
        Vec_PtrSort(vCone, (int (*)(const void *, const void *)) Vec_PtrSortCompareObjIds);
        if (Vec_PtrSize(vCone) > 0) {
            Vec_PtrPush(vMsfc, (void*)vCone);
        }
    }

    Vec_PtrSort(vMsfc, (int (*)(const void *, const void *)) Vec_PtrSortCompareFirstObjIdInCone);
}

void Lsv_NtkPrintMsfc(Abc_Ntk_t* pNtk) {
    Vec_Ptr_t* vMsfc = Vec_PtrAlloc(100), *pOneMsfc;

    Abc_Obj_t* pObj;
    int i, j;
    Lsv_NtkMsfc(pNtk, vMsfc);
    Vec_PtrForEachEntry(Vec_Ptr_t*, vMsfc, pOneMsfc, i) {
        printf("MSFC %d: ", i);
        Vec_PtrForEachEntry(Abc_Obj_t*, pOneMsfc, pObj, j) {
            printf("%s", Abc_ObjName(pObj));
            if (j != pOneMsfc->nSize - 1) {
                printf(",");
            }
        }
        printf("\n");
    }
    Abc_NtkCleanMarkA(pNtk);
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
    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkPrintMsfc(pNtk);
    return 0;
usage:
    Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
    Abc_Print(-2, "\t        prints the maximum single-fanout cone (MSFC) decomposition of the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}