#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_MSFC(Abc_Frame_t* pAbc, int argc, char** argv);


set<int> get_MFSC(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, set<int> s);



void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_MSFC, 0);
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

void Lsv_NtkMSFC(Abc_Ntk_t* pNtk)
{
	Abc_Obj_t* pObj;
	int i;
	vector< set<int> > MFSC;
	Abc_NtkForEachObj(pNtk, pObj, i)
	{

		//printf("Obj Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
		bool root = false;
		//cout << Abc_ObjFanoutNum(pObj) <<endl;
		if(Abc_ObjFanoutNum(pObj)==1)
		{
			int y;
			y = Abc_ObjFanout(pObj,0) -> Type;
			//cout << y <<endl;
			if( y!=1 && y!=7 )
			{
				root = true;
			}
		}
		else if(Abc_ObjFanoutNum(pObj)>1)
			root = true;

		int z;
		z = pObj -> Type;
		//cout << z <<endl;

		if(pObj -> Type !=1 && pObj -> Type != 7)
			root = false;
		set<int> s;
		if(root)
		
		{
			//printf("root\n");
			s.insert(Abc_ObjId(pObj));
			s = get_MFSC(pNtk, pObj,s);	
			MFSC.push_back(s);
		}

	}
	sort(MFSC.begin(), MFSC.end());
	for(int j=0; j<MFSC.size() ; j++)
	{
		printf("MSFC %d: ",j);
		for (auto it = MFSC[j].begin(); it!= MFSC[j].end(); it++)
		{
			if( it != MFSC[j].begin() )
				printf(",");
				//cout <<",";
			//cout<<"n"<< *it ;
			printf("n%d",*it);
		}
		printf("\n");
	}
}

set<int> get_MFSC(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, set<int> s)
{
	int i;
	Abc_Obj_t* pFi;
	Abc_ObjForEachFanin(pObj,pFi, i)
	{
		if(Abc_ObjFanoutNum(pFi) == 1)
		{
			int y = pFi -> Type;
			if(y==1 || y==7)
			{
				//printf("%d\n", Abc_ObjId(pFi));
				s.insert(Abc_ObjId(pFi));
				s = get_MFSC(pNtk, pFi, s);
		
			}
		}
	}
	return s;
}


int Lsv_MSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
