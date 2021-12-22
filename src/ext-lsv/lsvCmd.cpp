#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include <vector>
#include <algorithm>
#include <unordered_map>

extern "C" Aig_Man_t* Abc_NtkToDar( Abc_Ntk_t* pNtk, int fExors, int fRegisters );

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
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

void msfc_traversal(Abc_Obj_t *pObj ,std::vector<Abc_Obj_t*>& vec, std::vector<Abc_Obj_t*>& Ids, std::unordered_map<Abc_Obj_t*, int>& flag){
  int j;
  Abc_Obj_t *pFanin;
  std::vector<Abc_Obj_t*> tmp;
  Abc_ObjForEachFanin(pObj, pFanin, j) {
    if(Abc_ObjIsNode(pFanin) && Abc_ObjFanoutNum(pFanin) == 1){ 
      flag[pFanin] = -1;
      tmp.push_back(pFanin);
    }
    // if(pFanin->Type == ABC_OBJ_CONST1) {
    //   flag[pFanin] = -1;
    //   tmp.push_back(pFanin);
    // }
  }
  for(int i=tmp.size()-1; i>-1; --i) {
    vec.push_back(tmp[i]); 
    msfc_traversal(tmp[i], vec, Ids, flag);
  }
  tmp.clear();
  
  return;
}

bool msfccompare(std::vector<Abc_Obj_t*>& a, std::vector<Abc_Obj_t*>&  b) {
    return Abc_ObjId(a[a.size()-1]) < Abc_ObjId(b[b.size()-1]); 
}

bool Obj_cmp(Abc_Obj_t*& a, Abc_Obj_t*& b) {
    return Abc_ObjId(a) > Abc_ObjId(b);
}

void Lsv_NtkPrintMsfc(Abc_Ntk_t* pNtk) {
  Abc_Obj_t *pObj, *tmp_Obj;
  int i = 0;
  Abc_NtkIncrementTravId(pNtk);
  std::vector<Abc_Obj_t*> Ids, vec;
  std::unordered_map<Abc_Obj_t*, int> flag;
  std::vector<std::vector<Abc_Obj_t*>> msfc;
  std::vector<Abc_Obj_t*>::iterator iter;
  Abc_NtkForEachObj(pNtk, pObj, i){
    if(pObj->Type == ABC_OBJ_CONST1 && Abc_ObjFanoutNum(pObj) != 0) {
      Ids.push_back(pObj);
      flag[pObj] = 0;
    }
    else if(Abc_ObjIsNode(pObj)) {
      Ids.push_back(pObj);
      flag[pObj] = 0;
    } 
  }
  for(int i=Ids.size()-1; i>-1; --i){
    if(flag[Ids[i]] == -1){
      continue;
    }
    else{
      tmp_Obj = Ids[i];
      flag[tmp_Obj] = -1;
      vec.push_back(tmp_Obj);
      msfc_traversal(tmp_Obj, vec, Ids, flag);
      std::sort(vec.begin(), vec.end(), Obj_cmp);
      msfc.push_back(vec);
      vec.clear();
    }
  }
  std::sort(msfc.begin(), msfc.end(), msfccompare);
  for(int k=0; k<msfc.size(); ++k){
    printf("MSFC %d: ", k);
    for(int l=msfc[k].size()-1; l>-1; --l){
      if(l == msfc[k].size()-1) printf("%s", Abc_ObjName(msfc[k][l]));
      else printf(",%s", Abc_ObjName(msfc[k][l]));
    }
    printf("\n");
  }
  return;
}

void Lsv_NtkOrBidec(Abc_Ntk_t* pNtk) {
  int i, j, POid = 0, status, pNum = 0, pVarNum = 0, pCiNum = 0;
  std::vector<int> vars1, vars2, vars3, alpha, beta, CiIds, result;
  Abc_Obj_t* pObj;
  Aig_Obj_t* pAigObj;
  lit Lit1[1], Lit2[3];
  int flag = 0;
  std::vector<int> assumption;
  Abc_NtkForEachCo(pNtk, pObj, i ){
    pNum = 0;
    pCiNum = 0;
    pVarNum = 0;
    vars1.clear();
    vars2.clear();
    vars3.clear();
    alpha.clear();
    beta.clear();
    CiIds.clear();
    result.clear();
    // construct cone
    Abc_Ntk_t* pNtk1 = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0); 
    if ( Abc_ObjFaninC0(pObj) ){
      Abc_ObjXorFaninC( Abc_NtkPo(pNtk1, 0), 0 );
    }
    Abc_Ntk_t* pAig = Abc_NtkStrash(pNtk1, 0, 1, 0);
    Aig_Man_t* pMan = Abc_NtkToDar(pAig, 0, 1);
    Aig_ManForEachObj(pMan, pAigObj, j) {
      if (pAigObj->Type == 3) {
        POid = pAigObj->Id; 
      }
      if (pAigObj->Type == 2) {
        CiIds.push_back(pAigObj->Id);
        pCiNum ++;
      }
      pNum ++;
    }
    Cnf_Dat_t* pCnf = Cnf_Derive(pMan, 1);

    for (size_t k=0; k<pNum; ++k) {
      vars1.push_back(pCnf->pVarNums[k]);
      if (pCnf->pVarNums[k] != -1) {
        pVarNum ++;
      }
    }
    sat_solver* pSat = (sat_solver*) Cnf_DataWriteIntoSolver(pCnf, 1, 0);  

    // add f1(x)
    Lit1[0] = Abc_Var2Lit(pCnf->pVarNums[POid], 0);
    sat_solver_addclause( pSat, Lit1, Lit1+1);

    
    // add ~f2(x)
    Cnf_DataLift(pCnf, pVarNum);
    for (size_t k=0; k<pNum; ++k) {
      vars2.push_back(pCnf->pVarNums[k]);
    }
    for (j=0; j<pCnf->nClauses; ++j) {
      sat_solver_addclause( pSat, pCnf->pClauses[j], pCnf->pClauses[j+1] );
    }
   
    Lit1[0] = Abc_Var2Lit(pCnf->pVarNums[POid], 1);
    sat_solver_addclause(pSat, Lit1, Lit1+1);
    
    // add ~f3(x)
    Cnf_DataLift( pCnf, pVarNum);
    for (size_t k=0; k<pNum; ++k) {
      vars3.push_back(pCnf->pVarNums[k]);
    }
    for (j=0; j<pCnf->nClauses; ++j) {
      sat_solver_addclause(pSat, pCnf->pClauses[j], pCnf->pClauses[j+1] );
    }
    Lit1[0] = Abc_Var2Lit(pCnf->pVarNums[POid], 1);
    sat_solver_addclause(pSat, Lit1, Lit1+1);
    

    // add (alpha, beta) to the sat_xolver
    sat_solver_setnvars(pSat, 2*pCiNum);
    for (size_t j=0; j<pCiNum; ++j) {
      alpha.push_back(3*pVarNum+1+j);
      beta.push_back(3*pVarNum+pCiNum+1+j);
    }
    
    // add [(x1 == x1') V a] and [(x1 ==x1'') V b]
    for (size_t j=0; j<pCiNum; ++j) {
      
      Lit2[0] = Abc_Var2Lit(vars1[CiIds[j]], 1);
      Lit2[1] = Abc_Var2Lit(vars2[CiIds[j]], 0);
      Lit2[2] = Abc_Var2Lit(alpha[j], 0);
      sat_solver_addclause( pSat, Lit2, Lit2+3);
      Lit2[0] = Abc_Var2Lit(vars2[CiIds[j]], 1);
      Lit2[1] = Abc_Var2Lit(vars1[CiIds[j]], 0);
      Lit2[2] = Abc_Var2Lit(alpha[j], 0);
      sat_solver_addclause( pSat, Lit2, Lit2+3);
      Lit2[0] = Abc_Var2Lit(vars1[CiIds[j]], 1);
      Lit2[1] = Abc_Var2Lit(vars3[CiIds[j]], 0);
      Lit2[2] = Abc_Var2Lit(beta[j], 0);
      sat_solver_addclause( pSat, Lit2, Lit2+3);
      Lit2[0] = Abc_Var2Lit(vars3[CiIds[j]], 1);
      Lit2[1] = Abc_Var2Lit(vars1[CiIds[j]], 0);
      Lit2[2] = Abc_Var2Lit(beta[j], 0);
      sat_solver_addclause( pSat, Lit2, Lit2+3);   
    }
    
    // do assumption
    if (pCiNum <= 1) {
      printf( "PO %s support partition: 0\n", Abc_ObjName(pObj));
    } else {
        for (size_t j=0; j<pCiNum-1; ++j) {
          for (size_t k=j+1; k<pCiNum; ++k) {
            for (size_t l=0; l<pCiNum; ++l) {
              if (l == j) {
                assumption.push_back(Abc_Var2Lit(alpha[l], 0));
                assumption.push_back(Abc_Var2Lit(beta[l], 1));
              } else if (l == k) {
                assumption.push_back(Abc_Var2Lit(alpha[l], 1));
                assumption.push_back(Abc_Var2Lit(beta[l], 0));
              } else {
                assumption.push_back(Abc_Var2Lit(alpha[l], 1));
                assumption.push_back(Abc_Var2Lit(beta[l], 1));
              }
            } 
            status = sat_solver_solve( pSat, &assumption[0], &assumption[0]+2*pCiNum, 0, 0, 0, 0 );
            if ( status == l_False ) {
              printf( "PO %s support partition: 1\n", Abc_ObjName(pObj));
              int* pLits;
              for (size_t m=0; m<2*pCiNum; ++m) {
                result.push_back(0);
              }
              result[j] = 1;
              result[k+pCiNum] = 1;
              for (size_t m=0; m<sat_solver_final(pSat, &pLits); ++m){
                result[*(pLits + m)/2-3*pVarNum-1] = 0;
              }
              for (size_t m=0; m<pCiNum; ++m) {
                if(result[m] == 1 && result[m+pCiNum] == 0) {
                  printf("2");
                } else if (result[m] == 0 && result[m+pCiNum] == 0) {
                  printf("0");
                } else {
                  result[m] = 0;
                  printf("1");
                }
              }
              printf("\n");
              flag = 1;
              assumption.clear();
              break;
            }
            assumption.clear();
            if (flag == 1) break;
          }
          if (flag == 1) break;
        }
        if (flag == 0) {
          printf( "PO %s support partition: 0\n", Abc_ObjName(pObj));
        }
        flag = 0;
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
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints all msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkOrBidec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        prints the satisfiability\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
