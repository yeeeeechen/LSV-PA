#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

using namespace std;

// add new command
static int LSV_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc)
{
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", LSV_CommandPrintMSFC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = { init, destroy };

struct PackageRegistrationManager
{
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// ** some useful function **
// abc.h --> Abc_ObjIsPi(pObj), Abc_ObjIsPo(pObj)
// abc.h --> Abc_ObjForEachFanin(), Abc_ObjForEachFanout()
// abc.h --> Abc_NtkForEachPi(), Abc_NtkForEachPo()

// ** msfc flag **
// default = 0, traverse = 1, (PI or multi-fanout) = -1

// compare function
bool compareV(Abc_Obj_t* a, Abc_Obj_t* b)
{
  return Abc_ObjId(a) < Abc_ObjId(b);
}

bool compareVV(vector<Abc_Obj_t*>& a, vector<Abc_Obj_t*>& b)
{
  return Abc_ObjId(a[0]) < Abc_ObjId(b[0]);
}

// unordered map --> <name, flag>
unordered_map<string, int> msfc_flag;

// traverse function
void Lsv_Traverse_MSFC(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode, vector<Abc_Obj_t*>& find_msfc, vector<int>& multi_id, vector<string>& PO_node)
{
  // if meet PI --> return (no PI)
  if (Abc_ObjIsPi(pNode)) { msfc_flag[Abc_ObjName(pNode)] = -1; return; }
  // if meet multi-fanout --> return (count = 1 --> exist)
  if (msfc_flag[Abc_ObjName(pNode)] == -1) { return; }
  if (msfc_flag[Abc_ObjName(pNode)] == 0)
  {
    msfc_flag[Abc_ObjName(pNode)] = 1; // marked as traversed 
    find_msfc.push_back(pNode);
  }
  // variable
  Abc_Obj_t* pFanin;
  int i;
  // recursively traverse each fanin
  Abc_ObjForEachFanin(pNode, pFanin, i) { Lsv_Traverse_MSFC(pNtk, pFanin, find_msfc, multi_id, PO_node); }

}

// main function
void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk)
{
  // global variable
  Abc_Obj_t* PO;
  int i;
  vector<vector<Abc_Obj_t*>> msfc_pair;
  vector<Abc_Obj_t*>  multi_fanout_node; /* temp store those have multi fanout's gate */
  vector<string>      PO_node;
  vector<int>         msfc_min_id;
  vector<int>         id_multi_fanout_node; /* id of the above */
  // default each node with flag=0 && find whether has "multi-fanout"
  Abc_Obj_t* pObj;
  int node, count_node=0;
  Abc_NtkForEachNode(pNtk, pObj, node) 
  {
    ++count_node;
    msfc_flag[Abc_ObjName(pObj)] = 0;
    // multi-fanout && not (PI / PO)
    if ((Abc_ObjFanoutNum(pObj) > 1)) 
    {
      msfc_flag[Abc_ObjName(pObj)] = -1;
      multi_fanout_node.push_back(pObj);
      id_multi_fanout_node.push_back(Abc_ObjId(pObj));
    }
  }

  // check for each PO
  Abc_NtkForEachPo(pNtk, PO, i)
  {
    // variable
    PO_node.push_back(Abc_ObjName(PO));
    Abc_Obj_t* pFanin;
    int j;
    // recursively traverse each fanin
    Abc_ObjForEachFanin(PO, pFanin, j)
    {
      // start from PO's fanin --> first round !
      vector<Abc_Obj_t*> first_find_msfc;
      if (msfc_flag[Abc_ObjName(pFanin)] != -1)
      {
        Lsv_Traverse_MSFC(pNtk, pFanin, first_find_msfc, id_multi_fanout_node, PO_node);
        // sort internally
        sort(first_find_msfc.begin(), first_find_msfc.end(), compareV);
        // vector<int> temp_first_msfc;
        // vector<Abc_Obj_t*> sorted_first_msfc;
        // for (int k = 0 ; k < first_find_msfc.size() ; ++k)
        // {
        //   temp_first_msfc.push_back(Abc_ObjId(first_find_msfc[k]));
        // }
        // sort(temp_first_msfc.begin(), temp_first_msfc.end());
        // for (int k = 0 ; k < temp_first_msfc.size() ; ++k)
        // {
        //   for (int l = 0 ; l < first_find_msfc.size() ; ++l)
        //   {
        //     if (Abc_ObjId(first_find_msfc[l]) == temp_first_msfc[k]) { sorted_first_msfc.push_back(first_find_msfc[l]); }
        //   }
        // }
        // msfc_min_id.push_back(Abc_ObjId(sorted_first_msfc[0]));
        msfc_pair.push_back(first_find_msfc);
      }
    }
  }
  // second round ! find msfc from multi-fanout node 
  int count = 0;
  for (int i = 0 ; i < multi_fanout_node.size() ; ++i)
  {
    Abc_Obj_t* pNode = multi_fanout_node[i];
    // mark the root as flag = 0
    msfc_flag[Abc_ObjName(multi_fanout_node[i])] = 0;
    vector<Abc_Obj_t*> second_find_msfc;
    // recursively traverse each fanin
    Lsv_Traverse_MSFC(pNtk, pNode, second_find_msfc, id_multi_fanout_node, PO_node);
    count += 1;
    // sort internally
    sort(second_find_msfc.begin(), second_find_msfc.end(), compareV);
    // vector<int> temp_second_msfc;
    // vector<Abc_Obj_t*> sorted_second_msfc;
    // for (int k = 0 ; k < second_find_msfc.size() ; ++k)
    // {
    //   temp_second_msfc.push_back(Abc_ObjId(second_find_msfc[k]));
    // }
    // sort(temp_second_msfc.begin(), temp_second_msfc.end());
    // for (int k = 0 ; k < temp_second_msfc.size() ; ++k)
    // {
    //   for (int l = 0 ; l < second_find_msfc.size() ; ++l)
    //   {
    //     if (Abc_ObjId(second_find_msfc[l]) == temp_second_msfc[k]) { sorted_second_msfc.push_back(second_find_msfc[l]); }
    //   }
    // }
    // msfc_min_id.push_back(Abc_ObjId(sorted_second_msfc[0]));
    msfc_pair.push_back(second_find_msfc);
  }

  // sort 
  sort(msfc_pair.begin(), msfc_pair.end(), compareVV);
  // sort(msfc_min_id.begin(), msfc_min_id.end());
  // vector<vector<Abc_Obj_t*>> final_ans;
  // for (int k = 0 ; k < msfc_min_id.size() ; ++k)
  // {
  //   for (int l = 0 ; l < msfc_pair.size() ; ++l)
  //   {
  //     if (Abc_ObjId(msfc_pair[l][0]) == msfc_min_id[k]) { final_ans.push_back(msfc_pair[l]); }
  //   }
  // }

  // output (print)
  int count_ans = 0;
  for (int k = 0 ; k < msfc_pair.size() ; ++k)
  {
    printf("MSFC %d: ", count_ans);
    for (int l = 0 ; l < msfc_pair[k].size() ; ++l)
    {
      // printf("%s (id = %d) ", Abc_ObjName(final_ans[k][l]), Abc_ObjId(final_ans[k][l]));
      printf("%s", Abc_ObjName(msfc_pair[k][l]));
      if (l == msfc_pair[k].size()-1) { printf("\n"); }
      else { printf(","); }
    }
    ++count_ans;
  }

}


// Define command function : LSV_CommandPrintMSFC
int LSV_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv)
{
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return -1;
  }
  // main function
  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;

}