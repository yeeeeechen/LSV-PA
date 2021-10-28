#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

namespace
{
    int Lsv_CommandPrintMsfc(Abc_Frame_t *pAbc, int argc, char **argv);

    void init(Abc_Frame_t *pAbc)
    {
        Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
    }

    void destroy(Abc_Frame_t *pAbc) {}

    Abc_FrameInitializer_t frame_initializer = {init, destroy};

    struct PackageRegistrationManager
    {
        PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
    } lsvPackageRegistrationManager;

    // record nodes with multiple FOs
    void Lsv_FindHeads_Rec(Abc_Obj_t *pNode, Vec_Ptr_t *vNodes)
    {
        Abc_Obj_t *pFanin;
        int i;
        if (Abc_ObjIsCi(pNode))
            return;
        // if this node is already visited, it is a head
        if (Abc_NodeIsTravIdCurrent(pNode))
        {
            if (!(pNode->fMarkA))
            {
                pNode->fMarkA = 1;
                Vec_PtrPush(vNodes, pNode);
            }
            return;
        }
        // mark the node as visited
        Abc_NodeSetTravIdCurrent(pNode);
        // skip the constants
        if (Abc_AigNodeIsConst(pNode))
            return;
        assert(Abc_ObjIsNode(pNode));
        // visit the transitive fanin of the node
        Abc_ObjForEachFanin(pNode, pFanin, i)
            Lsv_FindHeads_Rec(pFanin, vNodes);
        // // visit the equivalent nodes
        // if (Abc_AigNodeIsChoice(pNode))
        //     for (pFanin = (Abc_Obj_t *)pNode->pData; pFanin; pFanin = (Abc_Obj_t *)pFanin->pData)
        //         Lsv_FindHeads_Rec(pFanin, vNodes);
    }

    Vec_Ptr_t *Lsv_FindHeads(Abc_Ntk_t *pNtk)
    {
        Vec_Ptr_t *vNodes;
        Abc_Obj_t *pNode;
        int i;
        assert(Abc_NtkIsStrash(pNtk));
        // set the traversal ID
        Abc_NtkIncrementTravId(pNtk);
        // start the array of nodes
        vNodes = Vec_PtrAlloc(100);
        // go through the PO nodes and call for each of them
        Abc_NtkForEachCo(pNtk, pNode, i)
        {
            pNode = Abc_ObjFanin0(pNode);
            if (!Abc_ObjIsCi(pNode) && pNode->fMarkA == 0)
            {
                // Each node with fanout to PO is a head
                Vec_PtrPush(vNodes, pNode);
                pNode->fMarkA = 1;
                Lsv_FindHeads_Rec(pNode, vNodes);
            }
        }
        // // collect dangling nodes
        // Abc_NtkForEachNode(pNtk, pNode, i) if (!Abc_NodeIsTravIdCurrent(pNode))
        //     Lsv_FindHeads_Rec(pNode, vNodes);
        return vNodes;
    }

    // compute each SFC
    void Lsv_RecordSFC_Rec(Abc_Obj_t *pNode, Vec_Ptr_t *vNodes)
    {
        Abc_Obj_t *pFanin;
        int i;
        // skip heads and PIs
        if (pNode->fMarkA || Abc_ObjIsCi(pNode))
            return;

        Vec_PtrPush(vNodes, pNode);
        // skip the constants
        if (Abc_AigNodeIsConst(pNode))
            return;

        assert(Abc_ObjIsNode(pNode));
        // visit the transitive fanin of the node
        Abc_ObjForEachFanin(pNode, pFanin, i)
            Lsv_RecordSFC_Rec(pFanin, vNodes);
        // // visit the equivalent nodes
        // if (Abc_AigNodeIsChoice(pNode))
        //     for (pFanin = (Abc_Obj_t *)pNode->pData; pFanin; pFanin = (Abc_Obj_t *)pFanin->pData)
        //         Lsv_RecordSFC_Rec(pFanin, vNodes);
    }

    static int Vec_PtrSortByObjId(Abc_Obj_t **p1, Abc_Obj_t **p2)
    {
        return (int)Abc_ObjId(*p1) - (int)Abc_ObjId(*p2);
    }
    static int Vec_PtrSortByFirstObjId(Vec_Ptr_t **p1, Vec_Ptr_t **p2)
    {
        return (int)Abc_ObjId((Abc_Obj_t *)Vec_PtrEntry(*p1, 0)) - (int)Abc_ObjId((Abc_Obj_t *)Vec_PtrEntry(*p2, 0));
    }

    void Lsv_NtkPrintMsfc(Abc_Ntk_t *pNtk)
    {
        Abc_Obj_t *pNode, *pFanin;
        Vec_Ptr_t *vNodes, *vHeads, *vSFCs;
        int i, j;
        Abc_NtkCleanMarkA(pNtk);
        vHeads = Lsv_FindHeads(pNtk);
        vSFCs = Vec_PtrAlloc(100);
        Vec_PtrForEachEntry(Abc_Obj_t *, vHeads, pNode, i)
        {
            vNodes = Vec_PtrAlloc(100);
            Vec_PtrPush(vSFCs, vNodes);
            Vec_PtrPush(vNodes, pNode);
            Abc_ObjForEachFanin(pNode, pFanin, j)
                Lsv_RecordSFC_Rec(pFanin, vNodes);
        }
        Abc_NtkCleanMarkA(pNtk);
        Vec_PtrFree(vHeads);

        // Sort the computed SFCs
        Vec_PtrForEachEntry(Vec_Ptr_t *, vSFCs, vNodes, i)
            Vec_PtrSort(vNodes, (int (*)(const void *, const void *))Vec_PtrSortByObjId);
        Vec_PtrSort(vSFCs, (int (*)(const void *, const void *))Vec_PtrSortByFirstObjId);
        Vec_PtrForEachEntry(Vec_Ptr_t *, vSFCs, vNodes, i)
        {
            Abc_Print(1, "MSFC %d: ", i);
            Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pNode, j)
            {
                if (j > 0)
                    Abc_Print(1, ",");
                Abc_Print(1, Abc_ObjName(pNode));
            }
            Vec_PtrFree(vNodes);
            Abc_Print(1, "\n");
        }
        Vec_PtrFree(vSFCs);
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
        if (!Abc_NtkIsStrash(pNtk))
        {
            Abc_Print(-1, "Works only for strashed AIG (run \"strash\").\n");
            return 1;
        }
        Lsv_NtkPrintMsfc(pNtk);
        return 0;

    usage:
        Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
        Abc_Print(-2, "\t        prints the maximum single fanout cone of the network\n");
        Abc_Print(-2, "\t-h    : print the command usage\n");
        return 1;
    }

} // unamed namespace