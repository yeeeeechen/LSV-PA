#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
using namespace std;

static int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void traversal_fanin (Abc_Obj_t* pObj, std::vector <int> &temp, std::vector <string> &temp_name){
  Abc_Obj_t* pFanin;
  int j;
  Abc_ObjForEachFanin(pObj, pFanin, j) {
    if ( Abc_ObjIsPi(pFanin)==false and (Abc_ObjFanoutNum(pFanin)==1)){
      temp.push_back(Abc_ObjId(pFanin));
      temp_name.push_back(Abc_ObjName(pFanin));
      traversal_fanin (pFanin, temp, temp_name);
    }
  }
}

void Lsv_NtkPrintMsfc(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  std::vector <int> test, store_head;
  std::vector <std::vector <int>> result;
  std::vector <std::vector <string>> result_name;
  std::vector <bool> store_traverse;
  int a, b, c, d, i, j;
  Abc_NtkForEachNode(pNtk, pObj, a) {
    if (Abc_ObjFanoutNum(pObj) > 1) {
      store_head.push_back(Abc_ObjId(pObj));
      store_traverse.push_back(false);
    }
  }

  Abc_NtkForEachObj( pNtk, pObj, i ){
    if (Abc_AigNodeIsConst(pObj)==true){
      if (Abc_ObjFanoutNum(pObj) >= 1) {
        std::vector <int> temp;
        std::vector <string> temp_name;
        temp.push_back(Abc_ObjId(pObj));
        temp_name.push_back(Abc_ObjName(pObj));
        result.push_back(temp);
        result_name.push_back(temp_name);
      }
    }
  }



  Abc_Obj_t* pPo;
  Abc_NtkForEachPo(pNtk, pPo, b){
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(pPo, pFanin, c) {
      store_head.push_back(Abc_ObjId(pFanin));
      store_traverse.push_back(false);
    }
  }

  if (Abc_NtkLatchNum(pNtk) > 0){
    Abc_Obj_t* pObj;
    Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s, type = %d\n", Abc_ObjId(pObj), Abc_ObjName(pObj), Abc_ObjType(pObj));
    }
  }

  Abc_Obj_t* pObj_2;
  Abc_NtkForEachNode(pNtk, pObj_2, d){
    std::vector<int>::iterator it = std::find(store_head.begin(), store_head.end(), Abc_ObjId(pObj_2));
    if (it != store_head.end()){
      int index = std::distance(store_head.begin(), it);
      std::vector <int> temp;
      std::vector <string> temp_name;
      temp.push_back(Abc_ObjId(pObj_2));
      temp_name.push_back(Abc_ObjName(pObj_2));
      traversal_fanin (pObj_2, temp, temp_name);
      result.push_back(temp);
      result_name.push_back(temp_name);
     }
    else{
      continue;}
  }
  for (i=0; i<result.size(); i++){
    for (j=0; j<result[i].size(); j++){
      int min=j;
      for (a=j; a<result[i].size(); a++){
        if (result[i][a] < result[i][min]){
          min = a;
        }
      }
      std::swap(result[i][j], result[i][min]);
      std::swap(result_name[i][j], result_name[i][min]);
    }
  }
  for (i=0; i<result.size(); i++){
    int min = i;
    for (j=i; j<result.size(); j++){
      if (result[j][0] < result[min][0]){
          min = j;
      }
    }
    std::swap(result[i], result[min]);
    std::swap(result_name[i], result_name[min]);
  }

  int k, m;
  for (k=0;k<result_name.size();k++){
    std::cout<<"MSFC "<<k<<": ";
    for (m=0;m<result_name[k].size();m++){
      std::cout<<result_name[k][m];
      if ((m+1)!=result_name[k].size()){
        std::cout<<",";
      }
    }
    std::cout<<std::endl;
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
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
