#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int debugLevel = 0;

void pa1_init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
}

void pa1_destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t pa1_frame_initializer = {pa1_init, pa1_destroy};

struct PA1PackageRegistrationManager {
  PA1PackageRegistrationManager() { Abc_FrameAddInitializer(&pa1_frame_initializer); }
} pa1PackageRegistrationManager;

void Lsv_AigTraceMSFC(Abc_Obj_t* pObj, int Level, Vec_Wec_t* msfc_mgr) {
  if ( debugLevel ) {
    printf("Object Id = %d, name = %s, isPO = %d, isLatch = %d, # of fanin = %d\n",
          Abc_ObjId(pObj), Abc_ObjName(pObj), Abc_ObjIsPo(pObj), 
          Abc_ObjIsLatch(pObj), Abc_ObjFaninNum(pObj));
    Abc_Obj_t* pFanin =  Abc_ObjFanin(pObj, 0);
    int j = 0;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
              Abc_ObjName(pFanin));
    }
  }

  Abc_Obj_t* fanIn0 = (Abc_ObjFaninNum(pObj) >= 2 && !Abc_AigNodeIsConst(pObj))? Abc_ObjFanin0(pObj) : nullptr;
  Abc_Obj_t* fanIn1 = (Abc_ObjFaninNum(pObj) >= 1 && !Abc_AigNodeIsConst(pObj))? Abc_ObjFanin1(pObj) : nullptr;

  Vec_Int_t* msfc;
  if ( pObj->fMarkA || Abc_ObjIsPi(pObj) || Abc_ObjIsPo(pObj) || Abc_ObjIsLatch(pObj) ) return ;
  pObj->fMarkA = 1;

  if ( Abc_ObjFanoutNum(pObj) > 1 ) {
    Vec_WecGrow( msfc_mgr, msfc_mgr->nCap+1 );
    msfc = Vec_WecPushLevel( msfc_mgr );
    Vec_IntGrow( msfc, 1 );
    Level = Vec_WecSize( msfc_mgr );
  }

  if ( Level == -1 ) {
    Vec_WecGrow( msfc_mgr, msfc_mgr->nCap+1 );
    msfc = Vec_WecPushLevel( msfc_mgr );
    Level = Vec_WecSize( msfc_mgr );
  } 

  if ( !fanIn0 || Abc_ObjIsPi(fanIn0) || 
      Abc_ObjIsPo(fanIn0) || Abc_ObjIsLatch(fanIn0) ) 
    fanIn0 = nullptr;

  if ( !fanIn1 || Abc_ObjIsPi(fanIn1) || 
      Abc_ObjIsPo(fanIn1) || Abc_ObjIsLatch(fanIn1) )
    fanIn1 = nullptr;

  if ( fanIn0 == nullptr || (fanIn1 != nullptr && 
        Abc_ObjId(fanIn0) > Abc_ObjId(fanIn1)) ) {
    fanIn0 = fanIn1;
    fanIn1 = nullptr;
  }

  if ( fanIn0 != nullptr )
    Lsv_AigTraceMSFC( fanIn0, Level, msfc_mgr);

  if ( fanIn1 != nullptr ) 
    Lsv_AigTraceMSFC( fanIn1, Level, msfc_mgr);

  assert( Level != -1 );
  msfc = Vec_WecEntry( msfc_mgr, Level - 1 );
  Vec_IntPushOrder( msfc, Abc_ObjId(pObj) );

}

void Lsv_AigPrintMSFC(Abc_Ntk_t* pNtk) {

  assert( Abc_NtkHasAig(pNtk) );

  Vec_Wec_t* msfc_mgr = Vec_WecAllocExact( 1 );
  Abc_Obj_t* pObj;
  int i;

  Abc_NtkForEachLiPo(pNtk, pObj, i) {
    if ( debugLevel ) {
      printf( "LiPo Id = %d, name = %s, isPO = %d, isLatch = %d, # of fanout = %d, # of fanin = %d\n",
              Abc_ObjId(pObj), Abc_ObjName(pObj), Abc_ObjIsPo(pObj), 
              Abc_ObjIsLatch(pObj), Abc_ObjFanoutNum(pObj), Abc_ObjFaninNum(pObj) );

      Abc_Obj_t* pFanin =  Abc_ObjFanin0(pObj);
      int j = 0;
      printf("  Fanin-%d: Id = %d, name = %s\n", j, 
              Abc_ObjId(pFanin),Abc_ObjName(pFanin) );
    }
    Lsv_AigTraceMSFC( Abc_ObjFanin0(pObj), -1, msfc_mgr);
  }

  Vec_WecSortByFirstInt( msfc_mgr, 0 );

  if ( debugLevel ) Vec_WecPrint( msfc_mgr, 0 );

  Vec_Int_t* vVec;
  int j;
  int Entry;

  Vec_WecForEachLevel( msfc_mgr, vVec, i) {
    printf("MSFC %d: ", i);
    Vec_IntForEachEntry( vVec, Entry, j) {
      pObj = (Abc_Obj_t *)pNtk->vObjs->pArray[ Entry ];
      printf("%s", Abc_ObjName(pObj));
      pObj->fMarkA = 0;

      if ( j != Vec_IntSize(vVec) - 1 ) printf(",");
    }
    printf("\n");
  }

  Vec_WecFree( msfc_mgr );
}

int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "hv")) != EOF) {
    switch (c) {
      case 'h':
        goto usage;
      case 'v':
        debugLevel = 1; break;
      default:
        goto usage;
    }
  }

  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  Lsv_AigPrintMSFC(pNtk);

  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the maximum single-fanout cones(MSFCs) that\n");
  Abc_Print(-2, "\t        covers all nodes of a given AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  Abc_Print(-2, "\t-v    : print the debug message\n");
  return 1;
}
