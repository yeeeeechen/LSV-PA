#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int LSV_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", LSV_CommandPrintMSFC, 0);
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

bool compareV(Abc_Obj_t* a, Abc_Obj_t* b)
{
  return Abc_ObjId(a) < Abc_ObjId(b);
}

bool compareVV(vector<Abc_Obj_t*>& a, vector<Abc_Obj_t*>& b)
{
  return Abc_ObjId(a[0]) < Abc_ObjId(b[0]);
}

unordered_map<string, int> const_exist;

// void TRAVERSE(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode, vector<Abc_Obj_t*>& cone, bool rt, vector<Abc_Obj_t*>& cst)
void TRAVERSE(Abc_Ntk_t* pNtk, Abc_Obj_t* pNode, vector<Abc_Obj_t*>& cone, bool rt)
{
    if (pNode -> fMarkA == 1) {return;}
    
    // base case
    else if (Abc_ObjIsCi(pNode))
    {
        pNode -> fMarkA = 1;
        pNode -> fMarkB = 0;
        return;
        
    }
    
    // constant
    else if (Abc_ObjType(pNode)==1)
    {
        if (Abc_ObjFanoutNum(pNode) > 1)
        {
            // cst.push_back(pNode);
            return;
        }
        else
        {
            cone.push_back(pNode);
        }
        return;
    }
    
    // new fanout cone
    else if ((Abc_ObjFanoutNum(pNode) > 1)&&(!rt))
    {
        pNode -> fMarkB = 1;
        return;
    }
    
    // travel
    else
    {
        pNode->fMarkA = 1;
        cone.push_back(pNode);
    }

    pNode -> fMarkB = 0;
    Abc_Obj_t* pFanin;
    int i;
    // recursively traverse each fanin
    // Abc_ObjForEachFanin(pNode, pFanin, i) { TRAVERSE(pNtk, pFanin, cone, false, cst); }
    Abc_ObjForEachFanin(pNode, pFanin, i) { TRAVERSE(pNtk, pFanin, cone, false); }
}

void MSFC(Abc_Ntk_t* pNtk)
{
    int i_1;
    bool done;
    done = false;
    Abc_Obj_t* P_init;
    Abc_Obj_t* Po;
    vector<vector<Abc_Obj_t*> > cones;
    
    // initialization
    Abc_NtkForEachNode(pNtk, P_init, i_1)
    {
        P_init -> fMarkA = 0;
        P_init -> fMarkB = 0;
    }
    
    int i_2;
    // primary output
    Abc_NtkForEachPo(pNtk, Po, i_2)
    {
        int j_1;
        Po -> fMarkA = 1;
        Abc_Obj_t* pFanin;
        Abc_ObjForEachFanin(Po, pFanin, j_1)
        {
            //printf("PI \n");
            vector<Abc_Obj_t*> cone;
            vector<Abc_Obj_t*> cst;
            // TRAVERSE(pNtk, pFanin, cone, true, cst);
            TRAVERSE(pNtk, pFanin, cone, true);
            sort(cone.begin(), cone.end(), compareV);
            cones.push_back(cone);
            /*
            if ((!cst.empty())&&(!done))
            {
                cones.push_back(cst);
                done = true;
            }
            */
        }
    }

    while(true)
    {
        // check if empty
        bool exist;
        exist = false;
        Abc_Obj_t* P_check;
        int j_2;
        Abc_NtkForEachNode(pNtk, P_check, j_2)
        {
            if (P_check -> fMarkB == 1)
            {
                exist = true;
                break;
            }
        }
        if (exist == false) {break;}
        
        // execute
        Abc_Obj_t* P_ite;
        int j_3;
        Abc_NtkForEachNode(pNtk, P_ite, j_3)
        {
            if (P_ite -> fMarkB == 1)
            {
                vector<Abc_Obj_t*> CONE;
                vector<Abc_Obj_t*> CST;
                // TRAVERSE(pNtk, P_ite, CONE, true, CST);
                TRAVERSE(pNtk, P_ite, CONE, true);
                sort(CONE.begin(), CONE.end(), compareV);
                cones.push_back(CONE);
                /*
                if ((!CST.empty())&&(!done))
                {
                    cones.push_back(CST);
                    done = true;
                }
                */
            }
        }
    }
    
    sort(cones.begin(), cones.end(), compareVV);
    
    int count_ans = 0;
    for (int k = 0 ; k < cones.size() ; ++k)
    {
      printf("MSFC %d: ", count_ans);
      for (int l = 0 ; l < cones[k].size() ; ++l)
      {
        printf("%s", Abc_ObjName(cones[k][l]));
        if (l == cones[k].size()-1) { printf("\n"); }
        else { printf(","); }
      }
      ++count_ans;
    }

    int i_3;
    Abc_NtkForEachNode(pNtk, P_init, i_3)
    {
        P_init -> fMarkA = 0;
        P_init -> fMarkB = 0;
    }
    
    int i_4;
    Abc_NtkForEachPo(pNtk, P_init, i_4)
    {
        P_init -> fMarkA = 0;
        P_init -> fMarkB = 0;
    }
    
    int i_5;
    Abc_NtkForEachPi(pNtk, P_init, i_5)
    {
        P_init -> fMarkA = 0;
        P_init -> fMarkB = 0;
    }
}

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
  MSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;

}

