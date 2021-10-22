#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

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

void Lsv_BuildSFC(Abc_Obj_t* pObj, std::set<int>& sfc){
  sfc.insert(Abc_ObjId(pObj));
  Abc_Obj_t* pFanin;
  int j;
  Abc_ObjForEachFanin(pObj, pFanin, j) {
    if(Abc_ObjIsPo(pFanin) || Abc_ObjIsPi(pFanin)){
      continue;
    }
    if(Abc_ObjFanoutNum(pFanin) == 1){
      sfc.insert(Abc_ObjId(pFanin));
      Lsv_BuildSFC(pFanin, sfc);
    }
  }
}

struct compare_sfc{
  inline bool operator() (const std::set<int>& s1, const std::set<int>& s2){
    return(*s1.begin() < *s2.begin());
  }
};
void Lsv_PrintSFCS(std::vector< std::set<int> >& sfcs) {
  std::sort(sfcs.begin(), sfcs.end(), compare_sfc());
  for(size_t i=0; i<sfcs.size(); ++i){
    std::cout << "MSFC " << i << ": ";
    for(auto ite = sfcs[i].begin(); ite != sfcs[i].end(); ){
      std::cout << "n" << *ite;
      if(++ite != sfcs[i].end()){
        std::cout << ",";
      }
      else{
        std::cout << "\n";
      }
    }
  }
}

void Lsv_PrintMSFC(Abc_Ntk_t* pNtk){
  std::vector< std::set<int> > sfcs;
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachObj(pNtk, pObj, i){
    if(Abc_ObjType(pObj) == ABC_OBJ_CONST1 || Abc_ObjType(pObj) == ABC_OBJ_NODE){
      if(Abc_ObjFanoutNum(pObj) == 1){
        Abc_Obj_t* fanout = Abc_ObjFanout(pObj, 0);
        if(!Abc_ObjIsPo(fanout) && (fanout != pObj)){
          continue;
        }
      }
      if(Abc_ObjFanoutNum(pObj) == 0){
        continue;
      }
      std::set<int> sfc;
      Lsv_BuildSFC(pObj, sfc);
      sfcs.push_back(sfc);
    }
  }
  Lsv_PrintSFCS(sfcs);
}

int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_PrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the MSFCs in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}