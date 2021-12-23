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

void Lsv_CnfComplementAVariable(Cnf_Dat_t* pCnf, int theVar) {
    for (int* pLit = pCnf->pClauses[0], *pStop = pCnf->pClauses[pCnf->nClauses]; pLit < pStop; ++pLit) {
            if (Abc_Lit2Var(*pLit) == theVar) {
                *pLit = Abc_LitNot(*pLit);
            }
    }
}

bool Lsv_fXsToSat(sat_solver * pSat, Cnf_Dat_t * pCnf) {
    for (int i = 0; i < pCnf->nClauses; i++ ){
        if ( !sat_solver_addclause( pSat, pCnf->pClauses[i], pCnf->pClauses[i+1] ) ){
            return false;
        }
    }
    return true;
}
void Lsv_BiDecFreeData(Cnf_Dat_t * pfX, Cnf_Dat_t * pfX_p, Cnf_Dat_t * pfX_pp, sat_solver * pSat) {
    Cnf_DataFree(pfX);
        Cnf_DataFree(pfX_p);
        Cnf_DataFree(pfX_pp);
        sat_solver_delete(pSat);
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
    Aig_Obj_t* pAigObj;
    int i;

    Abc_Ntk_t* pCone;
    Aig_Man_t* pAig;

    Cnf_Dat_t* pfX, *pfX_p, *pfX_pp;
    sat_solver * pSat;

    Abc_NtkForEachPo(pNtk, pObj, i){
        printf("PO %s support partition: ", Abc_ObjName(pObj));
        // Get the cone of this output
        pCone = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0 );
        if ( Abc_ObjFaninC0(pObj) )
            Abc_ObjXorFaninC( Abc_NtkPo(pCone, 0), 0 );

        pAig = Abc_NtkToDar(pCone, 0, 0);
        pfX = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
        int conePO = Aig_ObjId(Aig_ManCo(pAig, 0)) / 2;
        int varNumShift = pfX->nVars;
        //Create f(X') and f(X'') and complement them
        pfX_p = Cnf_DataDup(pfX);
        Cnf_DataLift(pfX_p, varNumShift);

        pfX_pp = Cnf_DataDup(pfX);
        Cnf_DataLift(pfX_pp, 2 * varNumShift);
        // Cnf_DataPrint(pfX, 1);
        // printf("%d %d\n", conePO, varNumShift);
        // Cnf_DataPrint(pfX_p , 1);
        // Cnf_DataPrint(pfX_pp, 1);
        // for (int j = 0; j < varNumShift; ++j) {
        //     printf("%d, ", pfX->pVarNums[j]);
        // }
        // printf("\n");
        // for (int j = 0; j < varNumShift; ++j) {
        //     printf("%d, ", pfX_p->pVarNums[j]);
        // }
        // printf("\n");
        // for (int j = 0; j < varNumShift; ++j) {
        //     printf("%d, ", pfX_pp->pVarNums[j]);
        // }
        // printf("\n");
        pSat = sat_solver_new();
        sat_solver_setnvars(pSat, 5 * varNumShift); // for X, X', X'', alpha_i and beta_i
        if (!Lsv_fXsToSat(pSat, pfX)   ||
            !Lsv_fXsToSat(pSat, pfX_p) ||
            !Lsv_fXsToSat(pSat, pfX_pp)){
            printf("Instantiating sat_solver failed: in writing clauses from f(X)'s\n");
            Lsv_BiDecFreeData(pfX, pfX_p, pfX_pp, pSat);
            return;
        }

        if (!sat_solver_add_const(pSat, conePO, 0) || 
            !sat_solver_add_const(pSat, conePO + varNumShift, 1) ||
            !sat_solver_add_const(pSat, conePO + 2*varNumShift, 1)) {
            printf("Instantiating sat_solver failed: in asserting output phase f(X)'s\n");  
            Lsv_BiDecFreeData(pfX, pfX_p, pfX_pp, pSat);
            return;
        }

        
        for (int j = 0; j < varNumShift; ++j) {
            int xi = pfX->pVarNums[j];
            if (xi < 0) continue;
            int xi_p = xi + varNumShift, xi_pp = xi + 2 * varNumShift;
            int alpha_i = xi + 3 * varNumShift, beta_i = xi + 4 * varNumShift;
            if (!sat_solver_add_buffer_disable(pSat, xi, xi_p, alpha_i, 0)) {
                printf("Instantiating sat_solver failed: on alpha_%d clause\n", j); 
                Lsv_BiDecFreeData(pfX, pfX_p, pfX_pp, pSat);
                return; 
            }
            if (!sat_solver_add_buffer_disable(pSat, xi, xi_pp, beta_i, 0)) {
                printf("Instantiating sat_solver failed: on beta_%d clause\n", j); 
                Lsv_BiDecFreeData(pfX, pfX_p, pfX_pp, pSat);
                return; 
            }
        }
        // printf("%d\n", sat_solver_nclauses(pSat));
        int status = 1;
        lit assumption[4];
        for (int seed1 = 0; seed1 < varNumShift - 1; ++seed1) {
            if (pfX->pVarNums[seed1] < 0) continue;
            assumption[0] = toLitCond(pfX->pVarNums[seed1] + 3 * varNumShift, 0);
            assumption[1] = toLitCond(pfX->pVarNums[seed1] + 4 * varNumShift, 1);
            for (int seed2 = seed1 + 1; seed2 < varNumShift; ++seed2) {
                if (pfX->pVarNums[seed2] < 0) continue;
                printf("%d %d\n", pfX->pVarNums[seed1], pfX->pVarNums[seed2]);
                assumption[2] = toLitCond(pfX->pVarNums[seed2] + 3 * varNumShift, 1);
                assumption[3] = toLitCond(pfX->pVarNums[seed2] + 4 * varNumShift, 0);
                // TODO: maintain assumpList based on seed1 and seed2
                status = sat_solver_solve(pSat, assumption, assumption+4, 0, 0, 0, 0);
                if (!status) break; // UNSAT corresponds to a viable partition

            }
            if (!status) break;
        }
        
        printf("%d\n", 1 - status);
        if (!status) {

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