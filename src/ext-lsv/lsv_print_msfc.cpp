#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <unordered_set>
#include <algorithm>

namespace {

struct msfc_tree {
    std::vector<Abc_Obj_t*> v_pObj;
};

bool sort_tree(Abc_Obj_t* &a, Abc_Obj_t* &b) {
    return Abc_ObjId(a) < Abc_ObjId(b);
}

bool sort_forest(msfc_tree* &a, msfc_tree* &b) {
    return Abc_ObjId(a->v_pObj[0]) < Abc_ObjId(b->v_pObj[0]);
}

void Lsv_PrintMsfc(Abc_Ntk_t* pNtk, int fFileOut) {
    int i, j, k;
    // build MSFCs
    std::vector<msfc_tree*> msfc_forest;
    std::unordered_set<unsigned> rootset;
    Abc_Obj_t* pPo;
    Abc_Obj_t* pFanin;
    Abc_NtkForEachPo(pNtk, pPo, i) {
        Abc_ObjForEachFanin(pPo, pFanin, j) {
            if (Abc_ObjType(pFanin) == ABC_OBJ_PI)
                continue;
            if (rootset.find(Abc_ObjId(pFanin)) == rootset.end()) {
                msfc_tree* p_new = new msfc_tree;
                p_new->v_pObj.push_back(pFanin);
                msfc_forest.push_back(p_new);
                rootset.insert(Abc_ObjId(pFanin));
            }
        }        
    }
    for (i = 0 ; i < msfc_forest.size() ; ++i) {        
        msfc_tree* p_now = msfc_forest[i];
        for (j = 0 ; j < p_now->v_pObj.size() ; ++j) {
            pPo = p_now->v_pObj[j];
            Abc_ObjForEachFanin(pPo, pFanin, k) {
                if (Abc_ObjType(pFanin) == ABC_OBJ_PI)
                    continue;
                if (Abc_ObjFanoutNum(pFanin) == 1) {
                    p_now->v_pObj.push_back(pFanin);
                }
                else if (rootset.find(Abc_ObjId(pFanin)) == rootset.end()) {
                    msfc_tree* p_new = new msfc_tree;
                    p_new->v_pObj.push_back(pFanin);
                    msfc_forest.push_back(p_new);
                    rootset.insert(Abc_ObjId(pFanin));
                }
            }
        }        
    }
    // sort
    int size = msfc_forest.size();
    for (i = 0 ; i < size ; ++i) {
        sort(msfc_forest[i]->v_pObj.begin(), msfc_forest[i]->v_pObj.end(), sort_tree);
    }
    sort(msfc_forest.begin(), msfc_forest.end(), sort_forest);    
    // print
    if (fFileOut == 1) {
        FILE *fp;
        fp = fopen("out.txt", "w"); 
        for (i = 0 ; i < size ; ++i) {
            fprintf(fp, "MSFC %d: ", i);
            msfc_tree* p = msfc_forest[i];
            bool first = true;
            for (j = 0 ; j < p->v_pObj.size() ; ++j) {
                if (!first)     fprintf(fp, ",");
                first = false;
                fprintf(fp, Abc_ObjName(p->v_pObj[j]));
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
    else {
        for (i = 0 ; i < size ; ++i) {
            printf("MSFC %d: ", i);
            msfc_tree* p = msfc_forest[i];
            bool first = true;
            for (j = 0 ; j < p->v_pObj.size() ; ++j) {
                if (!first)     printf(",");
                first = false;
                printf(Abc_ObjName(p->v_pObj[j]));
            }
            printf("\n");
        }
    }    
}

int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); 
    int c;
    int fFileOut = 0;
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "fh")) != EOF) {
        switch (c) {
        case 'f':
            fFileOut = 1;
            break;
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
    Lsv_PrintMsfc(pNtk, fFileOut);
    return 0;

    usage:
        Abc_Print(-2, "usage: lsv_print_msfc [-fh]\n");
        Abc_Print(-2, "\t         print maximum single fanout cones (MSFCs) of all nodes (excluding PIs and POs)\n");
        Abc_Print(-2, "\t         and sort the nodes in an increasing order with respect to their object IDs\n");
        Abc_Print(-2, "\t-f     : output MSFCs to out.txt\n");
        Abc_Print(-2, "\t-h     : print the command usage\n");
        return 1;
}

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct registrar {
    registrar() {
        Abc_FrameAddInitializer(&frame_initializer);
    }    
} lsv_print_msfc_registrar;

}