#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <algorithm>
using namespace std;
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintNodes, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;
void recvisit(Abc_Obj_t* pObj,int count,vector <int>* a)
{
  for(int j=0;j<Abc_ObjFaninNum(pObj);j++)
  {
    if(Abc_ObjIsNode(Abc_ObjFanin(pObj,j))&&Abc_ObjFanoutNum(Abc_ObjFanin(pObj,j))==1)
    {
      a->push_back(Abc_ObjId(Abc_ObjFanin(pObj,j)));
      recvisit(Abc_ObjFanin(pObj,j),count,a);
    }
  }
}
bool vcompare(vector<int>& a,vector<int>& b)
{
  return a[0]<b[0];
}

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  int count=0;
  vector <vector <int> > a(Abc_NtkNodeNum(pNtk)+1);

  if(Abc_ObjFanoutNum(Abc_NtkObj(pNtk,0))!=0)
  {
    if(Abc_ObjFanoutNum(Abc_NtkObj(pNtk,0))>1||(Abc_ObjIsPo(Abc_ObjFanout0(Abc_NtkObj(pNtk,0)))))
    {
      vector<int> c;
      c.push_back(0);
      a[count]=c;
      count++;
    }
  }
  
  Abc_NtkForEachNode(pNtk, pObj, i){
      if(Abc_ObjFanoutNum(pObj)>1||Abc_ObjIsPo(Abc_ObjFanout0(pObj)))
      {
        vector<int> c;
        c.push_back(Abc_ObjId(pObj));
        recvisit(pObj,count,&c);
        a[count]=c;
        count++;
      }
  }
  for(int j=0;j<count;j++)
  {
    
    sort(a[j].begin(),a[j].end());
     
  }
  a.resize(count);
  sort(a.begin(),a.end(),vcompare);
  for(int j=0;j<count;j++)
  {

    printf("MSFC %d: ",j);
    for(int n=0;n<a[j].size();n++)
    {
      if(n==0)
        printf("n%d",a[j][0]);
      else
        printf(",n%d",a[j][n]);
    }
    printf("\n");
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
