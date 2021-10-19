#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <set>
#include <algorithm>
#include <iostream>
#include <vector>
#include <queue>

static int Lsv_CommandMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void MSFCinit(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandMSFC, 0);
}

void MSFCdestroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t Msfc_frame_initializer = {MSFCinit, MSFCdestroy};

struct Msfc_PackageRegistrationManager {
  Msfc_PackageRegistrationManager() { Abc_FrameAddInitializer(&Msfc_frame_initializer); }
} Msfc_lsvPackageRegistrationManager;

void _Lsv_print_nodes(Abc_Ntk_t* pNtk) {
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


class VectorComparator
{
public:
  static bool singleComp(Abc_Obj_t* a,Abc_Obj_t* b)
  {
    return Abc_ObjId(a)<Abc_ObjId(b);
  }
  static bool vecComp(const std::vector<Abc_Obj_t*>& a,const std::vector<Abc_Obj_t*>& b)
  {
    return Abc_ObjId(a.at(0))<Abc_ObjId(b.at(0));
  }
};

void Lsv_MSFC(Abc_Ntk_t* pNtk) {
  using std::cout;
  using std::endl;
  Abc_Obj_t* pObj;
  Abc_Obj_t* pFanin;
  int i,j;
  std::set<Abc_Obj_t*> has_v;
  std::queue<Abc_Obj_t*> todos;
  std::vector<std::vector<Abc_Obj_t*> > cones;
  int nodeNum=Abc_NtkNodeNum(pNtk);
  int nowNum=0;
  Abc_NtkForEachPo(pNtk,pObj,i)
  {
    todos.push(Abc_ObjFanin0(pObj));
    //cout<<Abc_ObjId(pObj)<<endl;
  }
  //while(nowNum<nodeNum)
  while(!todos.empty())
  {
    std::queue<Abc_Obj_t*> single_todo;
    std::vector<Abc_Obj_t*> cone_vec;
    Abc_Obj_t* top=todos.front();
    std::set<Abc_Obj_t*> cone;
    todos.pop();
    if(has_v.find(top)==has_v.end())
    {
      has_v.insert(top);
    }
    else
    {
      continue;
    }
    //cout<<"v : "<<Abc_ObjId(top)<<endl;
    if(Abc_ObjIsPi(top))
    {
      continue;
    }
    cone.insert(top);
    single_todo.push(top);
    while(!single_todo.empty())
    {
      Abc_Obj_t* topfanin;
      Abc_ObjForEachFanin(single_todo.front(),topfanin,j)
      {
        if(Abc_ObjIsPi(topfanin))
        {
          //single_todo.pop();
          //continue;
        }
        else if(Abc_ObjFanoutNum(topfanin)==1&&cone.find(Abc_ObjFanout(topfanin,0))!=cone.end())
        {
          cone.insert(topfanin);
          single_todo.push(topfanin);
        }
        else
        {
          todos.push(topfanin);
        }
      }
      single_todo.pop();
    }
    for(std::set<Abc_Obj_t*>::iterator it=cone.begin();it!=cone.end();++it)
    {
      cone_vec.push_back(*it);
    }
    cones.push_back(cone_vec);
    nowNum+=cone_vec.size();
  }
  /*for(size_t i=0;i<cones.size();++i)
  {
    cout<<"MSFC "<<i<<": ";
    for(size_t j=0;j<cones.at(i).size();++j)
    {
      cout<<Abc_ObjName(cones.at(i).at(j))<<"("<<Abc_ObjId(cones.at(i).at(j))<<")";
      if(j!=cones.at(i).size()-1)
      {
        cout<<",";
      }
    }
    cout<<endl;
  }
  cout<<"------------------------------------------------"<<endl;*/
  for(size_t i=0;i<cones.size();++i)
  {
    sort(cones[i].begin(),cones[i].end(),VectorComparator::singleComp);
  }
  sort(cones.begin(),cones.end(),VectorComparator::vecComp);
  for(size_t i=0;i<cones.size();++i)
  {
    cout<<"MSFC "<<i<<": ";
    for(size_t j=0;j<cones.at(i).size();++j)
    {
      cout<<Abc_ObjName(cones.at(i).at(j));//<<"("<<Abc_ObjId(cones.at(i).at(j))<<")";
      if(j!=cones.at(i).size()-1)
      {
        cout<<",";
      }
    }
    cout<<endl;
  }
}


int Lsv_CommandMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_MSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}