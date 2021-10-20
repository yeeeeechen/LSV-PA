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

void Lsv_NodeMfscCone_rec(Abc_Obj_t* pNode, Vec_Ptr_t* vCone, int fTopmost) {
    Abc_Obj_t* pFanin;
    int i;

    if (Abc_NodeIsTravIdCurrent(pNode)) {
        return;
    }
    Abc_NodeSetTravIdCurrent(pNode);

    if (!fTopmost && (Abc_ObjIsCi(pNode) || pNode->vFanouts.nSize > 1)) {
        return;
    }

    Abc_ObjForEachFanin(pNode, pFanin, i) {
        Lsv_NodeMfscCone_rec(pFanin, vCone, 0);
    }
    //
    if (vCone) Vec_PtrPush(vCone, pNode);
}

void Lsv_NodeMsfcCone(Abc_Obj_t* pNode, Vec_Ptr_t* vCone) {
    //compute MSFC and store them in vCone
    assert(Abc_ObjIsNode(pNode) || Abc_ObjIsPo(pNode));
    assert(!Abc_ObjIsComplement(pNode));
    if (vCone) Vec_PtrClear(vCone);
    Abc_NtkIncrementTravId(pNode->pNtk);
    Lsv_NodeMfscCone_rec(pNode, vCone, 1);
}

void Lsv_NodeMfscConePrint(Abc_Obj_t* pNode) {
    Vec_Ptr_t* vCone;
    Abc_Obj_t* pObj; 
    int i;
    vCone = Vec_PtrAlloc(100);
    Abc_NodeDeref_rec(pNode);
    Lsv_NodeMsfcCone(pNode, vCone);
    Abc_NodeRef_rec(pNode);
    //TODO: Sort the nodes in cone (how?)
    Vec_PtrForEachEntry(Abc_Obj_t*, vCone, pObj, i) {
        printf("%s", Abc_ObjName(pObj));
        if (i != vCone->nSize - 1) {
            printf(",");
        }
    }
    printf("\n");
}

void Lsv_NtkPrintMsfc(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    int i = 0, j;
    Abc_NtkForEachObj(pNtk, pObj, j) {
        /*  Output Reference
            MSFC 0: n8,n9,n10
            MSFC 1: n11
            MSFC 2: n12
            MSFC 3: n13,n14,n15,n17,n18,n19,n20
            MSFC 4: n16
            MSFC 5: n21,n22,n23,n24
        */
        printf("MSFC %d: ", i);
        if (pObj->vFanins.nSize > 1 || Abc_ObjIsPo(pObj)) {
            printf("%s", Abc_ObjName(pObj));
            Lsv_NodeMfscConePrint(pObj);
            i++;
        }
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