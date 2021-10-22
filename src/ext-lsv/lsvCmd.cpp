#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
	Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

struct sort_msfc{
	inline bool operator() (Abc_Obj_t* pObj1, Abc_Obj_t* pObj2){
		return (Abc_ObjId(pObj1) < Abc_ObjId(pObj2));
	}
};

struct sort_msfcs{
	inline bool operator() (vector<Abc_Obj_t*>* msfc1, vector<Abc_Obj_t*>* msfc2){
		return (Abc_ObjId(msfc1->at(0)) < Abc_ObjId(msfc2->at(0)));
	}
};


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

void Lsv_DfsMsfc(vector<vector<Abc_Obj_t*>*>* msfcs, vector<Abc_Obj_t*>* msfc, Abc_Obj_t* pObjRoot){
	int i;
	if(msfc == nullptr){
		msfc = new vector<Abc_Obj_t*>;
		msfc->push_back(pObjRoot);
		msfcs->push_back(msfc);
	}
	else{
		msfc->push_back(pObjRoot);
	}

	pObjRoot->iTemp = 1;
	Abc_Obj_t* pObj;
	Abc_ObjForEachFanin(pObjRoot, pObj, i){
	  	if(pObj->Type == ABC_OBJ_NODE || pObj->Type == ABC_OBJ_CONST1){
			if(pObj->iTemp == 0){
				if(Abc_ObjFanoutNum(pObj) == 1)
					Lsv_DfsMsfc(msfcs, msfc, pObj);
				else
					Lsv_DfsMsfc(msfcs, nullptr, pObj);

			}
	  }
	}

}

void Lsv_NtkPrintMsfc(Abc_Ntk_t* pNtk) {
  vector<vector<Abc_Obj_t*>*>* msfcs = new vector<vector<Abc_Obj_t*>*>;
  int i;
  Abc_Obj_t* pPo;
  Abc_NtkForEachPo(pNtk, pPo, i) {
	int j;	
  	Abc_Obj_t* pObjRoot;
	Abc_ObjForEachFanin(pPo, pObjRoot, j){
	  	if(pObjRoot->Type == ABC_OBJ_NODE || pObjRoot->Type == ABC_OBJ_CONST1){
			if(pObjRoot->iTemp == 0)
				Lsv_DfsMsfc(msfcs, nullptr, pObjRoot);

	  }

	}
  }
  for(i=0; i<msfcs->size(); i++){
	sort(msfcs->at(i)->begin(), msfcs->at(i)->end(), sort_msfc());
  }
  sort(msfcs->begin(), msfcs->end(), sort_msfcs());

  for(i=0; i < msfcs->size(); i++){
	printf("MSFC %d: ", i);
	int j;
	//printf("length : %d ", msfcs->at(i)->size());
	for(j=0; j < msfcs->at(i)->size(); j++){
		if(j == 0)
			printf("%s", Abc_ObjName(msfcs->at(i)->at(j)));
		else
			printf(",%s", Abc_ObjName(msfcs->at(i)->at(j)));
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

int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkPrintMsfc(pNtk);
  //Lsv_NtkPrintNodes(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
