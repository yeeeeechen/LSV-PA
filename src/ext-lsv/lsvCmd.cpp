#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"

extern "C" Aig_Man_t * Abc_NtkToDar(Abc_Ntk_t *, int fExors, int fRegisters);
extern "C" Cnf_Dat_t * Cnf_Derive(Aig_Man_t *, int nOutputs);

using namespace std;

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int LSV_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int LSV_CommandBIDEC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", LSV_CommandBIDEC, 0);
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

void BIDEC(Abc_Ntk_t* pNtk)
{
    Abc_Obj_t* Po;
    int i;
    Abc_NtkForEachCo(pNtk, Po, i)
    {
        Abc_Ntk_t * pCone;
        Aig_Man_t * pAig;
        Cnf_Dat_t * pCnf;
        sat_solver * pSat;
        
        //bool SAT;
        //SAT = true;
        //int name;
        //name = Po -> Id;
        //name = 1;
        
        //printf("start \n");

        pCone = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(Po), Abc_ObjName(Po), 0 );
        if ( Abc_ObjFaninC0(Po) )
            Abc_ObjXorFaninC( Abc_NtkPo(pCone, 0), 0 );

        //printf("cone \n");
        
        pCone = Abc_NtkStrash( pCone, 0, 1, 0 );
        pAig = Abc_NtkToDar(pCone,0,1);
        //printf("AIG \n");
        
        pCnf = Cnf_Derive(pAig, 1);
        //printf("CNF \n");
        
        pSat = (sat_solver*) Cnf_DataWriteIntoSolver(pCnf, 1, 0);
        //printf("SAT \n");
        
        //printf("structure finish \n");
        
        Aig_Obj_t* Aobj;
        int shift;
        shift = 0;
        vector<int> variables;
        int vnum; // variable number
        int o_tag; // output tag
        int j; // iterator
        
        // data for sat
        Aig_ManForEachObj(pAig, Aobj, j)
        {
            vnum = pCnf->pVarNums[Aobj->Id];
            if (vnum > shift)
            {
                shift = vnum;
            }
            if (Aig_ObjType(Aobj) == AIG_OBJ_CI)
            {
                variables.push_back(Aobj -> Id);
            }
            if (Aig_ObjType(Aobj) == AIG_OBJ_CO)
            {
                o_tag = Aobj -> Id;
            }
        }
        
        // CNF output clause
        int IPsize = variables.size();
        int f_p = pCnf -> pVarNums[o_tag];
        int f_lit = Abc_Var2Lit(f_p, 0);
        int *f = &f_lit;
        sat_solver_addclause(pSat, f, f+1);
        
        // input
        vector<int> IP_list, IP_prime_list, IP_prime2_list;
        for (int k = 0 ; k < IPsize ; ++k)
        {
            IP_list.push_back(pCnf->pVarNums[variables[k]]);
            IP_prime_list.push_back(pCnf->pVarNums[variables[k]] + shift);
            IP_prime2_list.push_back(pCnf->pVarNums[variables[k]] + 2*shift);
        }
        
        // first negation
        Cnf_DataLift(pCnf, shift);
        int f_prime_lit = Abc_Var2Lit(f_p + shift, 1);
        int *f_prime = &f_prime_lit;
        sat_solver_addclause(pSat, f_prime, f_prime+1);
        for (int l = 0; l < pCnf -> nClauses; ++l)
        {
            sat_solver_addclause(pSat, pCnf -> pClauses[l], pCnf -> pClauses[l+1]);
        }
        
        // second negation
        Cnf_DataLift(pCnf, shift);
        int f_prime2_lit = Abc_Var2Lit(f_p + 2*shift, 1);
        int *f_prime2 = &f_prime2_lit;
        sat_solver_addclause(pSat, f_prime2, f_prime2+1);
        for (int m = 0; m < pCnf -> nClauses; ++m)
        {
            sat_solver_addclause(pSat, pCnf -> pClauses[m], pCnf -> pClauses[m+1]);
        }
        
        //printf("%d \n", IPsize);
        
        // sat size update
        
        for (int n = 0; n < 2*IPsize; ++n)
        {
            sat_solver_addvar(pSat);
        }
        
        // test
        //for (int i = 0 ; i < IPsize ; ++i) { sat_solver_addvar(pSat); }
        //for (int i = 0 ; i < IPsize ; ++i) { sat_solver_addvar(pSat); }
        
        // controller
        vector<int> a, b;
        int a_start, a_end;
        int b_start, b_end;
        a_start = 3*shift + 1;
        a_end = a_start + IPsize - 1;
        b_start = a_end + 1;
        b_end = b_start + IPsize - 1;
        
        // test
        //for (int i = a_start ; i < a_end + 1 ; ++i) { a.push_back(i); }
        //for (int i = b_start ; i < b_end + 1 ; ++i) { b.push_back(i); }
        
        for (int p = a_start; p < a_end + 1; ++p)
        {
            a.push_back(p);
            b.push_back(p+IPsize);
        }
        
        
        // control clauses
        for (int q = 0; q < IPsize ; ++q)
        {
            int control_a1[3] = {Abc_Var2Lit(IP_list[q], 0), Abc_Var2Lit(IP_prime_list[q], 1), Abc_Var2Lit(a[q], 0)};
            sat_solver_addclause(pSat, control_a1, control_a1+3);
            
            int control_a2[3] = {Abc_Var2Lit(IP_list[q], 1), Abc_Var2Lit(IP_prime_list[q], 0), Abc_Var2Lit(a[q], 0)};
            sat_solver_addclause(pSat, control_a2, control_a2+3);
            
            int control_b1[3] = {Abc_Var2Lit(IP_list[q], 0), Abc_Var2Lit(IP_prime2_list[q], 1), Abc_Var2Lit(b[q], 0)};
            sat_solver_addclause(pSat, control_b1, control_b1+3);
            
            int control_b2[3] = {Abc_Var2Lit(IP_list[q], 1), Abc_Var2Lit(IP_prime2_list[q], 0), Abc_Var2Lit(b[q], 0)};
            sat_solver_addclause(pSat, control_b2, control_b2+3);
            
        }
        
        // sat solving
        int partition;
        partition = 0;
        if (IPsize > 1)
        {
            for (int r = 0; r < IPsize - 1; ++r)
            {
                for (int s = r+1; s < IPsize; ++s)
                {
                    vector<int> assumption;
                    for (int t = 0; t < IPsize; ++t)
                    {
                        if (t == r)
                        {
                            assumption.push_back(2*a[t]+1);
                            assumption.push_back(2*b[t]);
                        }
                        
                        else if (t == s)
                        {
                            assumption.push_back(2*a[t]);
                            assumption.push_back(2*b[t]+1);
                        }
                        
                        else
                        {
                            assumption.push_back(2*a[t]+1);
                            assumption.push_back(2*b[t]+1);
                        }
                        
                    }
                    
                    // solving
                    int result;
                    result = sat_solver_solve(pSat, &assumption[0], &assumption[assumption.size()], 0, 0, 0, 0);
                    
                    int nCoreLits, * pCoreLits;
                    vector<int> ans_candidate, ans;
                    if (result == l_False)
                    {
                        partition = 1;
                        nCoreLits = sat_solver_final(pSat, &pCoreLits);
                        
                        // finding relevant variables
                        for (int k = 0 ; k < nCoreLits ; ++k)
                        {
                            if ((std::find(a.begin(), a.end(), int(pCoreLits[k]/2)) != a.end()) || (std::find(b.begin(), b.end(), int(pCoreLits[k]/2)) != b.end()))
                            {
                                ans_candidate.push_back(int(pCoreLits[k]/2));
                            }
                        }
                        
                        // output classification
                        //printf("%d %d \n", r, s);
                        for (int k = 0 ; k < IPsize ; ++k)
                        {
                            if (k == r)
                            {
                                ans.push_back(1);
                                continue;
                            }
                            if (k == s)
                            {
                                ans.push_back(2);
                                continue;
                            }
                            if ((std::find(ans_candidate.begin(), ans_candidate.end(), a[k]) != ans_candidate.end()) && (std::find(ans_candidate.begin(), ans_candidate.end(), b[k]) != ans_candidate.end()))
                            {
                                ans.push_back(0);
                                continue;
                            }
                            else if ((std::find(ans_candidate.begin(), ans_candidate.end(), a[k]) != ans_candidate.end()) && (std::find(ans_candidate.begin(), ans_candidate.end(), b[k]) == ans_candidate.end()))
                            {
                                ans.push_back(1);
                                continue;
                            }
                            else if ((std::find(ans_candidate.begin(), ans_candidate.end(), a[k]) == ans_candidate.end()) && (std::find(ans_candidate.begin(), ans_candidate.end(), b[k]) != ans_candidate.end()))
                            {
                                ans.push_back(2);
                                continue;
                            }
                            else if ((std::find(ans_candidate.begin(), ans_candidate.end(), a[k]) == ans_candidate.end()) && (std::find(ans_candidate.begin(), ans_candidate.end(), b[k]) == ans_candidate.end()))
                            {
                                ans.push_back(1);
                                continue;
                            }
                        }
                    }
                    if (partition == 1)
                    {
                        printf("PO %s support partition: 1 \n", Abc_ObjName(Po));
                        for (int k = 0 ; k < ans.size() ; ++k)
                        {
                            printf("%d", ans[k]);
                        }
                        printf("\n");
                        break;
                    }
                }
                if (partition == 1)
                {
                    break;
                }
            }
            if (partition == 0)
            {
                printf("PO %s support partition: 0 \n", Abc_ObjName(Po));
            }
        }
        else
        {
            printf("PO %s support partition: 0 \n", Abc_ObjName(Po));
        }
    }
}



int LSV_CommandBIDEC(Abc_Frame_t* pAbc, int argc, char** argv)
{
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF)
  {
    switch (c)
    {
      case 'h':
        goto usage;
      default:
        goto usage;
    }
  }
  if (!pNtk)
  {
    Abc_Print(-1, "Empty network.\n");
    return -1;
  }
  // main function
  BIDEC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        prints the BIDEC in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;

}

