#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <string>
#include <vector>
#include <list>

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

// lsv_print_msfc functions
void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int aFanout[Abc_NtkNodeNum(pNtk)], aGroupId[Abc_NtkNodeNum(pNtk)];
  std::vector<std::string> vPrint;
  std::list<int> lOrder;

  // complete aFanout
  int i, shift = -1;
  Abc_NtkForEachNode(pNtk, pObj, i){
    if(shift == -1) shift = Abc_ObjId(pObj);
    // printf("Object Id = %d, name = %s, fanoutNum = %d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), Abc_ObjFanoutNum(pObj));
    if(Abc_ObjFanoutNum(pObj) == 1 && Abc_ObjIsNode(Abc_ObjFanout0(pObj))){
      aFanout[Abc_ObjId(pObj)-shift] = Abc_ObjId(Abc_ObjFanout0(pObj));
    }
    else{
      aFanout[Abc_ObjId(pObj)-shift] = Abc_ObjId(pObj);
    }
  }

  // complete aGroupId
  int maxGroupId = -1, groupId;
  Abc_NtkForEachNodeReverse(pNtk, pObj, i){
    if(aFanout[i-shift] == i){
      groupId = ++maxGroupId;
      vPrint.push_back(std::string(Abc_ObjName(pObj)) + "\n");
    }
    else{
      groupId = aGroupId[aFanout[i-shift]-shift];
      vPrint[groupId] = std::string(Abc_ObjName(pObj)) + "," + vPrint[groupId];
    }
    aGroupId[i-shift] = groupId;

    if(groupId != lOrder.front() || lOrder.empty()){
      if(groupId <= maxGroupId){
        lOrder.remove(groupId);
      }
      lOrder.push_front(aGroupId[i-shift]);
    }
  }
  
  i = 0;
  // handle Const1 case
  if(Abc_ObjFanoutNum(Abc_AigConst1(pNtk)) > 0){
    printf("MSFC %d: %s\n", i, Abc_ObjName(Abc_AigConst1(pNtk)));
    ++i;
  }
  
  for(std::list<int>::iterator it = lOrder.begin(); it != lOrder.end(); ++it){
    printf("MSFC %d: %s", i, vPrint[(*it)].c_str());
    ++i;
  }
  
  // pObj = Abc_AigConst1(pNtk);
  // printf("Object Id = %d, name = %s, fanoutNum = %d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), Abc_ObjFanoutNum(pObj));
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

// lsv_print_nodes functions (example)
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