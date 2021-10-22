#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>

using namespace std;

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
  Abc_Obj_t* pObj;
  int i;
  vector<int> root_Id; // vector for saving the root id
  vector< vector<Abc_Obj_t*> > MSFC; // vector for saving the MSFC
  // Iterate for all objects and build MSFC
  Abc_NtkForEachObj(pNtk, pObj, i) {
    if (!Abc_ObjIsPo(pObj) && !Abc_ObjIsPi(pObj)) { // only internal nodes
      Abc_Obj_t* Fanout; // check whether const1 is used or not
      int num_fanout = 0;
      int l;
      Abc_ObjForEachFanout(pObj, Fanout, l) {
        num_fanout++;
      }
      if (num_fanout != 0){
        Abc_Obj_t* tObj = pObj;
        while(true) {
          Abc_Obj_t* pFanout;
          int j;
          int cnt_fanout = 0;
          Abc_Obj_t* tFanout;
          Abc_ObjForEachFanout(pObj, pFanout, j) {
            if (cnt_fanout == 0) tFanout = pFanout;
            cnt_fanout++;
          }
          if ((cnt_fanout == 1) && (!Abc_ObjIsPo(tFanout))) { // single fanout and not PO
            pObj = tFanout;
          }
          else {
            bool root_exist = false;
            for (int k = 0; k < root_Id.size(); k++) {
              if (Abc_ObjId(pObj) == root_Id[k]) {
                MSFC[k].push_back(tObj);
                root_exist = true;
                break;
              }
            }
            if (!root_exist) {
              root_Id.push_back(Abc_ObjId(pObj));
              vector<Abc_Obj_t*> temp_vec;
              temp_vec.push_back(tObj);
              MSFC.push_back(temp_vec);
            }
            break;
          }
        }
      }
    }
  }
  // Sort the results
    // No need since it iterates from the small id to the large id
  // Print the decomposition
  for (int m = 0; m < MSFC.size(); m++) {
    printf("MSFC %d: ", m);
    for (int n = 0; n < MSFC[m].size(); n++) {
      if (n == 0) printf("%s", Abc_ObjName(MSFC[m][n]));
      else printf(",%s", Abc_ObjName(MSFC[m][n]));
    }
    printf("\n");
  }
}

int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  assert(  Abc_NtkIsStrash( pNtk ) );
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
  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints maximum single-fanout cones that covers all nodes of a given AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}