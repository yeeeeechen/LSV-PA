#ifndef _LSV_MSFC_
#define _LSV_MSFC_

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <iostream>

static int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv);

void msfcInit(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
}

void msfcDestroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t msfcFrameInitializer = {msfcInit, msfcDestroy};

struct msfcPackageMgr {
  msfcPackageMgr() { Abc_FrameAddInitializer(&msfcFrameInitializer); }
} LsvMsfcPackageMgr;

bool Lsv_ClassifyNodes(Abc_Obj_t* currentObj,
                       std::vector<Abc_Obj_t*>& nodeVec,
                       std::vector< std::vector<Abc_Obj_t*> >& vecVec,
                       std::unordered_map<Abc_Obj_t*, unsigned>& nodeMap,
                       unsigned& size) {
  int numFO = currentObj->vFanouts.nSize;

  if(Abc_ObjIsPo(currentObj)){
    //std::cerr<<"reach PO "<<numFO<<std::endl;

    vecVec.push_back(nodeVec);
    for(Abc_Obj_t* pastObj: nodeVec){
      assert(nodeMap.find(pastObj)==nodeMap.end());
      nodeMap[pastObj] = size;
    }
    size++;
    return true;
  }
  else if(nodeMap.find(currentObj)!=nodeMap.end()){
    //node has been traversed
    unsigned idx = nodeMap[currentObj];
    std::vector<Abc_Obj_t*>& pastNodeVec = vecVec[idx];
    //std::cerr<<"size before insert: "<<vecVec[idx].size()<<std::endl;
    
    for(Abc_Obj_t* pastObj: nodeVec){
      pastNodeVec.push_back(pastObj);
      assert(nodeMap.find(pastObj)==nodeMap.end());
      nodeMap[pastObj] = idx;
    }
    //std::cerr<<"size after insert: "<<vecVec[idx].size()<<std::endl;
    
    return true;
  }
  else if(numFO>1 || numFO==0){
    nodeVec.push_back(currentObj);
    vecVec.push_back(nodeVec);
    for(Abc_Obj_t* pastObj: nodeVec){
      assert(nodeMap.find(pastObj)==nodeMap.end());
      nodeMap[pastObj] = size;
    }
    size++;
    return true;
  }
  else{
    assert(numFO==1);
    nodeVec.push_back(currentObj);
    Abc_Obj_t* nextObj = Abc_ObjFanout(currentObj, 0);
    if(Lsv_ClassifyNodes(nextObj, nodeVec, vecVec, nodeMap, size)) return true;
  }
  return false;
}

bool sortNodeVec(Abc_Obj_t* node1, Abc_Obj_t* node2){
  return Abc_ObjId(node1) < Abc_ObjId(node2);
}

bool sortVecVec(const std::vector<Abc_Obj_t*>& vec1, const std::vector<Abc_Obj_t*>& vec2){
  return Abc_ObjId(vec1.front()) < Abc_ObjId(vec2.front());
}

int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv) {
  std::unordered_map<Abc_Obj_t*, unsigned> nodeMap; //node to idx of vecVec
  std::vector< std::vector<Abc_Obj_t*> > vecVec;
  unsigned size=0;

  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  Abc_Obj_t* pObj;
  int i;
  //Abc_NtkForEachNode(pNtk, pObj, i){
  Abc_NtkForEachObj(pNtk, pObj, i){

    //the if below is necessary...????
    //if(!(Abc_ObjIsPi(pObj) || Abc_ObjIsPo(pObj))){

    if( Abc_ObjIsNode(pObj) || ((pObj->Type == ABC_OBJ_CONST1) && (Abc_ObjFanoutNum(pObj)!=0)) ){
      std::vector<Abc_Obj_t*> nodeVec;
      Lsv_ClassifyNodes(pObj, nodeVec, vecVec, nodeMap, size);
    }
  }

  for(std::vector<Abc_Obj_t*>& sortedNodeVec: vecVec){
    std::sort(sortedNodeVec.begin(), sortedNodeVec.end(), sortNodeVec);
  }
  std::sort(vecVec.begin(), vecVec.end(), sortVecVec);

  for(int j=0; j<vecVec.size(); ++j){
    std::vector<Abc_Obj_t*>& sortedNodeVec = vecVec[j];
    std::cout<<"MSFC "<<j<<":";
    for(int i=0; i<sortedNodeVec.size(); ++i){
      Abc_Obj_t* sortedNode = sortedNodeVec[i];

      if(i!=0) std::cout<<",";
      std::cout<<Abc_ObjName(sortedNode);
      //std::cout<<"(id:"<<Abc_ObjId(sortedNode)<<")";
    }
    std::cout<<std::endl;
  }

  return 0;
}

#endif
