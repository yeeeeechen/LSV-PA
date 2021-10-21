#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <iostream>

#include <vector>
#include <algorithm>

int Lsv_Print_Msfc_Command(Abc_Frame_t* pAbc, int argc, char** argv);
void Lsv_Print_Msfc(Abc_Ntk_t* pNtk);

enum status {
    UNKNOWN = 0,
    PIPO = -1,
    HEAD = 2,
    BODY = 1
};

// -----------------------------

void init(Abc_Frame_t* pAbc){
    Cmd_CommandAdd( pAbc, "LSV", "lsv_print_msfc", Lsv_Print_Msfc_Command, 0);
}

void destroy(Abc_Frame_t* pAbc){}

Abc_FrameInitializer_t frame_initializer = { init, destroy };


struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// -----------------------------




int Lsv_Print_Msfc_Command(Abc_Frame_t* pAbc, int argc, char** argv) {
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
        if (!pNtk) {
            Abc_Print(-1, "Empty network.\n");
            return 1;
        }
    }
    Lsv_Print_Msfc(pNtk);
    return 0;


    usage:
      Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
      Abc_Print(-2, "\t        find maximum single-fanout cones (MSFCs) that covers all nodes (excluding PIs and POs) of a given AIG\n");
      Abc_Print(-2, "\t-h    : print the command usage\n");
      return 1;
}

bool compare_between_nodes(Abc_Obj_t* a, Abc_Obj_t* b){
    return Abc_ObjId(a) < Abc_ObjId(b);
}

bool compare_between_Msfcs(std::vector<Abc_Obj_t*> a, std::vector<Abc_Obj_t*> b){
    return Abc_ObjId(a.at(0)) < Abc_ObjId(b.at(0));
}

void extend_Msfc(std::vector<Abc_Obj_t*> &v, Abc_Obj_t* head){
    int i;
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(head, pFanin, i){
        if(Abc_NodeTravId(pFanin)!=status::UNKNOWN){
            continue;
        }
        Abc_NodeSetTravId(pFanin, status::BODY);
        v.push_back(pFanin);
        extend_Msfc(v, pFanin);
    }
    return;
}

void Lsv_Print_Msfc(Abc_Ntk_t* pNtk) {
    std::vector<std::vector<Abc_Obj_t*>> msfcs;

    Abc_Obj_t* pObj;
    int i;

    Abc_NtkForEachObj(pNtk, pObj, i){
        Abc_NodeSetTravId(pObj, status::UNKNOWN);
    }

    Abc_NtkForEachObj(pNtk, pObj, i){
        if(Abc_ObjIsPi(pObj) ||  Abc_ObjIsPo(pObj)){
            Abc_NodeSetTravId(pObj, status::PIPO);
            continue;
        }
        if(Abc_ObjType(pObj)==1 && Abc_ObjFanoutNum(pObj)==0){      // for unused const 1
            Abc_NodeSetTravId(pObj, status::PIPO);
            continue;
        }
        if(Abc_ObjFanoutNum(pObj)==1){
            bool isHead = true;
            Abc_Obj_t* pFanin;
            int t;
            Abc_ObjForEachFanout(pObj, pFanin, t){
                if(!Abc_ObjIsPo(pFanin)){
                    isHead = false;
                }
            }
            if(!isHead){
                continue;
            }
        }
        Abc_NodeSetTravId(pObj, status::HEAD);
        std::vector<Abc_Obj_t*> temp;
        temp.push_back(pObj);
        msfcs.push_back(temp);
    }


    for(int j=0; j<msfcs.size(); j++){
        extend_Msfc(msfcs[j], msfcs[j].at(0));
    }

    Abc_NtkForEachObj(pNtk, pObj, i){
        if(Abc_NodeTravId(pObj)==status::UNKNOWN){
            printf("BUG: get a node without TravId\n");
        }
    }

    for(int j=0; j<msfcs.size(); j++){
        std::sort(msfcs[j].begin(), msfcs[j].end(), compare_between_nodes);
    }
    std::sort(msfcs.begin(), msfcs.end(), compare_between_Msfcs);

    for(int j=0; j<msfcs.size(); j++){
        printf("MSFC %d: ", j);
        for(int k=0; k<msfcs[j].size()-1; k++){
            printf("%s,", Abc_ObjName(msfcs[j][k]));
        }
        printf("%s\n", Abc_ObjName(msfcs[j][msfcs[j].size()-1]));
    }
}
