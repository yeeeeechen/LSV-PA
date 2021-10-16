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

// ** modify abc **
// abc.h --> struct Abc_Obj_t_ --> add "int msfc_flag" --> default=0, traverse=1, (PI or multi-fanout)=-1

// unordered map --> <name, flag>
unordered_map<string, int> msfc_flag;

// traverse function
void Lsv_Traverse_MSFC(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode, vector<Abc_Obj_t*>& find_msfc, vector<int>& multi_id, vector<string>& PO_node)
{
  // if meet PI --> return (no PI)
  if (Abc_ObjIsPi(pNode)) { cout << "PI" << endl; msfc_flag[Abc_ObjName(pNode)] = -1; return; }
  // if meet multi-fanout --> return (count = 1 --> exist)
  if (msfc_flag[Abc_ObjName(pNode)] == -1) { cout << "MULTI" << endl; return; }
  // if (count(multi_id.begin(), multi_id.end(), Abc_ObjId(pNode))) { cout << "MULTI" << endl; return; }
  if (msfc_flag[Abc_ObjName(pNode)] == 0)
  {
    cout << "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh : " << Abc_ObjName(pNode) << " --> " << msfc_flag[Abc_ObjName(pNode)] << endl;
    msfc_flag[Abc_ObjName(pNode)] = 1; // marked as traversed 
    // Do not push back PO
    if (!count(PO_node.begin(), PO_node.end(), Abc_ObjName(pNode))) { find_msfc.push_back(pNode); } 
  }
  // if all fanin are marked (flag = 1, -1) --> push back into ans_list 
    /*
      bool can_add_into_ans = true;
      Abc_Obj_t* pin;
      int i_;
      Abc_ObjForEachFanin(pNode, pin, i_)
      {
        if (msfc_flag[Abc_ObjName(pin)] == 0) // can keep traversing downward 
        { 
          cout << "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu" << endl; can_add_into_ans = false; 
        } 
      }
      if (can_add_into_ans)
      {
        msfc_flag[Abc_ObjName(pNode)] = 1; // marked as traversed 
        find_msfc.push_back(pNode); 
        cout << "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh : " << Abc_ObjName(pNode) << endl;
      }
    */
  // variable
  Abc_Obj_t* pFanin;
  int i;
  // recursively traverse each fanin
  Abc_ObjForEachFanin(pNode, pFanin, i)
  {
    Lsv_Traverse_MSFC(pNtk, pFanin, find_msfc, multi_id, PO_node);
  }

}

// main function
void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk)
{
  cout << "==================================" << endl;
  cout << "HELLO! ENTER Lsv_NtkPrintMSFC()..." << endl;
  cout << "==================================" << endl;
  cout << endl;
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
  printf("\n=============\n");
  printf("Total node...");
  printf("\n=============\n");
  Abc_NtkForEachNode(pNtk, pObj, node) 
  {
    printf("%s ", Abc_ObjName(pObj));
    ++count_node;
    msfc_flag[Abc_ObjName(pObj)] = 0;
    // multi-fanout && not (PI / PO)
    if ((Abc_ObjFanoutNum(pObj) > 1)) 
    {
      // for debugging
      // variable
      Abc_Obj_t* pFanout;
      int j;
      int count = 0;
      cout << "\njkjkjkjkjkjkjkjk : " << Abc_ObjFanoutNum(pObj) << endl;
      // recursively traverse each fanin
      Abc_ObjForEachFanout(pObj, pFanout, j)
      {
        cout << "hhhhhhhhhhhhhhhhhh : " << count << endl;
        cout << Abc_ObjName(pFanout) << endl;
        count++;
      }
      //
      //
      msfc_flag[Abc_ObjName(pObj)] = -1;
      multi_fanout_node.push_back(pObj);
      id_multi_fanout_node.push_back(Abc_ObjId(pObj));
    }
  }
  printf("\n\n===================================\n");
  printf("There are %d node !!!\n", count_node);
  printf("There are %d multi-fanout node !!!\n", multi_fanout_node.size());
  printf("===================================\n");

  // check for each PO
  Abc_NtkForEachPo(pNtk, PO, i)
  {
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(PO), Abc_ObjName(PO));
    // msfc_flag[Abc_ObjName(PO)] = -1;
    // variable
    cout << "kkkkkkkkkkkkkkkkkkkkkkkkkk : " << Abc_ObjName(PO) << " --> " << msfc_flag[Abc_ObjName(PO)] << endl;
    PO_node.push_back(Abc_ObjName(PO));
    Abc_Obj_t* pFanin;
    int j;
    // recursively traverse each fanin
    Abc_ObjForEachFanin(PO, pFanin, j)
    {
      // printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
      cout << endl;
      // start from PO's fanin --> first round !
      vector<Abc_Obj_t*> first_find_msfc;
      if (msfc_flag[Abc_ObjName(pFanin)] != -1)
      {
        Lsv_Traverse_MSFC(pNtk, pFanin, first_find_msfc, id_multi_fanout_node, PO_node);
        // sort internally
        vector<int> temp_first_msfc;
        vector<Abc_Obj_t*> sorted_first_msfc;
        for (int k = 0 ; k < first_find_msfc.size() ; ++k)
        {
          temp_first_msfc.push_back(Abc_ObjId(first_find_msfc[k]));
        }
        sort(temp_first_msfc.begin(), temp_first_msfc.end());
        for (int k = 0 ; k < temp_first_msfc.size() ; ++k)
        {
          for (int l = 0 ; l < first_find_msfc.size() ; ++l)
          {
            if (Abc_ObjId(first_find_msfc[l]) == temp_first_msfc[k]) { sorted_first_msfc.push_back(first_find_msfc[l]); }
          }
        }
        msfc_min_id.push_back(Abc_ObjId(sorted_first_msfc[0]));
        msfc_pair.push_back(sorted_first_msfc);
        // for debugging
        printf("\n============================================\n");
        for (int k = 0 ; k < sorted_first_msfc.size() ; ++k)
        {
          printf("%s (id = %d) ", Abc_ObjName(sorted_first_msfc[k]), Abc_ObjId(sorted_first_msfc[k]));
        }
        printf("\n============================================\n");
      }
    }
  }
  printf("\n========================================\n");
  printf("First round has found %d msfc pair !!!\n", msfc_pair.size());
  printf("========================================\n\n");
  // second round ! find msfc from multi-fanout node 
  int count = 0;
  for (int i = 0 ; i < multi_fanout_node.size() ; ++i)
  {
    Abc_Obj_t* pNode = multi_fanout_node[i];
    // mark the root as flag = 0
    msfc_flag[Abc_ObjName(multi_fanout_node[i])] = 0;
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pNode), Abc_ObjName(pNode));
    vector<Abc_Obj_t*> second_find_msfc;
    // recursively traverse each fanin
    Lsv_Traverse_MSFC(pNtk, pNode, second_find_msfc, id_multi_fanout_node, PO_node);
    count += 1;
    // sort internally
    vector<int> temp_second_msfc;
    vector<Abc_Obj_t*> sorted_second_msfc;
    for (int k = 0 ; k < second_find_msfc.size() ; ++k)
    {
      temp_second_msfc.push_back(Abc_ObjId(second_find_msfc[k]));
    }
    sort(temp_second_msfc.begin(), temp_second_msfc.end());
    for (int k = 0 ; k < temp_second_msfc.size() ; ++k)
    {
      for (int l = 0 ; l < second_find_msfc.size() ; ++l)
      {
        if (Abc_ObjId(second_find_msfc[l]) == temp_second_msfc[k]) { sorted_second_msfc.push_back(second_find_msfc[l]); }
      }
    }
    msfc_min_id.push_back(Abc_ObjId(sorted_second_msfc[0]));
    msfc_pair.push_back(sorted_second_msfc);
    // for debugging
    cout << second_find_msfc.size() << endl;
    printf("\n============================================\n");
    for (int k = 0 ; k < sorted_second_msfc.size() ; ++k)
    {
      printf("%s (id = %d) ", Abc_ObjName(sorted_second_msfc[k]), Abc_ObjId(sorted_second_msfc[k]));
    }
    printf("\n============================================\n");
  }
  printf("\n========================================\n");
  printf("Second round has found %d msfc pair !!!\n", count);
  printf("========================================\n\n");

  // sort 
  sort(msfc_min_id.begin(), msfc_min_id.end());
  vector<vector<Abc_Obj_t*>> final_ans;
  for (int k = 0 ; k < msfc_min_id.size() ; ++k)
  {
    for (int l = 0 ; l < msfc_pair.size() ; ++l)
    {
      if (Abc_ObjId(msfc_pair[l][0]) == msfc_min_id[k]) { final_ans.push_back(msfc_pair[l]); }
    }
  }

  // output (print)
  printf("\n===============\n");
  printf("Final Answer...");
  printf("\n===============\n");
  int count_ans = 0;
  for (int k = 0 ; k < final_ans.size() ; ++k)
  {
    printf("MSFC %d: ", count_ans);
    for (int l = 0 ; l < final_ans[k].size() ; ++l)
    {
      printf("%s (id = %d) ", Abc_ObjName(final_ans[k][l]), Abc_ObjId(final_ans[k][l]));
      if (l == final_ans[k].size()-1) { printf("\n"); }
      else { printf(","); }
    }
    ++count_ans;
  }
  printf("\n");

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