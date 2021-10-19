#include <set>
#include <vector>
#include <algorithm>
#include <iostream>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLsvPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandLsvPrintMsfc, 0);
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



void FindTheSameMsfc(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, std::vector<Abc_Obj_t*> &v,std::vector<std::vector<Abc_Obj_t*> > &vv) {
  Abc_Obj_t* pFanin;
  int i;
  if ( Abc_NodeIsTravIdCurrent( pObj ) || Abc_ObjFaninNum(pObj) == 0) return;
  Abc_NodeSetTravIdCurrent( pObj );
  Abc_ObjForEachFanin(pObj, pFanin, i)
  {
    if(Abc_ObjFanoutNum(pFanin) == 1 && Abc_ObjFaninNum(pFanin) != 0)
    {
      // TODO : set to the same group
      FindTheSameMsfc(pNtk, pFanin, v, vv);
      v.push_back(pFanin);
    }
    else
    {
      continue;
    }
  }
  return;
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

bool compareVV(std::vector<Abc_Obj_t*> &v1, std::vector<Abc_Obj_t*> &v2)
{
  return v1[0] -> Id < v2[0] -> Id;
}

bool compareV(Abc_Obj_t* a, Abc_Obj_t* b)
{
  return a -> Id < b -> Id;
} 

void FindConst(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, std::vector<Abc_Obj_t*> &v,std::vector<std::vector<Abc_Obj_t*> > &vv) {
  Abc_Obj_t* pFanin;
  int i;
  if ( Abc_NodeIsTravIdCurrent( pObj ) ) return;
  Abc_NodeSetTravIdCurrent( pObj );
  if(Abc_ObjType(pObj) == 1)
  {
    v.push_back(pObj);
      return;
  }
  Abc_ObjForEachFanin(pObj, pFanin, i)
  {
    if(Abc_ObjType(pFanin) == 1)
    {
      // TODO : set to the same group
      // FindTheSameMsfc(pNtk, pFanin, v, vv);
      Abc_NodeSetTravIdCurrent( pFanin );
      v.push_back(pFanin);
      return;
    }
    else
    {
      FindConst(pNtk, pFanin, v, vv);
    }
  }
  return;
}

void printMsfc(Abc_Ntk_t* pNtk)
{
  Abc_Obj_t* pPo;
  Abc_Obj_t* pObj;
  std::vector<std::vector<Abc_Obj_t*> > vv;
  std::vector<Abc_Obj_t*> v;
  // Abc_NtkIncrementTravId( pNtk );
  int i;
  Abc_Obj_t* pFanin;
  int j;
  // Abc_NtkForEachPo(pNtk, pPo, i) 
  // {
  //   Abc_ObjForEachFanin(pPo, pFanin, j) 
  //   {
  //     // std::cout<<"pi type = " << Abc_ObjType(pPi) << std::endl;
  //     if(!Abc_NodeIsTravIdCurrent( pFanin ) )
  //     {
  //       v.clear();
  //       Abc_NodeSetTravIdCurrent( pFanin );
  //       FindConst(pNtk, pFanin, v, vv);
  //       std::cout << "v[0] - " << Abc_ObjName( v[0]) <<std::endl;
  //       vv.push_back(v);
  //       v.clear();
  //     }
  //   }
  // }
  // std::cout << "bello" <<std::endl;
  Abc_NtkForEachObj(pNtk, pObj, i) 
  {
    // std::cout<<"pObj type = " << Abc_ObjType(pObj) << " " << Abc_ObjFanoutNum(pObj) << std::endl;
    if( Abc_ObjType(pObj) == ABC_OBJ_CONST1 && Abc_ObjFanoutNum(pObj) == 1)
    {
      v.clear();
      v.push_back(pObj);
      vv.push_back(v);
      v.clear();
    }
  }
  Abc_NtkIncrementTravId( pNtk );
  Abc_NtkForEachPo(pNtk, pPo, i) 
  {
    Abc_ObjForEachFanin(pPo, pFanin, j) 
    {
      if(!Abc_NodeIsTravIdCurrent( pFanin ) && Abc_ObjFaninNum(pFanin) != 0)
      {
        assert(v.empty());
        FindTheSameMsfc(pNtk, pFanin, v, vv);
        v.push_back(pFanin);
        vv.push_back(v);
        v.clear();
      }
      // std::cout << "fanout num = " << Abc_ObjFanoutNum(pFanin) << std::endl;
    }
  }
  Abc_Obj_t* pNode;
  // Abc_ObjForeachPi
  Abc_NtkForEachNode(pNtk, pNode,i)
  {
    if(!Abc_NodeIsTravIdCurrent( pNode ) && Abc_ObjFanoutNum(pNode) > 1 )
    {
      // std::cout << "node name : " << Abc_ObjName(pNode) << std::endl;
      assert(v.empty());
      FindTheSameMsfc(pNtk, pNode, v, vv);
      v.push_back(pNode);
      vv.push_back(v);
      v.clear();
    }
  }

  for(size_t i = 0; i < vv.size(); ++i)
  {
    sort(vv[i].begin(), vv[i].end(), compareV);
  }
  sort(vv.begin(), vv.end(), compareVV);

  for(size_t i = 0; i < vv.size(); ++i)
  {
    std::cout << "MSFC " << i << ": ";
    for(size_t j = 0; j < vv[i].size(); ++j)
    {
      std::cout << Abc_ObjName(vv[i][j]);
      if( j < vv[i].size() - 1) std::cout << ",";
      else if( j == vv[i].size() - 1 ) std::cout << std::endl;
    }
  }

}

int Lsv_CommandLsvPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  printMsfc(pNtk);
  return 0;
  
  usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;

}