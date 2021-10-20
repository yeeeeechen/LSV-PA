#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
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
  Abc_Obj_t*
   pObj;
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
    NODE(Abc_Obj_t* pObj):pObj(pObj){
      ObjId = Abc_ObjId(pObj);
      ObjName = Abc_ObjName(pObj);
    }
    NODE(Abc_Obj_t* pObj,unsigned int ObjId,char* ObjName):pObj(pObj),ObjId(ObjId),ObjName(ObjName){}
    Abc_Obj_t* getObj(){
      return pObj;
    }
    unsigned int getObjId(){
      return ObjId;
    }
    char* getObjName(){
      return ObjName;
    }
  private:
    Abc_Obj_t* pObj;
    unsigned int ObjId;
    char* ObjName;
};

void Traversal(vector<NODE*> &msfc_list, Abc_Obj_t* pObj)
{
  Abc_Obj_t* pFanin;
  int i;
  Abc_ObjForEachFanin(pObj, pFanin, i) {
    if((Abc_ObjFanoutNum(pFanin)==1)&&(Abc_ObjIsPi(pFanin) == 0)){
      msfc_list.push_back(new NODE(pFanin));
      Traversal(msfc_list, pFanin);
    }
  }
}
bool compare_list(NODE* node1, NODE* node2){
  return Abc_ObjId(node1->getObj()) < Abc_ObjId(node2->getObj());
}
bool compare_ALLSTAR(vector<NODE*> list1, vector<NODE*> list2){
  return Abc_ObjId(list1[0]->getObj()) < Abc_ObjId(list2[0]->getObj());
}
void push_back_ALLSTAR(vector<vector<NODE*>> &MSFC_ALLSTAR, vector<NODE*> &msfc_list, Abc_Obj_t* pObj){
  msfc_list.push_back(new NODE(pObj));
  Traversal(msfc_list, pObj);
  sort(msfc_list.begin(),msfc_list.end(), compare_list);
  MSFC_ALLSTAR.push_back(msfc_list);
}

void myfile_ALLSTAR(vector<vector<NODE*>> &MSFC_ALLSTAR){
  std::ofstream myfile;
  //myfile.open("example.txt");
  int MSFC_i = 0;
  vector<vector<NODE*>>::iterator it;
  //myfile << "ABC command line: \"lsv_print_msfc\"." << endl << endl;
  for(it=MSFC_ALLSTAR.begin();it!=MSFC_ALLSTAR.end();++it){
    //myfile << "MSFC " << MSFC_i <<": ";
    cout << "MSFC " << MSFC_i <<": ";
    int dotFlag = 0;
    vector<NODE*>::iterator it2;    
    for(it2=(*it).begin();it2!=(*it).end();++it2){
      if(dotFlag == 0) dotFlag = 1;
      else cout << ",";//myfile << "," ;
      //myfile<< Abc_ObjName((*it2)->getObj()) ;
      cout << Abc_ObjName((*it2)->getObj()) ;
    }
    //myfile << endl;
    cout << endl;
    MSFC_i++;
  }
  //myfile.close();
}

void Lsv_NtkPrintMsfc(Abc_Ntk_t* pNtk) {
  vector<vector<NODE*>> MSFC_ALLSTAR;
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachObj(pNtk, pObj, i) {
    vector<NODE*> msfc_list;
    Abc_Obj_t* pFanout;
    int j;
    if(Abc_ObjFanoutNum(pObj)>1 && !Abc_ObjIsPi(pObj) && !Abc_ObjIsPo(pObj)) push_back_ALLSTAR(MSFC_ALLSTAR,msfc_list,pObj);
    else if(Abc_ObjType(pObj) == ABC_OBJ_CONST1 && Abc_ObjFanoutNum(pObj)>=1 )push_back_ALLSTAR(MSFC_ALLSTAR,msfc_list,pObj);
    else if(!Abc_ObjIsPi(pObj) && !Abc_ObjIsPo(pObj)){
      Abc_ObjForEachFanout(pObj,pFanout,j){
	      if(Abc_ObjIsPo(pFanout))push_back_ALLSTAR(MSFC_ALLSTAR,msfc_list,pObj);
      }
    }
  }
  sort(MSFC_ALLSTAR.begin(),MSFC_ALLSTAR.end(),compare_ALLSTAR);
  myfile_ALLSTAR(MSFC_ALLSTAR);
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

