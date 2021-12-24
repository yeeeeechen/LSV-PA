#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include <iostream>
using namespace std;

extern "C" Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t*, int, int);

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBiDec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBiDec, 0);
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
void Lsv_NtkOrBiDec(Abc_Ntk_t* pNtk) {
  int i, j, k, m, n, v, status;
  Abc_Obj_t* pPo;
  Abc_NtkForEachPo(pNtk, pPo, i){
    //Changd to 0, delete 3 lines.
    Abc_Ntk_t* curNtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 0);
    /**if ( Abc_ObjFaninC0(pPo) ){
      Abc_ObjXorFaninC( Abc_NtkPo(pNtk, 0), 0 );
    }**/
    Aig_Man_t* curAig = Abc_NtkToDar(curNtk, 0, 1);
    Cnf_Dat_t* curCnf = Cnf_Derive(curAig, Aig_ManCoNum(curAig));
    sat_solver* curSat =(sat_solver*)Cnf_DataWriteIntoSolver(curCnf, 1, 0);
    lit polit[1];
    int poId;
    int shift = curCnf->nVars;
    poId = curCnf->pVarNums[Aig_ObjId(Aig_ManCo(curCnf->pMan, 0))];
    polit[0] = toLitCond(poId, pPo->fCompl0);
    sat_solver_addclause(curSat, polit, polit+1);
    //Add clauses of negated f(X').
    Cnf_DataLift(curCnf, shift);
    for ( j = 0; j < curCnf->nClauses; j++ ){
      sat_solver_addclause(curSat, curCnf->pClauses[j], curCnf->pClauses[j+1]);
    }
    poId += shift;
    polit[0] = toLitCond(poId, !pPo->fCompl0);
    sat_solver_addclause(curSat, polit, polit+1);
    //Add clauses of negated f(X'').
    Cnf_DataLift(curCnf, shift);
    for ( j = 0; j < curCnf->nClauses; j++ ){
      sat_solver_addclause(curSat, curCnf->pClauses[j], curCnf->pClauses[j+1]);
    }
    poId += shift;
    polit[0] = toLitCond(poId, !pPo->fCompl0);
    sat_solver_addclause(curSat, polit, polit+1);
    //TODO:Create control variables.
    //TODO:Add control clauses. 
    //ctrl_var[2*j] = alpha;
    //ctrl_var[2*j+1] = beta;
    Aig_Obj_t* Pin;
    int num_pi;
    Aig_ManForEachCi(curAig, Pin, num_pi){
      int alpha = sat_solver_addvar(curSat);
      int beta = sat_solver_addvar(curSat);
      int xpp = curCnf->pVarNums[Pin->Id];
      sat_solver_add_buffer_enable(curSat,xpp-(curCnf->nVars)*2 ,xpp-(curCnf->nVars), alpha, 0);
      sat_solver_add_buffer_enable(curSat,xpp-(curCnf->nVars)*2 ,xpp , beta, 0);
    }
    //Incremental SAT solving
    lit ass_list[num_pi*2];
    for (j = 0; j < num_pi-1; j++ ){
      for (k = j+1; k < num_pi; k++ ){
        //TODO:Reset the assumption list.
        for (m = 0; m < num_pi*2; m++){

          if(m%2 == 0){
            if(m/2 == j){
              ass_list[m] = toLitCond((curCnf->nVars)*3+m, 0);
            }
            else{
              ass_list[m] = toLitCond((curCnf->nVars)*3+m, 1);
            }
          }
          else{
            if((m-1)/2 == k){
              ass_list[m] = toLitCond((curCnf->nVars)*3+m, 0);
            }
            else{
              ass_list[m] = toLitCond((curCnf->nVars)*3+m, 1);
            }
          }
        }
        //TODO:SAT solving.
        status = sat_solver_solve(curSat, ass_list, ass_list + num_pi*2, 0, 0, 0, 0);
        if(status == l_False) break;
      }
    }
    //Print result.
    if(status == l_False){
      printf("PO %s support patition: 1\n", Abc_ObjName(pPo));
      //TODO:Final conflict clause.
      int* fin_con;
      int num_con;
      num_con = sat_solver_final(curSat, &fin_con);
      //TODO:Print the variable partition.
      int alpha_val[num_pi];
      int beta_val[num_pi];
      for (j = 0; j < num_pi; j++){
        alpha_val[j] = 1;
        beta_val[j] = 1;
      }
      for (k = 0; k < num_con; k++){
        int index = fin_con[k]/2-3*(curCnf->nVars);
        if(index%2 == 0){
          alpha_val[index/2] = 0;
        }
        else{
          beta_val[index/2] = 0;
        }
      }
      for (j = 0; j < num_pi; j++){
        if(alpha_val[j] == 0){
          if(beta_val[j] == 0){
            //C
            printf("0");
          }
          else{
            //B
            printf("1");
          }
        }
        else{
          printf("2");
        }
      }
      printf("\n");
    }
    else{
      printf("PO %s support patition: 0\n", Abc_ObjName(pPo));
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
int Lsv_CommandOrBiDec(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkOrBiDec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        print the or bi-decomposition of each PO, return 0 if ther are none.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
