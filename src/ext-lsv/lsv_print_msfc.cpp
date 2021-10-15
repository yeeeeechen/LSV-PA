#include <iostream>
#include <algorithm>
#include <vector>
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
// abc.h --> struct Abc_Obj_t_ --> add "bool msfc_flag"

// traverse function
void Lsv_Traverse_MSFC(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode, vector<Abc_Obj_t*> first_find_msfc, vector<int> multi_id)
{
  // if meet PI --> return 
  if (Abc_ObjIsPi(pNode)) { pNode->msfc_flag = -1; return; }
  // if meet multi-fanout --> return (count = 1 --> exist)
  if (count(multi_id.begin(), multi_id.end(), Abc_ObjId(pNode))) { return; }
  // if all fanin are marked (flag = 1, -1) --> push back into ans_list 
  bool can_add_into_ans = true;
  Abc_Obj_t* pin;
  int i_;
  Abc_ObjForEachFanin(pNode, pin, i_)
  {
    if (pin->msfc_flag == 0) { can_add_into_ans = false; } /* can keep traversing downward */
  }
  if (can_add_into_ans)
  {
    pNode->msfc_flag = 1; /* marked as traversed */
    first_find_msfc.push_back(pNode); 
  }
  // variable
  Abc_Obj_t* pFanin;
  int i;
  // recursively traverse each fanin
  Abc_ObjForEachFanin(pNode, pFanin, i)
  {
    Lsv_Traverse_MSFC(pNtk, pFanin, first_find_msfc, multi_id);
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
  vector<int>         id_multi_fanout_node; /* id of the above */
  // default each node with flag=0 && find whether has "multi-fanout"
  Abc_Obj_t* pObj;
  int node;
  Abc_NtkForEachNode(pNtk, pObj, node) 
  {
    pObj->msfc_flag = 0;
    // multi-fanout && not (PI / PO)
    if ((!Abc_ObjIsPi(pObj)) && (!Abc_ObjIsPo(pObj)) && (sizeof(pObj->vFanouts) > 1)) 
    {
      pObj->msfc_flag = -1;
      multi_fanout_node.push_back(pObj);
      id_multi_fanout_node.push_back(Abc_ObjId(pObj));
    }
  }
  // check for each PO
  Abc_NtkForEachPo(pNtk, PO, i)
  {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(PO), Abc_ObjName(PO));
    cout << endl;
    // variable
    Abc_Obj_t* pFanin;
    int j;
    // recursively traverse each fanin
    Abc_ObjForEachFanin(PO, pFanin, j)
    {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin), Abc_ObjName(pFanin));
      // start from PO's fanin --> first round !
      vector<Abc_Obj_t*> first_find_msfc;
      Lsv_Traverse_MSFC(pNtk, pFanin, first_find_msfc, id_multi_fanout_node);
      msfc_pair.push_back(first_find_msfc);
    }
  }
  printf("\n========================================\n");
  printf("First round has found %d msfc pair !!!\n", msfc_pair.size());
  printf("========================================\n");
  // second round ! find msfc from multi-fanout node 

  // sort and output (print)

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