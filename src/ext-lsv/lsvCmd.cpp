#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <set>
using namespace std;
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_MSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_MSFC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

struct myComp{
  bool operator() (const Abc_Obj_t* a, const Abc_Obj_t* b){
    return a->Id < b->Id;
  }
};

struct myComp1{
  bool operator() (const set <Abc_Obj_t*, myComp> a, const set <Abc_Obj_t*, myComp> b){
    return  (*(a.begin()))->Id < (*(b.begin()))->Id;
  }
};

void Lsv_Mark(Abc_Obj_t* pObj, bool flag = false){
    Abc_Obj_t* pFanin;
    int i;
    Abc_ObjForEachFanin(pObj, pFanin, i) {
      // printf("%u\n", pFanin->fMarkA);
      if (pFanin->fMarkA || pFanin->fMarkB ||  Abc_ObjIsPi(pFanin)){
        continue;
      }
      else{
        if (Abc_ObjFanoutVec(pFanin)->nSize > 1 || flag){
          pFanin->fMarkB = 1;
        }
        else {
          pFanin->fMarkA = 1;
        }
        Lsv_Mark(pFanin);
      }
    }
}

void Lsv_Construct(Abc_Obj_t* pObj, set <Abc_Obj_t*, myComp>& small){
    Abc_Obj_t* pFanin;
    int i;
    Abc_ObjForEachFanin(pObj, pFanin, i) 
    {
      if (pFanin->fMarkA || pFanin->fMarkB ||  Abc_ObjIsPi(pFanin)){
        continue;
      }
      else{
          pFanin->fMarkA = 1;
          small.insert(pFanin);
          Lsv_Construct(pFanin, small);
      }
    }
  
}
void msfc_print(unsigned count, set <Abc_Obj_t*, myComp> small){
  printf("MSFC %u: ", count);
  set <Abc_Obj_t*, myComp>::iterator iter = small.begin();
  bool flag = true;
  while(iter != small.end()){
    if(flag){
      flag = false;
    }
    else{
      printf(",");
    }
    printf("n%u", (*iter)->Id);
    ++iter;
  }
  printf("\n");
}
int Lsv_MSFC(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Obj_t* pPo;
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int i;
  Abc_NtkForEachPo( pNtk, pPo, i ){
    Lsv_Mark(pPo, true);
  }
  Abc_Obj_t* pObj;
  Abc_NtkForEachObj(pNtk, pObj, i){
    pObj->fMarkA = 0;
  }
  set <set <Abc_Obj_t*, myComp>, myComp1> to_print;
  Abc_NtkForEachObj(pNtk, pObj, i){
    if(pObj->fMarkB == 1){
      set <Abc_Obj_t*, myComp> small;
      small.insert(pObj);
      Lsv_Construct(pObj, small);
      to_print.insert(small);
    }
  }
  unsigned count = 0;
  set <set <Abc_Obj_t*, myComp>, myComp1>::iterator iter = to_print.begin();
  while(iter != to_print.end()){
    msfc_print(count, *iter);
    ++count;
    ++iter;
  }
  Abc_NtkForEachObj(pNtk, pObj, i){
    pObj->fMarkA = 0;
    pObj->fMarkB = 0;
  }
  return 0;
usage:
  Abc_Print(-2, "usage: lsv_print_msfc\n");
  Abc_Print(-2, "\t        prints msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}









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
