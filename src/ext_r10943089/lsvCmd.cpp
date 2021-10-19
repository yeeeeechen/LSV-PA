#include <vector>
#include <map>
#include <queue>
#include <string>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

using namespace std;

namespace {

bool sortrow( Abc_Obj_t* Obj1, Abc_Obj_t* Obj2 ) { return Abc_ObjId(Obj1) < Abc_ObjId(Obj2); }
bool sortcol( const vector<Abc_Obj_t*>& v1, const vector<Abc_Obj_t*>& v2 ) { 
  // printf("v1 size: %d, %s / v2 size: %d, %s\n", v1.size(), Abc_ObjName(v1[0]), v2.size(), Abc_ObjName(v2[0]));
  return Abc_ObjId(v1[0]) < Abc_ObjId(v2[0]); 
}

static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  Abc_Obj_t *pObj, *pPo, *pFanin;
  int i, j, msfcIndex = 0;
  
  map<Abc_Obj_t*, int> _msfcMap; // object to index in _msfc
  queue<Abc_Obj_t*> _objQueue; // queue to avoid recursive call

  // initial map with index = -1
  // printf("=====Traverse each node and Po=====\n");
  bool const1 = false;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    if (!Abc_ObjIsPo(pObj)) { _msfcMap[pObj] = -1; }
  }
  // initial PO's index and push into queue
  Abc_NtkForEachPo(pNtk, pPo, i) {
    Abc_ObjForEachFanin(pPo, pFanin, j) {
      if ((!Abc_ObjIsPi(pFanin) && _msfcMap[pFanin] == -1))
      {
        _msfcMap[pFanin] = msfcIndex;
        // printf("PO FanIn Id = %d, name = %s, index= %d\n", Abc_ObjId(pFanin), Abc_ObjName(pFanin), msfcIndex);
        _objQueue.push(pFanin);
        msfcIndex++;
      }
      if(Abc_AigNodeIsConst(pFanin) && !const1)
      {
        // printf("Po FanIn is Const1. %s\n", Abc_ObjName(pFanin));
        _msfcMap[pFanin] = msfcIndex;
        msfcIndex++;
        const1 = true;
      }
    }
  }

  // from queue calc their index in msfc
  // printf("=====Construct MSFC ======\n");
  while(!_objQueue.empty()) {
    pObj = _objQueue.front();
    _objQueue.pop();
    // printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_ObjForEachFanin(pObj, pFanin, i) {
      // printf("  pFanin Id = %d, name = %s\n", Abc_ObjId(pFanin), Abc_ObjName(pFanin));
      if (!Abc_ObjIsPi(pFanin) && _msfcMap[pFanin] == -1) {
        if (Abc_ObjFanoutNum(pFanin) == 1) {
          _msfcMap[pFanin] = _msfcMap[pObj];
          // printf("    pFanin is the same msfc as its parent! (%d)\n", _msfcMap[pFanin]);
        } else {
          _msfcMap[pFanin] = msfcIndex;
          // printf("    New msfc is construct (%d)!\n", msfcIndex);
          msfcIndex++;
        }
        _objQueue.push(pFanin);
      }
    }
  }
  // printf("=====Construct MSFC finished=====\n");

  // push_back to _msfc
  // printf("=====push to _msfc======\n");
  // printf("total %d MSFC\n", msfcIndex);
  vector<vector<Abc_Obj_t*> > _msfc(msfcIndex);
  map<Abc_Obj_t*, int>::iterator it;
  for (it = _msfcMap.begin(); it != _msfcMap.end(); it++) {
    if (it->second == -1) { printf("something wrong!! %s\n", Abc_ObjName(_msfc[i][j])); }
    if (!Abc_ObjIsPi(it->first) && !Abc_ObjIsPo(it->first)) {
      // printf("Object Id = %d, name = %s, index = %d\n", Abc_ObjId(it->first), Abc_ObjName(it->first), it->second);
      _msfc[it->second].push_back(it->first);  
    }
  }

  // sort _msfc by Abc_ObjId() twice
  // printf("=====sort _msfc======\n");
  for (i = 0; i < _msfc.size(); i++) {
    // printf("_msfc[%d] size: %d\n", i, _msfc[i].size());
    sort(_msfc[i].begin(), _msfc[i].end(), sortrow);
  }
  // printf("_msfc size: %d\n", _msfc.size());
  sort(_msfc.begin(), _msfc.end(), sortcol);
  
  // print message
  // printf("=====print _msfc======\n");
  for(i = 0; i < _msfc.size(); i++) {
    printf("MSFC %d: ", i);
    for(j = 0; j < _msfc[i].size()-1; j++) {
      printf("%s,", Abc_ObjName(_msfc[i][j]));
    }
    printf("%s\n", Abc_ObjName(_msfc[i][j]));
  }
  // printf("=====End function======\n");
}

int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  // print helping message
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  // empty network
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  // call main function
  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        find maximum single-fanout cones (MSFCs) that covers all nodes (excluding PIs and POs) of a given AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

}