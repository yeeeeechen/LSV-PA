#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <string>
using namespace std;

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

class NODE{

  public:
    NODE(Abc_Obj_t* pObj,unsigned int ObjId,char* ObjName):pObj(pObj),ObjId(ObjId),ObjName(ObjName){}
    Abc_Obj_t* pObj;
    unsigned int ObjId;
    string ObjName;
};

void Lsv_NtkPrintMsfc(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  std::vector<NODE> msfc_list[Abc_NtkObjNum(pNtk)];
  int msfcindex = 0;
  int count = 1;
  Abc_NtkForEachNode(pNtk, pObj, i){
    if(Abc_ObjFanoutNum(pObj)==1){
      int j;
      Abc_Obj_t* pFanout;
      Abc_ObjForEachFanout(pObj, pFanout, j) {
        if(Abc_ObjType(pFanout)==ABC_OBJ_PO){
          msfc_list[msfcindex].push_back(NODE(pObj,Abc_ObjId(pObj), Abc_ObjName(pObj)));
          msfcindex = msfcindex + count;
          count = 1;
        }
        else{
          msfc_list[msfcindex].push_back(NODE(pObj,Abc_ObjId(pObj), Abc_ObjName(pObj)));
        }
      }
    }
    else{
      if(!msfc_list[msfcindex].empty()){
        msfc_list[msfcindex+count].push_back(NODE(pObj,Abc_ObjId(pObj), Abc_ObjName(pObj)));
        count = count + 1;
      }
      else{
        msfc_list[msfcindex].push_back(NODE(pObj,Abc_ObjId(pObj), Abc_ObjName(pObj)));
        msfcindex = msfcindex + 1;
      }
    }
  }
  
  for(int i=0;i<=msfcindex;i++){
    vector<NODE>::iterator it;
    if(!msfc_list[i].empty())
      printf("MSFC%d :",i);
    else break;
    for(it=msfc_list[i].begin();it !=msfc_list[i].end(); ++it)
      printf(" %s",it->ObjName.c_str());
    printf("\n");
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
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  Lsv_NtkPrintMsfc(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
