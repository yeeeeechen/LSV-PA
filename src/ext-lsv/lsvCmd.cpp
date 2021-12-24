#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
// #include "aig/aig/aig.h"
#include <set>
using namespace std;
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_MSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_OR_BIDEC(Abc_Frame_t* pAbc, int argc, char** argv);
extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_MSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_OR_BIDEC, 0);
}
//
void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;


int Lsv_OR_BIDEC(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  assert(Abc_NtkIsStrash(pNtk));
  Abc_Obj_t* pPo;
  // sat_solver * pSat = sat_solver_new();
  int i;
  // printf("h1\n");
  Abc_NtkForEachPo( pNtk, pPo, i ){
    bool flag = false;
    Abc_Ntk_t * pNtk1 = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 0);
    if ( Abc_ObjFaninC0(pPo) )
        Abc_ObjXorFaninC( Abc_NtkPo(pNtk1, 0), 0 );
    Aig_Man_t * pMan = Abc_NtkToDar(pNtk1, 0, 1 );
    int nPi = Aig_ManCiNum( pMan );
    // int pO_var = Aig_ObjId(Aig_ManCo(pMan, 0));
    Cnf_Dat_t * pCnf = Cnf_Derive( pMan, 1);
    int* cnf_PI_IDs = new int[nPi];
    int j;
    Aig_Obj_t* pObj;
    Aig_ManForEachCi(pCnf->pMan, pObj, j) {
      cnf_PI_IDs[j] = pCnf->pVarNums[Aig_ObjId(pObj)];
      // std::cout << pCnf->pVarNums[Aig_ObjId(pObj)] << "\n";
    }
    // printf("%d\n", Aig_ObjId(Aig_ManCo(pMan, 0)));
    // printf("%d\n", pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pMan, 0))]);
    int pO_var = pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pCnf->pMan, 0))];
    int VarShift = pCnf->nVars;
    sat_solver * pSat = (sat_solver *)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    lit pO_lit_0 = toLitCond(pO_var, 0);
    sat_solver_addclause( pSat, &pO_lit_0, &pO_lit_0 + 1 );

    Cnf_DataLift(pCnf, VarShift);
    // printf("nVars: %d\n", pCnf->nVars);
    lit* pBeg, * pEnd;
    // printf("h2\n");
    // sat_solver_setnvars( pSat, sat_solver_nvars(pSat) + VarShift );
    Cnf_CnfForClause( pCnf, pBeg, pEnd, j ) {
      sat_solver_addclause( pSat, pBeg, pEnd );
    }
    lit pO_lit_1 = toLitCond(pO_var + VarShift, 1);
    sat_solver_addclause( pSat, &pO_lit_1, &pO_lit_1 + 1 );

    Cnf_DataLift(pCnf, VarShift);
    // sat_solver_setnvars( pSat, sat_solver_nvars(pSat) + VarShift );
    Cnf_CnfForClause( pCnf, pBeg, pEnd, j ) {
      sat_solver_addclause( pSat, pBeg, pEnd );
    }
    lit pO_lit_2 = toLitCond(pO_var + VarShift * 2, 1);
    sat_solver_addclause( pSat, &pO_lit_2, &pO_lit_2 + 1 );
    Abc_Obj_t* pPi;
    // sat_solver_setnvars( pSat, sat_solver_nvars(pSat) + nPi * 2 );
    // printf("h3\n");
    for(j = 0;j < nPi;++j){
      // printf("%d %d\n", pPi->Id, j);
      lit* to_add = new lit[3];
      to_add[0] = toLitCond(cnf_PI_IDs[j], 1);
      to_add[1] = toLitCond(cnf_PI_IDs[j] + VarShift, 0);
      to_add[2] = toLitCond(j + VarShift * 3, 0);
      sat_solver_addclause( pSat, to_add, to_add + 3 );
      delete [] to_add;
      to_add = new lit[3];
      to_add[0] = toLitCond(cnf_PI_IDs[j], 0);
      to_add[1] = toLitCond(cnf_PI_IDs[j] + VarShift, 1);
      to_add[2] = toLitCond(j + VarShift * 3, 0);
      sat_solver_addclause( pSat, to_add, to_add + 3 );
      to_add = new lit[3];
      to_add[0] = toLitCond(cnf_PI_IDs[j], 1);
      to_add[1] = toLitCond(cnf_PI_IDs[j] + VarShift * 2, 0);
      to_add[2] = toLitCond(j + VarShift * 3 + nPi, 0);
      sat_solver_addclause( pSat, to_add, to_add + 3 );
      delete [] to_add;
      to_add = new lit[3];
      to_add[0] = toLitCond(cnf_PI_IDs[j], 0);
      to_add[1] = toLitCond(cnf_PI_IDs[j] + VarShift * 2, 1);
      to_add[2] = toLitCond(j + VarShift * 3 + nPi, 0);
      sat_solver_addclause( pSat, to_add, to_add + 3 );
      delete [] to_add;
    }
    // printf("h4\n");
    bool * Is_InFinal = new bool [2*nPi];
    for(int b = 0;b < 2*nPi; ++b){
      Is_InFinal[b] = false;
    }
    for(j = 0;j < nPi;++j){
      // printf("alpha: %d\n", alpha);
      if(flag){
        break;
      }
      for(int k = j + 1;k < nPi;++k){
        lit * assumptionList = new lit[2*nPi];
        for(int i1 = 0;i1 < nPi*2;++i1){
          if(i1 < nPi){
            if(i1 == j){
              assumptionList[i1] = toLitCond(i1 + VarShift*3, 0);
              // printf("ID1: %d\n", (i1 + 1)*2 + base);
            }else{
              assumptionList[i1] = toLitCond(i1 + VarShift*3, 1);
              // printf("Not_ID1: %d\n", (i1 + 1)*2 + base + 1);
            }
          }else{
            if(i1 == k + nPi){
              assumptionList[i1] = toLitCond(i1 + VarShift*3, 0);
              // printf("ID2: %d\n", (i1 + 1 - Abc_NtkPiNum( pNtk1 ))*2 + base + alpha);
            }else{
              assumptionList[i1] = toLitCond(i1 + VarShift*3, 1);
              // printf("Not_ID2: %d\n", (i1 + 1 - Abc_NtkPiNum( pNtk1 ))*2 + base + alpha + 1);
            }
          }
        }
        if(sat_solver_solve(pSat, assumptionList, assumptionList + 2*nPi, 0, 0, 0, 0) == l_False){
          int ** pArray = new int *;
          int pArray_size = sat_solver_final(pSat, pArray);
          // printf("%d\n", pArray_size);
          printf("PO %s support partition: 1\n", Abc_ObjName(pPo));
          for(int i2 = 0;i2 < pArray_size;++i2){
            assert(Abc_Lit2Var(pArray[0][i2]) - VarShift * 3 >= 0);
            Is_InFinal[Abc_Lit2Var(pArray[0][i2]) - VarShift * 3] = true;
          }

          for (int b = 0;b < nPi;++b){
            if(Is_InFinal[b] && Is_InFinal[b + nPi]){
              printf("0");
            }else if(Is_InFinal[b]){
              printf("2");
            }else{
              printf("1");
            }
          }
          printf("\n");
          flag = true;
          break;
        }

        // printf("\n");
        // printf("%d\n", sat_solver_final(pSat, pArray));
        // printf("%d\n", sat_solver_solve(pSat, assumptionList, assumptionList + 2*Abc_NtkPiNum( pNtk1 ), 0, 0, 0, 0));
        delete [] assumptionList;
        // break;
      }
      // break;
    }
    // break;
    if(!flag){
      printf("PO %s support partition: 0\n", Abc_ObjName(pPo));
    }
    Abc_NtkDelete(pNtk1);
  }
  return 0;
usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        decides whether each circuit PO f(X) is OR bi-decomposable\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}























struct myComp{
  bool operator() (const Abc_Obj_t* a, const Abc_Obj_t* b){
    return a->Id < b->Id;
  }
};

struct myComp1{
  bool operator() (const set <Abc_Obj_t*, myComp> a, const set <Abc_Obj_t*, myComp> b){
    return  (*(a.begin()))->Id < (*(b.begin()))->Id;
  }
};

void Lsv_Mark(Abc_Obj_t* pObj, bool flag = false){
    Abc_Obj_t* pFanin;
    int i;
    Abc_ObjForEachFanin(pObj, pFanin, i) {
      // printf("%u\n", pFanin->fMarkA);
      if (pFanin->fMarkA || pFanin->fMarkB ||  Abc_ObjIsPi(pFanin)){
        continue;
      }
      else{
        if (Abc_ObjFanoutVec(pFanin)->nSize > 1 || flag){
          pFanin->fMarkB = 1;
        }
        else {
          pFanin->fMarkA = 1;
        }
        Lsv_Mark(pFanin);
      }
    }
}

void Lsv_Construct(Abc_Obj_t* pObj, set <Abc_Obj_t*, myComp>& small){
    Abc_Obj_t* pFanin;
    int i;
    Abc_ObjForEachFanin(pObj, pFanin, i) 
    {
      if (pFanin->fMarkA || pFanin->fMarkB ||  Abc_ObjIsPi(pFanin)){
        continue;
      }
      else{
          pFanin->fMarkA = 1;
          small.insert(pFanin);
          Lsv_Construct(pFanin, small);
      }
    }
  
}
void msfc_print(unsigned count, set <Abc_Obj_t*, myComp> small){
  printf("MSFC %u: ", count);
  set <Abc_Obj_t*, myComp>::iterator iter = small.begin();
  bool flag = true;
  while(iter != small.end()){
    if(flag){
      flag = false;
    }
    else{
      printf(",");
    }
    printf("n%u", (*iter)->Id);
    ++iter;
  }
  printf("\n");
}
int Lsv_MSFC(Abc_Frame_t* pAbc, int argc, char** argv){
  Abc_Obj_t* pPo;
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int i;
  Abc_NtkForEachPo( pNtk, pPo, i ){
    Lsv_Mark(pPo, true);
  }
  Abc_Obj_t* pObj;
  Abc_NtkForEachObj(pNtk, pObj, i){
    pObj->fMarkA = 0;
  }
  set <set <Abc_Obj_t*, myComp>, myComp1> to_print;
  Abc_NtkForEachObj(pNtk, pObj, i){
    if(pObj->fMarkB == 1){
      set <Abc_Obj_t*, myComp> small;
      small.insert(pObj);
      Lsv_Construct(pObj, small);
      to_print.insert(small);
    }
  }
  unsigned count = 0;
  set <set <Abc_Obj_t*, myComp>, myComp1>::iterator iter = to_print.begin();
  while(iter != to_print.end()){
    msfc_print(count, *iter);
    ++count;
    ++iter;
  }
  Abc_NtkForEachObj(pNtk, pObj, i){
    pObj->fMarkA = 0;
    pObj->fMarkB = 0;
  }
  return 0;
usage:
  Abc_Print(-2, "usage: lsv_print_msfc\n");
  Abc_Print(-2, "\t        prints msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}









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
