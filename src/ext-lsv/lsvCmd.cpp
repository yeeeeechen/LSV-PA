#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <queue>
#include <algorithm>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
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

void Lsv_PrintMSFC(Abc_Ntk_t* pNtk){
  std::queue<Abc_Obj_t*> QOO;
  std::vector<int> msfcs;
  Abc_Obj_t* pObj;
  int i;
  int count(0);
  //printf(" %d \n ",Vec_PtrSize((pNtk)->vObjs));
  Abc_NtkForEachNode(pNtk, pObj, i) {
    //printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    
	//printf("  Fanout num: %d \n",Abc_ObjFanoutNum(pObj));
    
    if (Abc_NtkHasSop(pNtk)) {
      printf("The SOP of this node:\n%s", (char*)pObj->pData);
    }
	
	if(Abc_ObjFanoutNum(pObj) != 1 || Abc_ObjType(Abc_ObjFanout(pObj, 0)) != 7){
		while(!QOO.empty()) {
			QOO.pop();
		}
		msfcs.clear();
		QOO.push(pObj);
		
		while(!QOO.empty()){
			int j;
			Abc_Obj_t* pFanin;
			msfcs.push_back( Abc_ObjId(QOO.front()));
			
			Abc_ObjForEachFanin( QOO.front(), pFanin, j ){
				if(Abc_ObjFanoutNum(pFanin) == 1 && Abc_ObjType(pFanin) == 7){
					QOO.push(pFanin);
				}
			}
			QOO.pop();
		}
		std::sort(msfcs.begin(), msfcs.end());
		printf("MSFC %d: %s",count++,Abc_ObjName(Abc_NtkObj(pNtk, msfcs[0])));
		for(int k=1;k<msfcs.size();k++){
			printf(",%s", Abc_ObjName(Abc_NtkObj(pNtk, msfcs[k])));
		}
		printf("\n");
	}
  }
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
  Lsv_PrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
