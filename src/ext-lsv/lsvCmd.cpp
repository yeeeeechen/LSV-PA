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

bool compareByTreeID (vector<Abc_Obj_t*> tree1, vector<Abc_Obj_t*> tree2){
  return (Abc_ObjId(tree1.front()) < Abc_ObjId(tree2.front()) );
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
  Abc_Obj_t* pObj;
  vector<Abc_Obj_t*> p_msfc_nodes;  
  vector<vector<Abc_Obj_t*> > msfc_trees; // stores SORTED msfc trees
  map<Abc_Obj_t*, int> obj2vec;
  int sortedNodeCount = 0;
  int msfcNodeCount = 0;
  int msfc_tree_index = 0;
  int node_index = 0;
  int i = 0;
  // load all nodes, including Const1
  for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ ){ 
    if ( (pObj) != NULL && (Abc_ObjIsNode(pObj) || (pObj->Type == ABC_OBJ_CONST1 && Abc_ObjFanoutNum(pObj) != 0))) { 
      p_msfc_nodes.push_back(pObj);
      obj2vec[pObj] = node_index;
      node_index++;
    }
  }
  int totalNodeCount = p_msfc_nodes.size();
  // if ((Abc_ObjFanoutNum(p_msfc_nodes.front()) == 1) || (Abc_ObjFanoutNum(p_msfc_nodes.front()) == 1)) p_msfc_nodes.erase(p_msfc_nodes.begin());
  while(sortedNodeCount < totalNodeCount){ // find an MSFC in the AIG
    // printf("msfc_tree %d\n", msfc_tree_index); 
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
    // sort(p_msfc_nodes.begin(), p_msfc_nodes.end() - sortedNodeCount, compareByID);
    sort(p_msfc_nodes.end() - sortedNodeCount, p_msfc_nodes.end() - sortedNodeCount + msfcNodeCount, compareByID);
    vector<Abc_Obj_t*> msfc_tree_temp;
    for (i=0; i<msfcNodeCount; i++){
      msfc_tree_temp.push_back(p_msfc_nodes[i + totalNodeCount - sortedNodeCount]);
      obj2vec[p_msfc_nodes[i + totalNodeCount - sortedNodeCount]] = i + totalNodeCount - sortedNodeCount;      
    }
    msfc_trees.push_back(msfc_tree_temp);
    msfc_tree_temp.clear();

    // print out info
    /*
    printf("MSFC %d: %s", msfc_tree_index, Abc_ObjName(p_msfc_nodes[totalNodeCount - sortedNodeCount]));
    if (msfcNodeCount > 1){
      for (node_index = totalNodeCount - sortedNodeCount + 1; node_index <= totalNodeCount - sortedNodeCount + msfcNodeCount - 1; ++node_index){
        printf(",%s",Abc_ObjName(p_msfc_nodes[node_index]));
      }
    }
    printf("\n");
    */
    msfc_tree_index++;
    msfcNodeCount=0;
  }
  // print out info
  sort(msfc_trees.begin(), msfc_trees.end(), compareByTreeID);
  for (int i=0; i < msfc_trees.size(); i++){
    printf("MSFC %d: %s", i, Abc_ObjName(msfc_trees[i][0]));
    if (msfc_trees[i].size() > 1){
      for (int j=1; j < msfc_trees[i].size(); j++){
        printf(",%s", Abc_ObjName(msfc_trees[i][j]));
      }
    }
    printf("\n"); 
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