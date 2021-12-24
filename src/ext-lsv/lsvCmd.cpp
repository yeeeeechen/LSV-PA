#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <vector>
using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC (Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBiDec (Abc_Frame_t* pAbc, int argc, char** argv);


extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBiDec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

//helper function


void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n, #FO = %d", Abc_ObjId(pObj), Abc_ObjName(pObj), Abc_ObjFanoutNum(pObj));
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

// compare using the obj Id, assuming vector of Abc_Obj_t*
static int Lsv_VecPtrSortCompare1( void ** pp1, void ** pp2 )
{
  if ( Abc_ObjId( (Abc_Obj_t*)(*pp1) ) < Abc_ObjId( (Abc_Obj_t*)(*pp2) ) )
    return -1;
  if ( Abc_ObjId( (Abc_Obj_t*)(*pp1) ) > Abc_ObjId( (Abc_Obj_t*)(*pp2) ) ) 
    return 1;
  return 0; 
}

// compare using the first obj Id, assuming vector of (vector of Abc_Obj_t*)
static int Lsv_VecPtrSortCompare2( void ** pp1, void ** pp2 )
{
  if( Abc_ObjId( (Abc_Obj_t*)Vec_PtrEntry( (Vec_Ptr_t*)(*pp1), 0) ) <
      Abc_ObjId( (Abc_Obj_t*)Vec_PtrEntry( (Vec_Ptr_t*)(*pp2), 0) ) ){
    return -1;
  }
  if( Abc_ObjId( (Abc_Obj_t*)Vec_PtrEntry( (Vec_Ptr_t*)(*pp1), 0) ) >
      Abc_ObjId( (Abc_Obj_t*)Vec_PtrEntry( (Vec_Ptr_t*)(*pp2), 0) ) ){
    return 1;
  }
  return 0; 
}


void Lsv_AigMSFC_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes, Vec_Ptr_t* msfc, bool isRoot)
{
    Abc_Obj_t * pFanin;
    int i;

    if ( Abc_NodeIsTravIdCurrent( pNode ) || pNode->fMarkA==1 ) { return; }
    // mark the node as visited
    Abc_NodeSetTravIdCurrent( pNode );
    // skip the PI
    if ( Abc_ObjIsCi(pNode) ){ return ;}
    // push node with multiple fanout to the back of vNodes
    if ( Abc_ObjFanoutNum(pNode) > 1 && !isRoot){
      //printf("%s has %d fanout\n", Abc_ObjName(pNode), Abc_ObjFanoutNum(pNode));
      Vec_PtrPush(vNodes, pNode);
      return;
    }

    // visit the transitive fanin of the node
    Abc_ObjForEachFanin( pNode, pFanin, i )
      Lsv_AigMSFC_rec( pFanin, vNodes, msfc, 0);
    // visit the equivalent nodes
    // if ( Abc_AigNodeIsChoice( pNode ) )
    //     for ( pFanin = (Abc_Obj_t *)pNode->pData; pFanin; pFanin = (Abc_Obj_t *)pFanin->pData )
    //         Abc_AigDfs_rec( pFanin, vNodes );
    // add the node after the fanins have been added
    if(Abc_ObjIsPo(pNode)) { return; }
    Vec_PtrPush( msfc, pNode );
    pNode->fMarkA = 1;
}

Vec_Ptr_t * Lsv_AigMSFC(Abc_Ntk_t * pNtk){
  Vec_Ptr_t * msfcVec;
  Vec_Ptr_t * vNodes;
  Abc_Obj_t * pNode;
  int i;
  assert( Abc_NtkIsStrash(pNtk) );
  Abc_NtkCleanMarkA( pNtk );                                                                         

  vNodes = Vec_PtrAlloc( 100 );
  msfcVec = Vec_PtrAlloc( 100 );
  // go through the PO nodes and call for each of them
  Abc_NtkForEachCo( pNtk, pNode, i ) { Vec_PtrPush(vNodes, pNode); }
  Vec_PtrForEachEntry( Abc_Obj_t*, vNodes, pNode, i ){
    Abc_NtkIncrementTravId( pNtk );
    Vec_Ptr_t * msfc = Vec_PtrAlloc(50);
    Lsv_AigMSFC_rec(pNode, vNodes, msfc, 1);
    Vec_PtrSort(msfc, (int (*)(const void *, const void *)) Lsv_VecPtrSortCompare1);
    if( Vec_PtrSize(msfc)>0 )
      Vec_PtrPush(msfcVec, (void*)msfc);
  }
  Abc_NtkCleanMarkA( pNtk ); 
  return msfcVec;
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk){
  Vec_Ptr_t* msfcVec;
  Vec_Ptr_t* pVec;
  Abc_Obj_t* pNode;
  int i, j;
  msfcVec = Lsv_AigMSFC(pNtk);
  Vec_PtrSort(msfcVec, (int (*)(const void *, const void *)) Lsv_VecPtrSortCompare2);
  Vec_PtrForEachEntry( Vec_Ptr_t*, msfcVec, pVec, i ){
    printf("MSFC %d: ", i);
    Vec_PtrForEachEntry( Abc_Obj_t*, pVec, pNode, j ){
      printf("%s", Abc_ObjName(pNode));
      if( j!=Vec_PtrSize(pVec)-1 ) printf(",");
    }
    printf("\n");
  }
}
/***********************************/
/*********** PA2 begin *************/
/***********************************/


void Lsv_NtkPrintOrBiDec(Abc_Ntk_t* pNtk){
  int i;
  Abc_Obj_t* pCo;
  // for each PO
  Abc_NtkForEachPo(pNtk, pCo, i){
    printf("PO %s support partition: ", Abc_ObjName(pCo));
    Abc_Ntk_t* pCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pCo), Abc_ObjName(pCo), 0);
    if(Abc_ObjFaninC0(pCo))
      Abc_ObjXorFaninC( Abc_NtkPo(pCone, 0), 0);
    // create its fanin aig
    Aig_Man_t* pMan = Abc_NtkToDar(pCone, 0, 1);
    Cnf_Dat_t* pCnf = Cnf_Derive(pMan, 1);
    int varNum = pCnf->nVars;
    int piNum = Aig_ManCiNum(pMan);
    Aig_Obj_t* pPO = Aig_ManCo(pMan, 0);
    // write f(x) into solver
    // x_i => var(x_i) , 
    // var(x'_i)    = var(x_i) + 1*varNum
    // var(x''_i)   = var(x_i) + 2*varNum

    int aigNumMax = Aig_ManObjNumMax(pMan);
    // int* pID2Var = new int[aigNumMax];
    // memcpy( pID2Var, pCnf->pVarNums, sizeof(int) * aigNumMax );
    vector<int> pID2Var(aigNumMax, -1);
    for(int j=0; j<aigNumMax;++j) pID2Var[j] = pCnf->pVarNums[j];


    sat_solver* pSolver = (sat_solver*) Cnf_DataWriteIntoSolver(pCnf, 1, 0);

    // add f(x') and f(x'')
    Cnf_DataLift(pCnf, varNum);
    int _;
    int* beg; int* end;
    Cnf_CnfForClause(pCnf, beg, end, _)
      sat_solver_addclause(pSolver, beg, end );
    Cnf_DataLift(pCnf, varNum);
    Cnf_CnfForClause(pCnf, beg, end, _)
      sat_solver_addclause(pSolver, beg, end );

    // toLitCond(v, sign)
    vector<int> alpha(piNum, 0); // varNum
    vector<int> beta(piNum, 0);
    for(int j=0; j<piNum; ++j)
      alpha[j] = sat_solver_addvar(pSolver);
    for(int j=0; j<piNum; ++j)
      beta[j] = sat_solver_addvar(pSolver);

    int alpha_start = 0, alpha_end = 0;
    if(piNum){
      alpha_start = alpha[0];
      alpha_end = alpha[piNum-1];
    }

    // build (xi==xi' v a) ..., 
    for(int j=0; j<piNum; ++j){
      Aig_Obj_t* pCi = Aig_ManCi(pMan, j);
      int xi = pID2Var[ Aig_ObjId(pCi) ];

      lit cl[3]; 
      //  alpha
      cl[0] = toLitCond(xi, 1); cl[1] = toLitCond(xi+varNum, 0); cl[2] = toLitCond(alpha[j], 0);
      sat_solver_addclause(pSolver, cl, cl+3);
      cl[0] = toLitCond(xi, 0); cl[1] = toLitCond(xi+varNum, 1);
      sat_solver_addclause(pSolver, cl, cl+3);

      // beta
      cl[0] = toLitCond(xi, 1); cl[1] = toLitCond(xi+2*varNum, 0); cl[2] = toLitCond(beta[j], 0);
      sat_solver_addclause(pSolver, cl, cl+3);
      cl[0] = toLitCond(xi, 0); cl[1] = toLitCond(xi+2*varNum, 1);
      sat_solver_addclause(pSolver, cl, cl+3);
    }


    // assert f(x) !f(x') !f(x'')

    lit tmp[1] = { lit(toLitCond( pID2Var[ Aig_ObjId(pPO) ], 0)) } ;
    sat_solver_addclause(pSolver, tmp, tmp+1);
    tmp[0] = toLitCond( pID2Var[Aig_ObjId(pPO)]+varNum, 1);
    sat_solver_addclause(pSolver, tmp, tmp+1);
    tmp[0] = toLitCond( pID2Var[Aig_ObjId(pPO)]+2*varNum, 1);
    sat_solver_addclause(pSolver, tmp, tmp+1);

  
    lit* assump_list = new lit[piNum*2];
    bool found = false;

    for(int j=0; j<piNum; ++j){
      assump_list[2*j] = toLitCond(alpha[j], 1);
      assump_list[2*j+1] = toLitCond(beta[j], 1);
    }

    for(int j=0; j<piNum-1; ++j){
      for(int k=j+1; k<piNum; ++k){

        assump_list[2*j] = toLitCond(alpha[j], 0);
        assump_list[2*k+1] = toLitCond(beta[k], 0);
        //printf("seedA=%i, seedB=%i\n", j, k);
        int status = sat_solver_solve( pSolver, assump_list, assump_list+piNum*2, 0, 0, 0, 0 );

        if( status==l_False ){
          int* conflict_cls;
          int len = 0;
          printf("1\n");
          len = sat_solver_final(pSolver, &conflict_cls);

          vector<int> partition(piNum, 3);
          for(int l=0; l<len; ++l){
            int var = lit_var(conflict_cls[l]);
            assert( lit_sign(conflict_cls[l])==0 );
            int k = (var-alpha_start) % piNum ; // kth pi
            bool isBeta =  var > alpha_end ;

            partition[k] -= (isBeta ? 1 : 2);
          }
          for(int l=0; l<piNum; ++l){
            if (partition[l]==3) partition[l] = 1;
            printf("%i",  partition[l] );
          }
          printf("\n");
          found = true;
          break;
        }
        // restore to partition C
        assump_list[2*j] = toLitCond(alpha[j], 1);
        assump_list[2*k+1] = toLitCond(beta[k], 1);
      }
      if(found) break;
    }
    delete [] assump_list;
    if(!found) printf("0\n");

  }
}





/***********************************/
/************ PA2 end **************/
/***********************************/

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
  Abc_Print(-2, "\t        prints the sorted MSFC group in the aig\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandOrBiDec(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkPrintOrBiDec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        print the valid variable partition for each PO\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}