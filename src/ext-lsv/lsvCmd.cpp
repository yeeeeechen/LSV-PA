#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <algorithm>
using namespace std;
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
void find_MSFC(Abc_Obj_t* pObj);
void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);//在這邊加入自己的command 
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;
/////////////////////////////////////////////////////////////////////////////////////////////////////
void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk)
{
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),Abc_ObjName(pFanin));
    }
    if (Abc_NtkHasSop(pNtk)) {
      printf("The SOP of this node:\n%s", (char*)pObj->pData);
    }
  }
}


std::vector<Abc_Obj_t*> cone;
std::vector<vector<Abc_Obj_t*> > cone_list;
int MSFC_num=0;

bool compare_function (Abc_Obj_t* n1, Abc_Obj_t* n2)
{
	return Abc_ObjId(n1)<Abc_ObjId(n2);
}

bool compare_function2 (vector<Abc_Obj_t*> n1, vector<Abc_Obj_t*> n2)
{
	return Abc_ObjId(n1[0])<Abc_ObjId(n2[0]);
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk)
{
  cone_list.clear();
  Abc_Obj_t* pObj;
  int i;
  Abc_Obj_t* pFanout;
  int j;
  int po_flag = 0;
  Abc_NtkForEachObj(pNtk, pObj, i)
  {
  	if((Abc_ObjType(pObj)==ABC_OBJ_NODE) || (Abc_ObjType(pObj)==ABC_OBJ_CONST1))
  	{
  		cone.clear();
	    po_flag = 0;
	    Abc_ObjForEachFanout(pObj,pFanout,j)
	    {
			if(Abc_ObjIsPo(pFanout)) po_flag = 1;
	    }
	    
	    if((Abc_ObjType(pObj)==ABC_OBJ_CONST1)&&(Abc_ObjFanoutNum(pObj)>=1))
	    {
	    	cone.push_back(pObj);
	    	cone_list.push_back(cone);
	    	MSFC_num++;
		}
	    else if(((Abc_ObjFanoutNum(pObj)!=1) || ((Abc_ObjFanoutNum(pObj)==1) & po_flag)) && (Abc_ObjType(pObj)!=ABC_OBJ_CONST1))
	    {
	    	cone.push_back(pObj);
	    	find_MSFC(pObj);
			std::sort(cone.begin(), cone.end(), compare_function);
	        cone_list.push_back(cone);
	        MSFC_num++;
	    }
	    
	}
    
    
  }
  
  std::sort(cone_list.begin(), cone_list.end(), compare_function2);
  for(i=0;i<MSFC_num;i++)
  {
  	int flag=0;
  	printf("MSFC %d: ",i);
  	vector<Abc_Obj_t*>::iterator print_cone;
  	for(print_cone=cone_list[i].begin();print_cone!=cone_list[i].end();print_cone++)
    {
	    if(flag==0) printf("%s",Abc_ObjName(*print_cone));
	    else printf(",%s",Abc_ObjName(*print_cone));
	    flag=1;
    }
    printf("\n");
  }
}

void find_MSFC(Abc_Obj_t* pObj)
{
  Abc_Obj_t* pFanin;
  int j;
  Abc_ObjForEachFanin(pObj, pFanin, j)
  {
      if((Abc_ObjFanoutNum(pFanin)==1)&&(!(Abc_ObjIsPi(pFanin))))
      {
        cone.push_back(pFanin);
        find_MSFC(pFanin);
      }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
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


int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv){
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
  Lsv_NtkPrintMSFC(pNtk);	//導向函式Lsv_NtkPrintMSFC 
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

