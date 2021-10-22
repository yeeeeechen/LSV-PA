#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include<iostream>
#include <vector>
#include <algorithm>
using namespace std;

static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

int m=0;vector<int>a;
int k=0;

//foreachnode{
// if fanout>1,cout Objname;
//else id+travesal;do{id} while(fanout=1);print all ids;
// }
void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    int i;
    int number=Abc_NtkNodeNum(pNtk);
    Abc_NtkForEachNode(pNtk, pObj, i) {
        if((Abc_ObjFanoutNum(pObj)>1 || Abc_ObjFanout0(pObj)->Type!=ABC_OBJ_NODE)  && pObj->fPersist==0 ){

            m++;
            printf("\n");
            if(Abc_ObjFanoutNum(pObj)>1){
                cout<<"MSFC"<<m-1<<": "<<Abc_ObjName(pObj);
            }
            else{
                cout<<"MSFC:"<<m-1;
            }
            DFS(pObj,pObj->Level);
            a.erase(unique(a.begin(),a.end()),a.end());
            sort(a.begin(),a.end());
            for (int i = 0; i < a.size(); i++) {
                if(i==a.size()-1){
                    printf("n%d",a[i]);
                }
                else{
                    printf("n%d,",a[i]);
                }

            }
            a.clear();
        }
        printf("\n");
    }

    Abc_Obj_t* DFS(Abc_Obj_t* pObj,int n){

        if(n==1 || n==0){
            return pObj;
        }
        else if(Abc_ObjFaninNum(pObj)>1 && pObj->Type==ABC_OBJ_NODE && Abc_ObjFanoutNum(pObj)<=1){
            for(int i=0;i<Abc_ObjFaninNum(pObj);i++){
                if(Abc_ObjFanoutNum(Abc_ObjFanin(pObj,i))<=1 ){
                    a.push_back(Abc_ObjId(pObj));
                    a.push_back(Abc_ObjId(Abc_ObjFanin(pObj,i)));
                    Abc_ObjFanin(pObj,i)->fPersist=1;
                    DFS(Abc_ObjFanin(pObj,i),n-1);
                }
            }
        }
        else if(Abc_ObjFanoutNum(pObj)>1){
            for(int i=0;i<Abc_ObjFaninNum(pObj);i++){
                if(Abc_ObjFanin(pObj,i)->Type==ABC_OBJ_NODE && Abc_ObjFanoutNum(Abc_ObjFanin(pObj,i))<=1){
                    if(i==0){
                        m++;
                        printf("\n");
                        cout<<"MSFC: "<<m-1;
                    }
                    a.push_back(Abc_ObjId(Abc_ObjFanin(pObj,i)));
                    DFS(Abc_ObjFanin(pObj,i),n-1);
                }
            }
        }
        return pObj;
    }

    int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
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
        Lsv_NtkPrintMSFC(pNtk);
        return 0;

        usage:
        Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
        Abc_Print(-2, "\t        prints the nodes in the network\n");
        Abc_Print(-2, "\t-h    : print the command usage\n");
        return 1;
    }
