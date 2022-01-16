#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include "aig/aig/aig.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <vector>

static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void init_msfc(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
}

void destroy_msfc(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t initializer = {init_msfc, destroy_msfc};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&initializer); }
} lsvPackage;


int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

  /*if (pNtk -> Abc_NtkHasAig() == 0)
  {
    printf("Input circuit is not aig.");
    return 1;
  }*/
  int i, j, k, iFanin, tmp;
  int count = 1;
  int objnum = Abc_NtkObjNum(pNtk);
  Abc_Obj_t* pObj;
  Abc_Obj_t* pPo;
  Abc_Obj_t* pNo;
  Abc_Obj_t* pFanout;
  std::vector<unsigned> fanout(objnum,0);
  std::vector<unsigned> temp(objnum,0);
  std::vector<unsigned> msfc(objnum,1);

  std::vector<int> nodes;

  Abc_NtkForEachObj( pNtk, pObj, i )
  {
    fanout[i] = (Abc_ObjFanoutNum(pObj));
    temp[i]   = (Abc_ObjFanoutNum(pObj));
  }

  Abc_NtkForEachPo( pNtk, pPo, i ) 
  {
    Abc_ObjForEachFaninId( pPo, iFanin, j )
    {
      temp[iFanin] = temp[iFanin] - 1;
      if ( temp[iFanin] == 0 )
      {
        count = count + 1;
        msfc[iFanin] = count;
        nodes.push_back(iFanin);
      }
    }
  }

  while (nodes.empty() != true){
    pPo = Abc_NtkObj(pNtk, nodes.back());
    nodes.pop_back();
    
    Abc_ObjForEachFaninId( pPo, iFanin, j ){
      temp[iFanin] = temp[iFanin] - 1;
      if ( temp[iFanin] == 0 )
      {
        pNo = Abc_NtkObj(pNtk, iFanin);
        nodes.push_back(iFanin);
        Abc_ObjForEachFanout( pNo, pFanout, k ) {
          if (k == 0) {
            tmp = msfc[Abc_ObjId(pFanout)];
            msfc[iFanin] = tmp;
          }
          if (tmp != msfc[Abc_ObjId(pFanout)]) {
            count = count + 1;
            msfc[iFanin] = count;
            break;
          }
        }
      }
    }
  }

  int c = 0;
  Abc_NtkForEachObj( pNtk, pObj, i )
  {
    if (!(Abc_ObjIsPi(pObj)) and !(Abc_ObjIsPo(pObj)))
    {
      if (msfc[i] == 1)
      {
        if(Abc_ObjType(pObj) == 1){
          printf("MSFC %d : %s \n", c, Abc_ObjName(pObj));
          c = c + 1;
          msfc[i] = 0;
        }
      }
      else if (msfc[i] > 1)
      {
        printf("MSFC %d : %s", c, Abc_ObjName(pObj));
        for (int j=i+1; j < objnum; j++){
          if (msfc[j] == msfc[i]){
            printf(",%s", Abc_ObjName(Abc_NtkObj(pNtk, j)));
            msfc[j] = 0;
          }
        }
        msfc[i] = 0;
        printf("\n");
        c = c + 1;
      }
    }
  }

  return 0;
}
