#include<algorithm>
#include <vector>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"


//read ./lsv_fall_2021/pa1/4bitadder_s.blif 
//strash
//lsv_print_msfc
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrint_msfc(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  //lsv_print_msfc
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrint_msfc, 0);

}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

class Mfsc_set{
  public:
  static int merge(Mfsc_set* a,Mfsc_set* b, std::vector<Mfsc_set*>&total){
    Mfsc_set* big ;
    Mfsc_set*small;
    if(a->mfscset.size()>b->mfscset.size()){
      big=a;  
      small=b;
    }else{
      big=b;
      small=a;
    }
    int id;
    for(int i=0;i<small->mfscset.size();++i){
      id=small->mfscset[i];
      total[id]=big;
    }
    big->mergeInto(small);
    return 0;
  }
  Mfsc_set(int id){
    this->mfscset.push_back(id);
    mark=false;
  }
  int head(){return mfscset[0];}

  //a1 merge into this
  int mergeInto(Mfsc_set* a){
    for(int i=0;i<a->mfscset.size();++i){
      this->mfscset.push_back(a->mfscset[i]);
    }
    a->mfscset.clear();
    return 0;
  }
  void sort(){std::sort(mfscset.begin(),mfscset.end());}
  void show(int i){
    
    printf(" set : %d",i);
    for(int i=0;i<mfscset.size();++i){
      printf (" n%d ",mfscset[i]);
    }
    printf (" end\n");
  }
  bool mark;
  private:
  std::vector<int> mfscset;
  
};
bool compareMFSC_set(Mfsc_set* a,Mfsc_set*b){
  return a->head() <b->head();
}
void Lsv_NtkPrintNodes(Abc_Ntk_t*  pNtk) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s ,type=%d \n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin),Abc_ObjType(pFanin));
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
void Lsv_NtkPrint_msfc(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  std::vector<Mfsc_set*>total;
  for (int i=0;i<Vec_PtrSize((pNtk)->vObjs);++i){
    total.push_back(nullptr);
  }
  // init  :for each  node -> set
  Abc_NtkForEachNode(pNtk, pObj, i) {
    int thisid=Abc_ObjId(pObj);
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    total[thisid]=new Mfsc_set(Abc_ObjId(pObj));
    /*
    Abc_Obj_t* pFanin;
    int j;
    //Abc_ObjForEachFanout
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }
    */

  
  }
  // start merging mfsc set
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    int thisid=Abc_ObjId(pObj);
    int j;
    //Abc_ObjForEachFanout
    Abc_Obj_t* pFanout;
    if(Abc_ObjFanoutNum(pObj)>1)continue;
    Abc_ObjForEachFanout(pObj, pFanout, j){
      if(Abc_ObjType(pFanout)!=7)
      {printf("Object Id = %d not node\n",Abc_ObjId(pFanout));
      continue;
      }
      Mfsc_set::merge(total[thisid],total[Abc_ObjId(pFanout)],total);
      
      //total[thisid]->show(Abc_ObjId(pFanout));
    }
  }
  //printf("leave");
  //start collecting and sorting sets
  std::vector<Mfsc_set*> finalset; 
  for(int i=0;i<total.size();++i){
    if(total[i]==nullptr ||total[i]->mark)continue;
    total[i]->sort();
    total[i]->show(i);
    finalset.push_back(total[i]);
    total[i]->mark=true;
  }
  total.clear();
  //print out the mfsc sets
  std::sort(finalset.begin(),finalset.end(),compareMFSC_set);
  for(int i=0;i<finalset.size();++i){
    finalset[i]->show(i);
  }
  printf("end\n");
}
int Lsv_CommandPrint_msfc(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkPrint_msfc(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the  maximum single-fanout cones\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}