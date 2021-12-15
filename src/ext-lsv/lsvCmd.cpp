#include <set>
#include <vector>
#include <algorithm>
#include <iostream>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLsvPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLsvOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandLsvPrintMsfc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandLsvOrBidec, 0);
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



void FindTheSameMsfc(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, std::vector<Abc_Obj_t*> &v,std::vector<std::vector<Abc_Obj_t*> > &vv) {
  Abc_Obj_t* pFanin;
  int i;
  if ( Abc_NodeIsTravIdCurrent( pObj ) || Abc_ObjFaninNum(pObj) == 0) return;
  Abc_NodeSetTravIdCurrent( pObj );
  Abc_ObjForEachFanin(pObj, pFanin, i)
  {
    if(Abc_ObjFanoutNum(pFanin) == 1 && Abc_ObjFaninNum(pFanin) != 0)
    {
      // TODO : set to the same group
      FindTheSameMsfc(pNtk, pFanin, v, vv);
      v.push_back(pFanin);
    }
    else
    {
      continue;
    }
  }
  return;
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

bool compareVV(std::vector<Abc_Obj_t*> &v1, std::vector<Abc_Obj_t*> &v2)
{
  return v1[0] -> Id < v2[0] -> Id;
}

bool compareV(Abc_Obj_t* a, Abc_Obj_t* b)
{
  return a -> Id < b -> Id;
} 

void FindConst(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, std::vector<Abc_Obj_t*> &v,std::vector<std::vector<Abc_Obj_t*> > &vv) {
  Abc_Obj_t* pFanin;
  int i;
  if ( Abc_NodeIsTravIdCurrent( pObj ) ) return;
  Abc_NodeSetTravIdCurrent( pObj );
  if(Abc_ObjType(pObj) == 1)
  {
    v.push_back(pObj);
      return;
  }
  Abc_ObjForEachFanin(pObj, pFanin, i)
  {
    if(Abc_ObjType(pFanin) == 1)
    {
      // TODO : set to the same group
      // FindTheSameMsfc(pNtk, pFanin, v, vv);
      Abc_NodeSetTravIdCurrent( pFanin );
      v.push_back(pFanin);
      return;
    }
    else
    {
      FindConst(pNtk, pFanin, v, vv);
    }
  }
  return;
}

void printMsfc(Abc_Ntk_t* pNtk)
{
  Abc_Obj_t* pPo;
  Abc_Obj_t* pObj;
  std::vector<std::vector<Abc_Obj_t*> > vv;
  std::vector<Abc_Obj_t*> v;
  // Abc_NtkIncrementTravId( pNtk );
  int i;
  Abc_Obj_t* pFanin;
  int j;
  Abc_NtkForEachObj(pNtk, pObj, i) 
  {
    // std::cout<<"pObj type = " << Abc_ObjType(pObj) << " " << Abc_ObjFanoutNum(pObj) << std::endl;
    if( Abc_ObjType(pObj) == ABC_OBJ_CONST1 && Abc_ObjFanoutNum(pObj) > 0)
    {
      v.clear();
      v.push_back(pObj);
      vv.push_back(v);
      v.clear();
    }
  }
  Abc_NtkIncrementTravId( pNtk );
  Abc_NtkForEachPo(pNtk, pPo, i) 
  {
    Abc_ObjForEachFanin(pPo, pFanin, j) 
    {
      if(!Abc_NodeIsTravIdCurrent( pFanin ) && Abc_ObjFaninNum(pFanin) != 0)
      {
        assert(v.empty());
        FindTheSameMsfc(pNtk, pFanin, v, vv);
        v.push_back(pFanin);
        vv.push_back(v);
        v.clear();
      }
      // std::cout << "fanout num = " << Abc_ObjFanoutNum(pFanin) << std::endl;
    }
  }
  Abc_Obj_t* pNode;
  // Abc_ObjForeachPi
  Abc_NtkForEachNode(pNtk, pNode,i)
  {
    if(!Abc_NodeIsTravIdCurrent( pNode ) && Abc_ObjFanoutNum(pNode) > 1 )
    {
      // std::cout << "node name : " << Abc_ObjName(pNode) << std::endl;
      assert(v.empty());
      FindTheSameMsfc(pNtk, pNode, v, vv);
      v.push_back(pNode);
      vv.push_back(v);
      v.clear();
    }
  }

  for(size_t i = 0; i < vv.size(); ++i)
  {
    sort(vv[i].begin(), vv[i].end(), compareV);
  }
  sort(vv.begin(), vv.end(), compareVV);

  for(size_t i = 0; i < vv.size(); ++i)
  {
    std::cout << "MSFC " << i << ": ";
    for(size_t j = 0; j < vv[i].size(); ++j)
    {
      std::cout << Abc_ObjName(vv[i][j]);
      if( j < vv[i].size() - 1) std::cout << ",";
      else if( j == vv[i].size() - 1 ) std::cout << std::endl;
    }
  }

}

int Lsv_CommandLsvPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv) {
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

  printMsfc(pNtk);
  return 0;
  
  usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;

}


void or_bidec(Abc_Ntk_t* pNtk)
{
  Abc_Obj_t* pObj;
  Aig_Obj_t* pObj1;
  bool bidec = false;
  std::vector<int> inputs, outputs, alphas, betas, assumps, ans;
  int var, output_lit, output_var, nfinal, alpha_start, alpha_end, beta_start, beta_end;
  int i, j, status, max = -1, ans_m, ans_n;
  int * pfinal;
  lit* poutput;
  lit tmp_lit;
  std::vector<bool> alpha_constraint, beta_constraint;
  // do the same thing for each co
  Abc_NtkForEachCo(pNtk, pObj, i)
  {
    Abc_Ntk_t * pNtkCone;
    Abc_Ntk_t * pNtkAig;
    Aig_Man_t * pAigCone;
    Cnf_Dat_t * pCnfCone;
    sat_solver * pSat;
    bidec = false;
    // find the cone for the output
    pNtkCone = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0 );
    if ( Abc_ObjFaninC0(pObj) )
        Abc_ObjXorFaninC( Abc_NtkPo(pNtkCone, 0), 0 );
    // strash the circuit
    pNtkAig = Abc_NtkStrash( pNtkCone, 0, 1, 0 );
    // turn the circuit into aig
    pAigCone = Abc_NtkToDar(pNtkAig, 0, 1);
    // derive the cnf formula for the cone circuit
    pCnfCone = Cnf_Derive(pAigCone,Aig_ManCoNum(pAigCone));
    // std::cout << "Co num = " << Aig_ManNodeNum( pAigCone )  << std::endl;
    // Cnf_DataPrint(pCnfCone,1);
    outputs.clear();
    inputs.clear();
    alphas.clear();
    betas.clear();
    assumps.clear();
    alpha_constraint.clear();
    beta_constraint.clear();
    ans.clear();
    max = -1;
    ans_m = -1;
    ans_n = -1;
    Aig_ManForEachObj(pAigCone, pObj1, j)
    {
      // std::cout<<j<<"'th obj"<<std::endl;
      var = pCnfCone -> pVarNums[pObj1 -> Id];
      if(var < 0)
        continue;
      // std::cout<<var<<" "<<pObj1 -> Type << std::endl;
      // record the number of vars
      if(var > max)
        max = var;
      // record the var names of inputs
      if(pObj1 -> Type == AIG_OBJ_CI)
      {
        inputs.push_back(var);
        // std::cout << "input name : " << var << std::endl;
      }
        // Abc_Var2Lit
      //record the var name of the output
      if(pObj1 -> Type == AIG_OBJ_CO)
      {
        outputs.push_back(var);
        // std::cout << "output name : " << var << std::endl;
      }
      // if(pObj1 -> Type == AIG_OBJ_CONST1)
      // {
      //   std::cout << "const name : " << var << std::endl;
      // }
    }
    // Cnf_DataPrint(pCnfCone,1);
    // write the cnf formula into the solver
    pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnfCone, 1, 0);
    // std::cout<<"solver size = " << pSat -> size << std::endl;
    //create a copy of cnf formula for f'(X)
    Cnf_DataLift(pCnfCone, max);
    //write the new formula into the solver
    for(size_t k = 0; k < pCnfCone -> nClauses; k++)
    {
      // printf("))))))))))))))))))))))))))))))))))))))))))))\n");
      // pSat -> fPrintClause = true;
      if (!sat_solver_addclause(pSat, pCnfCone -> pClauses[k], pCnfCone -> pClauses[k+1]))
        assert(false);
      // pSat -> fPrintClause = false;
      // printf("))))))))))))))))))))))))))))))))))))))))))))\n");
    }
    // std::cout<<"solver size = " << pSat -> size << std::endl;
    // create another copy of cnf formula for f''(X)
    Cnf_DataLift(pCnfCone, max);
    // write the new formula into the sat solver
    for(size_t k = 0; k < pCnfCone -> nClauses; k++)
    {
      if (!sat_solver_addclause(pSat, pCnfCone -> pClauses[k], pCnfCone -> pClauses[k+1]))
        assert(false);
    }
    // std::cout<<"solver size = " << pSat -> size << std::endl;
    // add f, not f', not f'' as the clauses
    // for(size_t k = 0; k < 3; ++k)
    // {
    //   if(k == 0)
    //     output = Abc_Var2Lit(outputs[0], 0);
    //   else
    //     output = Abc_Var2Lit(outputs[0], 1);
    //   sat_solver_addclause(pSat, &output, &output + 1);
    //   outputs[0] += max;
    // }
    // create alpha and beta
    alpha_start = max * 3 + 1;
    for(size_t k = 0; k < inputs.size(); ++k)
      sat_solver_addvar(pSat);
    //   std::cout << "bello = " << sat_solver_addvar(pSat) << std::endl;
    alpha_end = alpha_start + inputs.size() - 1;
    beta_start = alpha_end + 1;
    for(size_t k = 0; k < inputs.size(); ++k)
      sat_solver_addvar(pSat);
      // std::cout << "bello = " << sat_solver_addvar(pSat) << std::endl;
    beta_end = beta_start + inputs.size() - 1;
    for(int k = alpha_start; k <= alpha_end; ++k)
    {
      alphas.push_back(k);
    }
    for(int k = beta_start; k <= beta_end; ++k)
    {
      betas.push_back(k);
    }

    //add the clauses for incremental sat solving
    lit Lits[3];
    // printf("))))))))))))))))))))))))))))))))))))))))))))\n");
    // pSat -> fPrintClause = true;
    for(size_t k = 0; k < inputs.size(); ++k)
    {
      Lits[0] = Abc_Var2Lit(inputs[k], 1);
      Lits[1] = Abc_Var2Lit(inputs[k] + max, 0);
      Lits[2] = Abc_Var2Lit(alphas[k], 0);
      sat_solver_addclause(pSat, Lits, Lits+3);
      Lits[0] = Abc_Var2Lit(inputs[k], 0);
      Lits[1] = Abc_Var2Lit(inputs[k] + max, 1);
      Lits[2] = Abc_Var2Lit(alphas[k], 0);
      sat_solver_addclause(pSat, Lits, Lits+3);
      Lits[0] = Abc_Var2Lit(inputs[k], 1);
      Lits[1] = Abc_Var2Lit(inputs[k] + 2 * max, 0);
      Lits[2] = Abc_Var2Lit(betas[k], 0);
      sat_solver_addclause(pSat, Lits, Lits+3);
      Lits[0] = Abc_Var2Lit(inputs[k], 0);
      Lits[1] = Abc_Var2Lit(inputs[k] + 2 * max, 1);
      Lits[2] = Abc_Var2Lit(betas[k], 0);
      sat_solver_addclause(pSat, Lits, Lits+3);
    }
    // printf("))))))))))))))))))))))))))))))))))))))))))))\n");
    //   pSat -> fPrintClause = true;
    output_var = outputs[0];
    for(size_t k = 0; k < 3; ++k)
    {
      if(k == 0)
      {
        tmp_lit = Abc_Var2Lit(output_var, 0);
        poutput = &tmp_lit;
      }
      else
      {
        tmp_lit = Abc_Var2Lit(output_var, 1);
        poutput = &tmp_lit;
      }
        
      sat_solver_addclause(pSat, poutput, poutput+1);
      output_var += max;
    }
    // printf("))))))))))))))))))))))))))))))))))))))))))))\n");
    //   pSat -> fPrintClause = false;
    for(size_t m = 0; m < inputs.size() - 1; ++m)
    {
      for(size_t n = m + 1; n < inputs.size(); ++n)
      {
        assumps.clear();
        // std::cout<<m << " " << n << std::endl;
        for(size_t k = 0; k < alphas.size(); ++k)
        {
          if(k == m)
          {
            assumps.push_back(toLitCond(alphas[k],1));
            assumps.push_back(toLitCond(betas[k],0));
          }
          else if(k == n)
          {
            assumps.push_back(toLitCond(alphas[k],0));
            assumps.push_back(toLitCond(betas[k],1));
          }
          else
          {
            assumps.push_back(toLitCond(alphas[k],1));
            assumps.push_back(toLitCond(betas[k],1));
          }
          
          // sat_solver_set_polarity(pSat, &alphas[k], 1);
          // sat_solver_set_polarity(pSat, &betas[k], 1);
        }
        // output_var = outputs[0];
        // for(size_t k = 0; k < 3; ++k)
        // {
        //   if(k == 0)
        //   {
        //     output_lit = Abc_Var2Lit(output_var, 0);
        //   }
        //   else
        //   {
        //     output_lit = Abc_Var2Lit(output_var, 1);
        //   }
            
        //   assumps.push_back(output_lit);
        //   output_var += max;
        // }
        // std::cout << "assump size = " << assumps.size() << std::endl;
        status = sat_solver_solve(pSat, &assumps[0],&assumps[assumps.size()], 0, 0, 0, 0);
        // FILE * pFile;
        // Sat_SolverPrintStats(pFile, pSat );
        // if ( status == l_Undef )
        //     printf( "Undecided.  \n" );
        // if ( status == l_True )
        //     printf( "Satisfiable.  \n" );
        if ( status == l_False )
        {
          bidec = true;
          ans_m = m;
          ans_n = n;
          nfinal = sat_solver_final(pSat, &pfinal);
          // printf( "Unsatisfiable. \n" );
          break;
        }
        // for(size_t k = 0; k < assumps.size(); ++k)
        // {
        //   std::cout<< "assump " << k << assumps[k] <<std::endl;
        // }
      }
      if(bidec == true)
        break;
    }
    if(bidec)
    {
      alpha_constraint.reserve(inputs.size());
      beta_constraint.reserve(inputs.size());
      for(size_t k = 0; k < inputs.size(); ++k)
      {
        alpha_constraint[k] = false;
        beta_constraint[k] = false;
      }
      for(size_t k = 0; k < nfinal; ++k)
      {
        if(pfinal[k] >= (alpha_start * 2) && pfinal[k] <= (alpha_end * 2 + 1))
        {
          alpha_constraint[int(pfinal[k] / 2 - alpha_start)] = true;
        }
        if(pfinal[k] >= (beta_start * 2) && pfinal[k] <= (beta_end * 2 + 1))
        {
          beta_constraint[int(pfinal[k] / 2 - beta_start)] = true;
        }
      }
      for(size_t k = 0; k < inputs.size(); ++k)
      {
        if(k == ans_m)
        {
          ans.push_back(1);
          continue;
        }
        if(k == ans_n)
        {
          ans.push_back(2);
          continue;
        }
        if(alpha_constraint[k] == true && beta_constraint[k] == true)
        {
          ans.push_back(0);
          continue;
        }
        if(alpha_constraint[k] == true && beta_constraint[k] == false)
        {
          ans.push_back(1);
          continue;
        }
        if(alpha_constraint[k] == false && beta_constraint[k] == true)
        {
          ans.push_back(2);
        }
        if(alpha_constraint[k] == false && beta_constraint[k] == false)
        {
          ans.push_back(1);
        }
      }
      // std::cout<<std::endl;
    }
    std::cout << "PO " << Abc_ObjName(pObj) << " support partition: " << bidec << std::endl;
    if(bidec)
    {
      for(size_t k = 0; k < ans.size(); ++k)
      {
        std::cout << ans[k];
      }
      std::cout << std::endl;
    }
  }
}

int Lsv_CommandLsvOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  or_bidec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}