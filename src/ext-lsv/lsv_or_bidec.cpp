#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

namespace {

void Lsv_OrBidec(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    Abc_Ntk_t* pNtkNew;
    Aig_Man_t* pAig;
    Cnf_Dat_t* pCnf;
    sat_solver* pSat;
    // int *pBeg, *pEnd;
    int ii, i, j, k;
    lit Lit, Lits[3];
    // get each PO
    Abc_NtkForEachPo(pNtk, pObj, ii) {
        // get support set
        pNtkNew = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0);   //TODO: 0 or 1?
        if (Abc_ObjFaninC0(pObj))
            Abc_ObjXorFaninC(Abc_NtkPo(pNtkNew, 0), 0);
        // ntk to aig
        pAig = Abc_NtkToDar(pNtkNew, 0, 1);     //TODO (0, 1)?
        // aig to cnf
        pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));
        int varshift = pCnf->nVars;
        // PI & PO info
        int nPi = Aig_ManCiNum(pAig);   // int nPi = Abc_NtkPiNum(pNtkNew);
        int* PiIDs = new int[nPi];
        Aig_Obj_t* pCi;
        Aig_ManForEachCi(pCnf->pMan, pCi, i) {
            PiIDs[i] = pCnf->pVarNums[Aig_ObjId(pCi)];
        }
        int PoID = pCnf->pVarNums[Aig_ObjId(Aig_ManCo(pCnf->pMan, 0))]; // int PoID = 1;        
        /*** construct CNF ***/
        // start the solver
        pSat = sat_solver_new();
        sat_solver_setnvars(pSat, varshift * 3 + nPi * 2); // {f(X), f(X'), f(X'')}, {alpha, beta}
        // f(X)
        for (i = 0; i < pCnf->nClauses; ++i) {
            sat_solver_addclause(pSat, pCnf->pClauses[i], pCnf->pClauses[i + 1]);
        }
        Lit = toLitCond(PoID, 0);
        sat_solver_addclause(pSat, &Lit, &Lit + 1);
        // F(X')
        Cnf_DataLift(pCnf, varshift);
        for (i = 0; i < pCnf->nClauses; ++i) {
            sat_solver_addclause(pSat, pCnf->pClauses[i], pCnf->pClauses[i + 1]);
        }
        Lit = toLitCond(PoID + varshift * 1, 1);
        sat_solver_addclause(pSat, &Lit, &Lit + 1);
        // F(X'')
        Cnf_DataLift(pCnf, varshift);
        for (i = 0; i < pCnf->nClauses; ++i) {
            sat_solver_addclause(pSat, pCnf->pClauses[i], pCnf->pClauses[i + 1]);
        }
        Lit = toLitCond(PoID + varshift * 2, 1);
        sat_solver_addclause(pSat, &Lit, &Lit + 1);
        // alpha & beta
        // Aig_Obj_t* pPi;     // Abc_Obj_t* pPi;
        int PiID, Cid;
        int alpha_base = varshift * 3, beta_base = varshift * 3 + nPi;
        // Aig_ManForEachCi(pCnf->pMan, pPi, i) {  // Abc_NtkForEachPi(pNtkNew, pPi, i) {
        for (i = 0; i < nPi; ++i) {    
            // PiID = pCnf->pVarNums[Aig_ObjId(pPi)];  // PiID = pCnf->pVarNums[Abc_ObjId(pPi)];            
            PiID = PiIDs[i];            
            // (_Xi or X'i or alpha_i)(Xi or _X'i or alpha_i)
            Lits[0] = toLitCond(PiID, 1);                   // _Xi
            Lits[1] = toLitCond(PiID + varshift * 1, 0);    // X'i
            Lits[2] = toLitCond(alpha_base + i, 0);         // alpha_i
            Cid = sat_solver_addclause(pSat, Lits, Lits + 3 );
            assert( Cid );
            Lits[0] = toLitCond(PiID, 0);                   // Xi
            Lits[1] = toLitCond(PiID + varshift * 1, 1);    // _X'i
            Lits[2] = toLitCond(alpha_base + i, 0);         // alpha_i
            Cid = sat_solver_addclause(pSat, Lits, Lits + 3 );
            assert( Cid );
            // (_Xi or X''i or beta_i)(Xi or _X''i or beta_i)
            Lits[0] = toLitCond(PiID, 1);                   // _Xi
            Lits[1] = toLitCond(PiID + varshift * 2, 0);    // X''i
            Lits[2] = toLitCond(beta_base + i, 0);          // beta_i
            Cid = sat_solver_addclause(pSat, Lits, Lits + 3 );
            assert( Cid );
            Lits[0] = toLitCond(PiID, 0);                   // Xi
            Lits[1] = toLitCond(PiID + varshift * 2, 1);    // _X''i
            Lits[2] = toLitCond(beta_base + i, 0);          // beta_i
            Cid = sat_solver_addclause(pSat, Lits, Lits + 3 );
            assert( Cid );
        }
        // iterate all possible seed variable partitions
        bool is_unsatisfiable = false;
        int status; // (l_Undef, l_True, l_False) = (UNDECIDED, SATISFIABLE, UNSATISFIABLE)
        int *pFinal, nFinal;
        lit assumptions[nPi * 2];
        for (i = 0; i < nPi && !is_unsatisfiable; ++i) {
            for (j = i + 1; j < nPi && !is_unsatisfiable; ++j) {
                // alpha
                for (k = 0 ; k < nPi; ++k) {
                    if (k == i)
                        assumptions[k] = toLitCond(alpha_base + k, 0);
                    else
                        assumptions[k] = toLitCond(alpha_base + k, 1);
                }
                // beta
                for (k = nPi ; k < nPi * 2; ++k) {
                    if (k == (j + nPi))
                        assumptions[k] = toLitCond(alpha_base + k, 0);
                    else
                        assumptions[k] = toLitCond(alpha_base + k, 1);
                }
                // sat solve
                status = sat_solver_solve(pSat, assumptions, assumptions + nPi * 2, 0, 0, 0, 0);
                if (status == l_False) {
                    is_unsatisfiable = true;
                    nFinal = sat_solver_final(pSat, &pFinal);
                    break;
                }
            }
        }
        // output
        if (is_unsatisfiable) {
            printf("PO %s suppport partition: 1\n", Abc_ObjName(pObj));
            bool is_hit[nPi * 2];
            for (i = 0; i < nPi * 2; ++i)
                is_hit[i] = false;
            for (i = 0; i < nFinal; ++i) 
                is_hit[Abc_Lit2Var(pFinal[i]) - varshift * 3] = true;
            // **Every literal in the conflict clause is of positive phase**
            for (i = 0; i < nPi; ++i) {
                if (is_hit[i] && is_hit[i + nPi])    // (alpha_i, beta_i) = (0, 0) --> C --> 0
                    printf("0");
                else if (is_hit[i])     // (alpha_i, beta_i) = (0, 1) --> B --> 1
                    printf("1");
                else if (is_hit[i + nPi])    // (alpha_i, beta_i) = (1, 0) --> A --> 2
                    printf("2");
                else    // (alpha_i, beta_i) = (1, 1) --> A or B --> 1 or 2
                    printf("2");
            }
            printf("\n");
        } else {
            printf("PO %s suppport partition: 0\n", Abc_ObjName(pObj));
        }
    }
}



int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc); 
    int c;
    Extra_UtilGetoptReset();
    while ((c = Extra_UtilGetopt(argc, argv, "fh")) != EOF) {
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
    Lsv_OrBidec(pNtk);
    // Lsv_NtkOrBiDecom(pNtk);
    return 0;

    usage:
        Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
        Abc_Print(-2, "\t         decide whether each cirecuit PO f(X) is OR bi-decomposable\n");
        Abc_Print(-2, "\t         and output '0' and the support partition if yes or output '1' if no\n");
        Abc_Print(-2, "\t-h     : print the command usage\n");
        return 1;
}

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct registrar {
    registrar() {
        Abc_FrameAddInitializer(&frame_initializer);
    }    
} lsv_or_bidec_registrar;

}