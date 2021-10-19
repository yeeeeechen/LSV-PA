#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <algorithm>
#include <unordered_map>

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

void msfc_traversal(Abc_Obj_t *pObj ,std::vector<Abc_Obj_t*>& vec, std::vector<Abc_Obj_t*>& Ids, std::unordered_map<Abc_Obj_t*, int>& flag){
  int j;
  Abc_Obj_t *pFanin;
  std::vector<Abc_Obj_t*> tmp;
  Abc_ObjForEachFanin(pObj, pFanin, j) {
    if(Abc_ObjIsNode(pFanin) && Abc_ObjFanoutNum(pFanin) == 1){ 
      flag[pFanin] = -1;
      tmp.push_back(pFanin);
    }
    // if(pFanin->Type == ABC_OBJ_CONST1) {
    //   flag[pFanin] = -1;
    //   tmp.push_back(pFanin);
    // }
  }
  for(int i=tmp.size()-1; i>-1; --i) {
    vec.push_back(tmp[i]); 
    msfc_traversal(tmp[i], vec, Ids, flag);
  }
  tmp.clear();
  
  return;
}

bool msfccompare(std::vector<Abc_Obj_t*>& a, std::vector<Abc_Obj_t*>&  b) {
    return Abc_ObjId(a[a.size()-1]) < Abc_ObjId(b[b.size()-1]); 
}

bool Obj_cmp(Abc_Obj_t*& a, Abc_Obj_t*& b) {
    return Abc_ObjId(a) > Abc_ObjId(b);
}

void Lsv_NtkPrintMsfc(Abc_Ntk_t* pNtk) {
  Abc_Obj_t *pObj, *tmp_Obj;
  int i = 0;
  Abc_NtkIncrementTravId(pNtk);
  std::vector<Abc_Obj_t*> Ids, vec;
  std::unordered_map<Abc_Obj_t*, int> flag;
  std::vector<std::vector<Abc_Obj_t*>> msfc;
  std::vector<Abc_Obj_t*>::iterator iter;
  Abc_NtkForEachObj(pNtk, pObj, i){
    if(pObj->Type == ABC_OBJ_CONST1 && Abc_ObjFanoutNum(pObj) != 0) {
      Ids.push_back(pObj);
      flag[pObj] = 0;
    }
    else if(Abc_ObjIsNode(pObj)) {
      Ids.push_back(pObj);
      flag[pObj] = 0;
    } 
  }
  for(int i=Ids.size()-1; i>-1; --i){
    if(flag[Ids[i]] == -1){
      continue;
    }
    else{
      tmp_Obj = Ids[i];
      flag[tmp_Obj] = -1;
      vec.push_back(tmp_Obj);
      msfc_traversal(tmp_Obj, vec, Ids, flag);
      std::sort(vec.begin(), vec.end(), Obj_cmp);
      msfc.push_back(vec);
      vec.clear();
    }
  }
  std::sort(msfc.begin(), msfc.end(), msfccompare);
  for(int k=0; k<msfc.size(); ++k){
    printf("MSFC %d: ", k);
    for(int l=msfc[k].size()-1; l>-1; --l){
      if(l == msfc[k].size()-1) printf("%s", Abc_ObjName(msfc[k][l]));
      else printf(",%s", Abc_ObjName(msfc[k][l]));
    }
    printf("\n");
  }
  return;
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
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints all msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}