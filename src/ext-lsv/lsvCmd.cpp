#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBiDec(Abc_Frame_t* pAbc, int argc, char** argv);

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int  fRegisters );
extern "C" Cnf_Dat_t * Cnf_Derive( Aig_Man_t * pAig, int nOutputs );
extern "C" void        Cnf_DataPrint( Cnf_Dat_t * p, int fReadable );

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
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

// sort method used by Vec_PtrSort
static int Vec_PtrSortCompareObjIds( void ** pp1, void ** pp2 ) {   
    Abc_Obj_t* pObj1 = (Abc_Obj_t*) *pp1;
    Abc_Obj_t* pObj2 = (Abc_Obj_t*) *pp2;
    
    if ( Abc_ObjId(pObj1) < Abc_ObjId(pObj2) )
        return -1;
    if ( Abc_ObjId(pObj1) > Abc_ObjId(pObj2) ) 
        return 1;
    return 0; 
}

static int Vec_PtrSortCompareFirstObjIdInCone( void ** pp1, void ** pp2 ) {   
    Abc_Obj_t* pObj1 = (Abc_Obj_t*) Vec_PtrEntry((Vec_Ptr_t*) *pp1, 0);
    Abc_Obj_t* pObj2 = (Abc_Obj_t*) Vec_PtrEntry((Vec_Ptr_t*) *pp2, 0);
    if ( Abc_ObjId(pObj1) < Abc_ObjId(pObj2) )
        return -1;
    if ( Abc_ObjId(pObj1) > Abc_ObjId(pObj2) ) 
        return 1;
    return 0; 
}

void Lsv_NodeMfsc_rec(Abc_Obj_t* pNode, Vec_Ptr_t* vIterateList, Vec_Ptr_t* vCone, int fTopmost) {
    Abc_Obj_t* pFanin;
    int i;

    if (pNode->fMarkA == 1) {
        return;
    }
    // Abc_NodeSetTravIdCurrent(pNode);

    if (Abc_ObjIsCi(pNode)) {
        return;
    }
    if (!fTopmost && Abc_ObjFanoutNum(pNode) > 1) {
        Vec_PtrPush(vIterateList, pNode);
        return;
    }

    Abc_ObjForEachFanin(pNode, pFanin, i) {
        Lsv_NodeMfsc_rec(pFanin, vIterateList, vCone, 0);
    }
    //
    if (vCone && !Abc_ObjIsCo(pNode)) Vec_PtrPush(vCone, pNode);
    pNode->fMarkA = 1; 
}

void Lsv_NodeMsfc(Abc_Obj_t* pNode, Vec_Ptr_t* vIterateList, Vec_Ptr_t* vCone) {
    // compute MSFC and store them in vCone
    assert(!Abc_ObjIsComplement(pNode));
    if (vCone) Vec_PtrClear(vCone);
    // Abc_NtkIncrementTravId(pNode->pNtk);
    Lsv_NodeMfsc_rec(pNode, vIterateList, vCone, 1);
}

void Lsv_NtkMsfc(Abc_Ntk_t* pNtk, Vec_Ptr_t* vMsfc) {
    Abc_Obj_t* pObj;
    Vec_Ptr_t* vObj = Vec_PtrAlloc(50);
    int i;
    Abc_NtkForEachCo(pNtk, pObj, i) {
        Vec_PtrPush(vObj, pObj);
    }
    Vec_PtrForEachEntry(Abc_Obj_t*, vObj, pObj, i) {
        Vec_Ptr_t* vCone = Vec_PtrAlloc(100);
        Lsv_NodeMsfc(pObj, vObj, vCone);
        Vec_PtrSort(vCone, (int (*)(const void *, const void *)) Vec_PtrSortCompareObjIds);
        if (Vec_PtrSize(vCone) > 0) {
            Vec_PtrPush(vMsfc, (void*)vCone);
        }
    }

    Vec_PtrSort(vMsfc, (int (*)(const void *, const void *)) Vec_PtrSortCompareFirstObjIdInCone);
}

void Lsv_NtkPrintMsfc(Abc_Ntk_t* pNtk) {
    Vec_Ptr_t* vMsfc = Vec_PtrAlloc(100), *pOneMsfc;

    Abc_Obj_t* pObj;
    int i, j;
    Lsv_NtkMsfc(pNtk, vMsfc);
    Vec_PtrForEachEntry(Vec_Ptr_t*, vMsfc, pOneMsfc, i) {
        printf("MSFC %d: ", i);
        Vec_PtrForEachEntry(Abc_Obj_t*, pOneMsfc, pObj, j) {
            printf("%s", Abc_ObjName(pObj));
            if (j != pOneMsfc->nSize - 1) {
                printf(",");
            }
        }
        printf("\n");
    }
    Abc_NtkCleanMarkA(pNtk);
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
    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkPrintMsfc(pNtk);
    return 0;
usage:
    Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
    Abc_Print(-2, "\t        prints the maximum single-fanout cone (MSFC) decomposition of the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

void Lsv_BiDecFreeData(Cnf_Dat_t * pfX, Cnf_Dat_t * pfX_p, Cnf_Dat_t * pfX_pp, sat_solver * pSat) {
    Cnf_DataFree(pfX);
        Cnf_DataFree(pfX_p);
        Cnf_DataFree(pfX_pp);
        sat_solver_delete(pSat);
}

bool Lsv_fXtoSat(sat_solver* pSat, Cnf_Dat_t* pfX) {
    for (int i = 0; i < pfX->nClauses; i++){
        if (!sat_solver_addclause(pSat, pfX->pClauses[i], pfX->pClauses[i+1])){
            return false;
        }
    }
    return true;
}

static inline int sat_solver_add_buffer_disable( sat_solver * pSat, int iVarA, int iVarB, int iVarEn, int fCompl )
{
    lit Lits[3];
    int Cid;
    assert( iVarA >= 0 && iVarB >= 0 && iVarEn >= 0 );

    Lits[0] = toLitCond( iVarA, 0 );
    Lits[1] = toLitCond( iVarB, !fCompl );
    Lits[2] = toLitCond( iVarEn, 0 ); // If En = 1, free variables
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );

    Lits[0] = toLitCond( iVarA, 1 );
    Lits[1] = toLitCond( iVarB, fCompl );
    Lits[2] = toLitCond( iVarEn, 0 ); // If En = 1, free variables
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );
    return 2;
}

void Lsv_NtkOrBiDec(Abc_Ntk_t* pNtk){
    // extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
    Abc_Obj_t* pObj;
    int i, j;
    int xi;

    Abc_Ntk_t* pCone;
    Aig_Man_t* pAig;
    Aig_Obj_t* pAigObj;

    Cnf_Dat_t* pfX, *pfX_p, *pfX_pp;
    sat_solver * pSat;

    Abc_NtkForEachPo(pNtk, pObj, i){
        printf("PO %s support partition: ", Abc_ObjName(pObj));
        // Get the cone of this output
        pCone = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0 );
        if ( Abc_ObjFaninC0(pObj) )
            Abc_ObjXorFaninC( Abc_NtkPo(pCone, 0), 0 );

        pAig = Abc_NtkToDar(pCone, 0, 0);
        assert(Aig_ManCoNum(pAig) == 1);
        pfX = Cnf_Derive(pAig, 0); // Cnf_Derive asserts outputs to be 1 when the second parameter is 0
        int varNumShift = pfX->nVars;
        
        //Create f(X') and f(X'') and complement them
        pfX_p = Cnf_DataDup(pfX);
        Cnf_DataLift(pfX_p, varNumShift); 

        // The last clause asserts the output to be 1
        // This line change it to 0
        pfX_p->pClauses[pfX_p->nClauses-1][0] ^= 1;

        pfX_pp = Cnf_DataDup(pfX_p);
        Cnf_DataLift(pfX_pp, varNumShift);
        
        pSat = sat_solver_new();
        sat_solver_setnvars(pSat, 5 * varNumShift); // for X, X', X'', alpha_i and beta_i
        if (!Lsv_fXtoSat(pSat, pfX   ) ||
            !Lsv_fXtoSat(pSat, pfX_p ) ||
            !Lsv_fXtoSat(pSat, pfX_pp) ){
            printf("0\n");
            // Lsv_BiDecFreeData(pfX, pfX_p, pfX_pp, pSat);
            continue;
        }

        // Collect all PIs to the cone
        Vec_Int_t * conePIs = Vec_IntAlloc(1);
        Aig_ManForEachCi(pAig, pAigObj, j) {
            Vec_IntPush(conePIs, pfX->pVarNums[pAigObj->Id]);
        }

        
        
        Vec_IntForEachEntry(conePIs, xi, j) {
            int xi_p    = xi +     varNumShift; 
            int xi_pp   = xi + 2 * varNumShift;
            int alpha_i = xi + 3 * varNumShift; 
            int beta_i  = xi + 4 * varNumShift;
            if (!sat_solver_add_buffer_disable(pSat, xi, xi_p, alpha_i, 0)) {
                printf("0\n"); 
                // Lsv_BiDecFreeData(pfX, pfX_p, pfX_pp, pSat);
                return; 
            }
            if (!sat_solver_add_buffer_disable(pSat, xi, xi_pp, beta_i, 0)) {
                printf("0\n");
                // Lsv_BiDecFreeData(pfX, pfX_p, pfX_pp, pSat);
                return; 
            }
        }

        int status = l_True;
        Vec_Int_t * vLits;
        
        for (int seed1 = 0; seed1 < Vec_IntSize(conePIs) - 1; ++seed1) {
            for (int seed2 = seed1 + 1; seed2 < Vec_IntSize(conePIs); ++seed2) {
                vLits = Vec_IntAlloc(2 * Vec_IntSize(conePIs));
                Vec_IntForEachEntry(conePIs, xi, j) {
                    Vec_IntPush(vLits, toLitCond(xi + 3 * varNumShift, ((j == seed1) ? 0 : 1)));
                    Vec_IntPush(vLits, toLitCond(xi + 4 * varNumShift, ((j == seed2) ? 0 : 1)));
                }
                // Vec_IntPrint(vLits);
                status = sat_solver_solve(pSat, Vec_IntArray(vLits), Vec_IntLimit(vLits), 0, 0, 0, 0);
                if (status == l_False) {
                    break; // UNSAT corresponds to a viable partition
                }

            }
            if (status == l_False) break;
            Vec_IntErase(vLits);  
        }
        
        printf("%d\n", (status == l_False) ? 1 : 0);
        if (status == l_False) {
            // Check conflict and print bidec
            int nFinal, *pFinal;
            nFinal = sat_solver_final(pSat, &pFinal);
            Vec_Int_t * AlphaConflict = Vec_IntAlloc(Vec_IntSize(conePIs));
            Vec_Int_t * BetaConflict  = Vec_IntAlloc(Vec_IntSize(conePIs));

            Vec_IntForEachEntry(conePIs, xi, j) {
                Vec_IntArray(AlphaConflict)[j] = 0;
                Vec_IntArray(BetaConflict) [j] = 0;
            } 
            // Vec_IntForEachEntry(conePIs, xi, j) {
            //     for (int tmp = 0; tmp < nFinal; ++tmp) {
            //         int literal = Abc_Lit2Var(pFinal[tmp]);
            //         flagA = 0; flagB = 0;
            //         if ((xi + 3 * varNumShift) == literal) {
            //             flagA = 1;
                        
            //         }
            //         else if ((xi + 4 * varNumShift) == literal) {
            //             flagB = 1;
            //         }
            //     }
            //     Vec_IntPush(AlphaConflict, flagA);
            //     Vec_IntPush(BetaConflict , flagB);
            // }
            for (int tmp = 0; tmp < nFinal; ++tmp) {
                int literal = Abc_Lit2Var(pFinal[tmp]);
                Vec_IntForEachEntry(conePIs, xi, j) {
                    if (xi + 3 * varNumShift == literal) {
                        Vec_IntArray(AlphaConflict)[j] = 1;
                        break;
                    }
                    if (xi + 4 * varNumShift == literal) {
                        Vec_IntArray(BetaConflict) [j] = 1;
                        break;
                    }
                } 
            }

            Vec_IntForEachEntry(conePIs, xi, j) {
                if (      Vec_IntArray(AlphaConflict)[j] && 
                          Vec_IntArray(BetaConflict) [j])     printf("0");
                else if (!Vec_IntArray(AlphaConflict)[j])     printf("1");
                else                                          printf("2");
            } 
            printf("\n");
        }
    }
    return;
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
    if (pNtk == NULL) {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    Lsv_NtkOrBiDec(pNtk);
    return 0;
usage:
    Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
    Abc_Print(-2, "\t        print possible OR-bi-decomposition for each PO of the network\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}