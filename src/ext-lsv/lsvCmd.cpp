#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <vector>
#include <algorithm>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBiDecom(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBiDecom, 0);
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

//////////////////////////////////////////////////////////////////////
// my code
//////////////////////////////////////////////////////////////////////

Abc_Obj_t * find_daminating_node(Abc_Obj_t *pObj)
{
  if (Abc_ObjFanoutNum(pObj) == 1)
  {
    if (Abc_ObjIsPo(Abc_ObjFanout(pObj, 0)))
    {
      // printf("find_daminating_node reaches PO.\n");
      return pObj;
    }
    else
    {
      return find_daminating_node(Abc_ObjFanout(pObj, 0));
    }
  }
  else
  {
    return pObj;
  }
}

void find_MSFC(Abc_Obj_t *node, std::vector<Abc_Obj_t *>& MSFC)
{
  if (node->fMarkA == false)
  {
    int j = 0;
    Abc_Obj_t *pFanin;
    node->fMarkA = true;
    MSFC.push_back(node);
    Abc_ObjForEachFanin(node, pFanin, j)
    {
      if (Abc_ObjIsPi(pFanin) == false && Abc_ObjFanoutNum(pFanin) == 1)
      {
        find_MSFC(pFanin, MSFC);
      }
    }
  }
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;

  std::vector<std::vector<Abc_Obj_t *>> MSFCs;
  Abc_NtkCleanMarkA(pNtk);

  Abc_NtkForEachObj(pNtk, pObj, i) {
    if (!Abc_ObjIsNode(pObj) && pObj->Type != ABC_OBJ_CONST1)
    {
      continue;
    }

    if (Abc_ObjFanoutNum(pObj) == 0)
    {
      continue;
    }

    if (pObj->fMarkA == true)
    {
      continue;
    }

    // find the daminating node (node with multi-fanout)
    Abc_Obj_t* daminating_node = find_daminating_node(pObj);
    // find all of the daminated nodes (nodes that have single-fanout and has a single-fanout node path to the daminating node)
    std::vector<Abc_Obj_t*> MSFC;
    find_MSFC(daminating_node, MSFC);
    MSFCs.push_back(MSFC);
  }

  for (int i = 0; i < MSFCs.size(); i++)
  {
    std::sort(MSFCs[i].begin(), MSFCs[i].end(), 
    [](const Abc_Obj_t *a, const Abc_Obj_t *b)
    {
      return a->Id < b->Id;
    });
  }
  std::sort(MSFCs.begin(), MSFCs.end(), 
  [](const std::vector<Abc_Obj_t *> &a, const std::vector<Abc_Obj_t *> &b)
  {
    return a[0]->Id < b[0]->Id;
  });

  for (int i = 0; i < MSFCs.size(); i++)
  {
    printf("MSFC %d: ", i);
    for (int j = 0; j < MSFCs[i].size(); j++)
    {
      if (j == 0)
      {
        printf("%s", Abc_ObjName(MSFCs[i][j]));
      }
      else
      {
        printf(",%s", Abc_ObjName(MSFCs[i][j]));
      }
    }
    printf("\n");
  }

  Abc_NtkCleanMarkA(pNtk);
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

  assert(Abc_NtkIsStrash(pNtk));

  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        maximum single-fanout cones (MSFCs) that covers all nodes (excluding PIs and POs) of a given AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

//////////////////////////////////////////////////////////////////////
// PA2
//////////////////////////////////////////////////////////////////////

int Lsv_CommandOrBiDecom(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  assert(Abc_NtkIsStrash(pNtk));

  // something

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        decides whether each circuit PO f(X) is OR bi-decomposable\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}