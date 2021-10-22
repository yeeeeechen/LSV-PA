#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <map>
using namespace std;


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

//------------------------------------
bool comp(vector<Abc_Obj_t*> a,vector<Abc_Obj_t*>b){
  return Abc_ObjId(a[0])<Abc_ObjId(b[0]);
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk){
  Abc_Obj_t* pObj;
  int i;
  map<int,vector<Abc_Obj_t*>> msfc;


  Abc_NtkForEachObj(pNtk, pObj, i){
    if(pObj->Type == ABC_OBJ_CONST1 && Abc_ObjFanoutNum(pObj) != 0) {
      msfc[Abc_ObjId(pObj)].push_back(pObj);
    }else if(Abc_ObjIsNode(pObj)){
      Abc_Obj_t* tmp=pObj;
      while(Abc_ObjFanoutNum(tmp)==1 and !Abc_ObjIsPo(Abc_ObjFanout0(tmp))){
        tmp=Abc_ObjFanout0(tmp);
      }
      msfc[Abc_ObjId(tmp)].push_back(pObj);
    }
    
    
  }

  
  int ind=0;
  vector<vector<Abc_Obj_t*>> sort_map;
  for(auto msfcnode:msfc){
    sort_map.push_back(msfcnode.second);
  }
  sort(sort_map.begin(),sort_map.end(),comp);

  for(auto msfcnode:sort_map){
    cout << "MSFC " << ind << ":";
    cout << " " << Abc_ObjName(msfcnode[0]);
    for(int j=1;j<msfcnode.size();++j){
      cout << "," << Abc_ObjName(msfcnode[j]);
    }
    cout <<endl;
    ind++;
  }
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
  Abc_Print(-2, "\t        prints the msfc nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}



//--------------------------------------------------------
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