#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <algorithm>
#include <map>
#include <vector>
using namespace std;

static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

// ---------- lsv_print_msfc ----------

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

bool compareByID (Abc_Obj_t* Obj1, Abc_Obj_t* Obj2){
  return (Abc_ObjId(Obj1) < Abc_ObjId(Obj2) );
}

int findMSFCLeaves(vector<Abc_Obj_t*> &msfc_nodes_vec, Abc_Obj_t* pObj, map<Abc_Obj_t*, int>& obj2vec, int& sortedNodeCount){ // find MSFC nodes recursively
  Abc_Obj_t* pFanin;
  int size = msfc_nodes_vec.size();
  int msfcNodeCount = 1;
  int i;
  
  // select and swap the current MSFC node to the back of the vector.
  sortedNodeCount++;
  Abc_Obj_t* Obj_temp = pObj; 
  int index_temp = obj2vec[pObj];
  msfc_nodes_vec[obj2vec[pObj]] = msfc_nodes_vec[size-sortedNodeCount];
  msfc_nodes_vec[size-sortedNodeCount] = Obj_temp;
  obj2vec[msfc_nodes_vec[size-sortedNodeCount]] = size-sortedNodeCount;
  obj2vec[msfc_nodes_vec[index_temp]] = index_temp;

  // check fanin nodes
  Abc_ObjForEachFanin(pObj, pFanin, i){ 
    if ((Abc_ObjFanoutNum(pFanin) == 1) 
    && ( Abc_ObjType(pFanin) != ABC_OBJ_PI)
    && (obj2vec[pFanin] < size-sortedNodeCount) ){ // find the children 
      msfcNodeCount += findMSFCLeaves (msfc_nodes_vec, pFanin, obj2vec, sortedNodeCount);
    }
  }
  return msfcNodeCount; // return total msfc node count when reading a leaf.
}

int findMSFCRoot(vector<Abc_Obj_t*> msfc_nodes_vec, Abc_Obj_t* pObj, map<Abc_Obj_t*, int> obj2vec, int sortedNodeCount){
  //cout <<"node name = "<<Abc_ObjName(pObj)<<", # of fanouts = "<<Abc_ObjFanoutNum(pObj)<<"\n";
  int size = msfc_nodes_vec.size();
  while(pObj != NULL){    
    if ((Abc_ObjFanoutNum(pObj) == 1) 
    && (Abc_ObjType(Abc_ObjFanout(pObj, 0)) != ABC_OBJ_PO) 
    && (obj2vec[Abc_ObjFanout(pObj, 0)] < size-sortedNodeCount)){ // keep searching the root of msfc tree
      Abc_Obj_t* pFanout = Abc_ObjFanout(pObj, 0);
      pObj = pFanout;
    }
    else return obj2vec[pObj]; // the root of msfc tree is found
  }
  return 0; 
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  vector<Abc_Obj_t*> p_msfc_nodes;  
  Abc_Obj_t* pObj;
  map<Abc_Obj_t*, int> obj2vec;
  int sortedNodeCount = 0;
  int msfcNodeCount = 0;
  int msfc_tree_index = 0;
  int node_index;
  int i = 0;
  Abc_NtkForEachNode(pNtk, pObj, node_index) { // Loading all nodes to p_msfc_nodes
    if ((Abc_ObjType(pObj) != ABC_OBJ_PO) && (Abc_ObjType(pObj) != ABC_OBJ_PI)){
      p_msfc_nodes.push_back(pObj);
      obj2vec[pObj] = i;
      i++;
    }
  }
  int totalNodeCount = p_msfc_nodes.size();
  // cout<<"# of nodes in the AIG = "<<totalNodeCount<<"\n";
  while(sortedNodeCount < totalNodeCount){ // find an MSFC in the AIG
    /*
    for (i=0; i<totalNodeCount; i++){
      cout<<"p_msfc_nodes["<<i<<"] = "<< Abc_ObjName(p_msfc_nodes[i])<<"\n";
      cout<<"obj2vec["<<Abc_ObjName(p_msfc_nodes[i])<<"] = "<<obj2vec[p_msfc_nodes[i]]<<"\n";
    }
    */
    // Find the root of the MSFC tree
    int rootID = findMSFCRoot(p_msfc_nodes, p_msfc_nodes.front(), obj2vec, sortedNodeCount); 
    //cout <<"root = "<<Abc_ObjName(p_msfc_nodes[rootID])<<"\n";
    
    // Find the nodes of the MSFC tree
    msfcNodeCount = findMSFCLeaves(p_msfc_nodes, p_msfc_nodes[rootID], obj2vec, sortedNodeCount); 
    
    // Sort the nodes in the MSFC tree by ID
    sort(p_msfc_nodes.begin(), p_msfc_nodes.end() - sortedNodeCount, compareByID);
    sort(p_msfc_nodes.end() - sortedNodeCount, p_msfc_nodes.end() - sortedNodeCount + msfcNodeCount, compareByID);
    for (i=0; i<totalNodeCount; i++){
      obj2vec[p_msfc_nodes[i]] = i;      
    }

    // print out info
    cout<<"MSFC "<<msfc_tree_index<<": ";
    cout <<Abc_ObjName(p_msfc_nodes[totalNodeCount - sortedNodeCount]);
    if (msfcNodeCount > 1){
      for (node_index = totalNodeCount - sortedNodeCount + 1; node_index <= totalNodeCount - sortedNodeCount + msfcNodeCount - 1; ++node_index){
        cout <<",";
        cout << Abc_ObjName(p_msfc_nodes[node_index]);
      }
    }
    cout <<"\n";
    msfc_tree_index++;
    msfcNodeCount=0;
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
  Abc_Print(-2, "\t        prints the maximum single-fanout cones (MSFCs) of the AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ---------- lsv_print_nodes ----------

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    int j;
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }
    Abc_Obj_t* pFanout;
    Abc_ObjForEachFanout(pObj, pFanout, j) {
      printf("  Fanout-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanout),
             Abc_ObjName(pFanout));
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