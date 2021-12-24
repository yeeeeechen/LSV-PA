bool DEBUG_MODE = false;

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include "sat/cnf/cnf.h"        // new for PA2

#include <iostream>

extern "C"{
    Aig_Man_t *Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

// -----------------------------  include related files ----^

int Lsv_Or_Bidec_Command(Abc_Frame_t* pAbc, int argc, char** argv);
void Lsv_Or_Bidec(Abc_Ntk_t* pNtk);

// -----------------------------  self-declared function declaration ----^

void init(Abc_Frame_t* pAbc){
    Cmd_CommandAdd( pAbc, "LSV", "lsv_or_bidec", Lsv_Or_Bidec_Command, 0);             // change here
}

void destroy(Abc_Frame_t* pAbc){}

Abc_FrameInitializer_t frame_initializer = { init, destroy };

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

int Lsv_Or_Bidec_Command(Abc_Frame_t* pAbc, int argc, char** argv) {
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
        if (!pNtk) {
            Abc_Print(-1, "Empty network.\n");
            return 1;
        }
    }
    Lsv_Or_Bidec(pNtk);
    return 0;


    usage:                                                                      // change here and below
      Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
      Abc_Print(-2, "\t        Decides whether each circuit PO f(X) is OR bi-decomposable and print the partition if there exists a valid non-trivial one.\n");
      Abc_Print(-2, "\t-h    : print the command usage\n");
      return 1;
}

// -----------------------------  routines for self-declared functions ----^

int solve_for_singlePo(Aig_Man_t* pAig){                // return -1 if F is contradiction, any assumption can cause unsatisfiable
    if(DEBUG_MODE)
        printf("flag000\n");
    sat_solver* pMainSat = sat_solver_new();
    int POid = Aig_ManCo(pAig, 0)->Id;
    int finalLit, success;                              // used later for adding the satisfiablily condiction that F = 1
    int nVariable;                                      // the number of variables in the CNF

    // ====================================================================================================================
    // ==========================     write F(X), NOT F(X'), NOT F(X'')     ===============================================
    // ====================================================================================================================

    Cnf_Dat_t* pCnf = Cnf_Derive(pAig, Aig_ManCoNum(pAig));                             // derive the CNF from the "structure" of F(X)
    nVariable = pCnf->nVars;
    pMainSat = (sat_solver*)Cnf_DataWriteIntoSolverInt(pMainSat, pCnf, 1, 0);           // push the clauses representing the "structure" of F(X)
    if(pMainSat==NULL){ Cnf_DataFree( pCnf ); return -1; }
    finalLit = toLitCond( pCnf->pVarNums[POid], 0 );                                    // note that 0 means positive                               // sat/bsat/satVec.h:static inline lit  toLitCond(int v, int c) { return v + v + (c != 0); }
    success = sat_solver_addclause(pMainSat, &finalLit, &finalLit+1);                   // add the satisfiablily condiction that F(X)=1
    if(!success){ Cnf_DataFree( pCnf ); return -1; }
    // finished f(X)

    Cnf_DataLift(pCnf, nVariable);                                                      // derive the CNF from the "structure" of F(X')
    pMainSat = (sat_solver*)Cnf_DataWriteIntoSolverInt(pMainSat, pCnf, 1, 0);           // push the clauses representing the "structure" of F(X')
    if(pMainSat==NULL){Cnf_DataFree( pCnf ); return -1; }
    //POid = Aig_ManCo(pAig, 0)->Id;
    finalLit = toLitCond( pCnf->pVarNums[POid], 1 );                                    // note that 1 means negative
    success = sat_solver_addclause(pMainSat, &finalLit, &finalLit+1);                   // add the satisfiablily condiction that F(X')=0 (i.e. NOT F(X')=1)
    if(!success){ Cnf_DataFree( pCnf ); return -1; }
    // finished NOT f(X')

    Cnf_DataLift(pCnf, nVariable);                                                      // derive the CNF from the "structure" of F(X'')
    pMainSat = (sat_solver*)Cnf_DataWriteIntoSolverInt(pMainSat, pCnf, 1, 0);           // push the clauses representing the "structure" of F(X')
    if(pMainSat==NULL){ Cnf_DataFree( pCnf );  return -1; }
    //POid = Aig_ManCo(pAig, 0)->Id;
    finalLit = toLitCond( pCnf->pVarNums[POid], 1 );                                    // note that 1 means negative
    success = sat_solver_addclause(pMainSat, &finalLit, &finalLit+1);                   // add the satisfiablily condiction that F(X'')=0 (i.e. NOT F(X'')=1)
    if(!success){ Cnf_DataFree( pCnf ); return -1; };
    // finished NOT f(X'')

    if(DEBUG_MODE)
        printf("flag001\n");

    // ====================================================================================================================
    // ==========================     write equivalence and controlling formula     =======================================
    // ====================================================================================================================

    Cnf_DataLift(pCnf, -2*nVariable);                       // return CNF to the initial status for convenience

    int nCi = Aig_ManCiNum(pAig);
    //  X    :     1  ~  n
    //  X'   :    n+1 ~ 2n
    //  X''  :   2n+1 ~ 3n
    // alpha :   3n+1 ~ 3n+m
    // beta  : 3n+m+1 ~ 3n+2m

    Aig_Obj_t* pEachCi;
    int index_Ci;
    Aig_ManForEachCi(pAig, pEachCi, index_Ci){
        int inputVar = pCnf->pVarNums[pEachCi->Id];
        if(DEBUG_MODE)
            printf("%d: Var %d -> %d\n", index_Ci, pEachCi->Id, inputVar);

        int clause[3];
        clause[0] = toLitCond( inputVar, 1 );                       // NOT x_i
        clause[1] = toLitCond( inputVar+nVariable, 0 );             // x_i'
        clause[2] = toLitCond( 3*nVariable+index_Ci+1, 0 );         // alpha_i
        success = sat_solver_addclause(pMainSat, clause, clause+3);
        if(!success){ Cnf_DataFree( pCnf ); return -1; }

        clause[0] = toLitCond( inputVar, 0 );                       // x_i
        clause[1] = toLitCond( inputVar+nVariable, 1 );             // NOT x_i'
        clause[2] = toLitCond( 3*nVariable+index_Ci+1, 0 );         // alpha_i
        success = sat_solver_addclause(pMainSat, clause, clause+3);
        if(!success){ Cnf_DataFree( pCnf ); return -1; }

        clause[0] = toLitCond( inputVar, 1 );                       // NOT x_i
        clause[1] = toLitCond( inputVar+2*nVariable, 0 );           // x_i''
        clause[2] = toLitCond( 3*nVariable+nCi+index_Ci+1, 0 );     // beta_i
        success = sat_solver_addclause(pMainSat, clause, clause+3);
        if(!success){ Cnf_DataFree( pCnf ); return -1; }

        clause[0] = toLitCond( inputVar, 0 );                       // x_i
        clause[1] = toLitCond( inputVar+2*nVariable, 1 );           // NOT x_i''
        clause[2] = toLitCond( 3*nVariable+nCi+index_Ci+1, 0 );     // beta_i
        success = sat_solver_addclause(pMainSat, clause, clause+3);
        if(!success){ Cnf_DataFree( pCnf ); return -1; }
    }

    if(DEBUG_MODE)
        printf("flag002\n");

    // ====================================================================================================================
    // =======================              solve under seed variable partitions              =============================
    // ====================================================================================================================

    if(DEBUG_MODE)
        printf("n = %d, m = %d\n", nVariable ,nCi);
    for(int i=1; i<=nCi; i++){for(int j=1; j<i; j++){           // for all seed variable partitions
        int* assumption = new int[2*nCi];
        if(DEBUG_MODE)
            printf("assumptions: ");
        for(int k=1; k<=nCi; k++){
            int alpha_i = 3*nVariable + k;
            int beta_i = 3*nVariable + nCi + k;
            assumption[2*k-2] = toLitCond( alpha_i, 1 );        // alpha_i = 0
            assumption[2*k-1] = toLitCond( beta_i, 1 );         // beta_i = 0
            if(k==i)
                assumption[2*k-2] = toLitCond( alpha_i, 0 );    // alpha_a = 1
            else if(k==j)
                assumption[2*k-1] = toLitCond( beta_i, 0 );     // beta_b = 1

            if(DEBUG_MODE)
                printf("%d %d ", assumption[2*k-2], assumption[2*k-1]);
        }

        int result = sat_solver_solve( pMainSat, assumption, assumption + 2*nCi, 0, 0, 0, 0);
        if(result==1){      // satisfiable
            if(DEBUG_MODE)
                printf("... satisfiable under this assumption.\n");
            continue;
        }
        else{               // unsatisfiable (if not time out)
            int *conflict_clause;
            int nFinal = sat_solver_final(pMainSat, &conflict_clause);          // get the conflict clause

            int* conflict_core = new int[3*nVariable + 2*nCi + 1];
            for(int k=0; k<3*nVariable + 2*nCi + 1; k++){
                conflict_core[k] = 1;
            }

            if(DEBUG_MODE)
                printf("\nconflict clause: ");
            for(int k=0; k<nFinal; k++){
                if(DEBUG_MODE)
                    printf("%d ", conflict_clause[k]);
                conflict_core[ conflict_clause[k]/2 ] = 0;                          // these variables being in X_c ensures unsatisfiablilty
            }
            if(DEBUG_MODE)
                printf("\n");

            printf("1\n");
            for(int k=1; k<=nCi; k++){
                int alpha_i = 3*nVariable + k;
                int beta_i = 3*nVariable + nCi + k;

                if(k==i)
                    printf("2");                                                    // assume to be in X_a
                else if(k==j)
                    printf("1");                                                    // assume to be in X_b
                else if(conflict_core[alpha_i]==0 || conflict_core[beta_i]==0)
                    printf("0");                                                    // these variables being in X_c ensures unsatisfiablilty
                else
                    printf("1");                                                    // can be in either X_a or X_b
            }
            printf("\n");
            Cnf_DataFree( pCnf );
            sat_solver_delete( pMainSat );
            return 0;
        }
    }}

    printf("0\n");
    Cnf_DataFree( pCnf );
    sat_solver_delete( pMainSat );
    return 0;
}

void Lsv_Or_Bidec(Abc_Ntk_t* pNtk) {
    int indexForEachPo;
    Abc_Obj_t* pPo;
    Abc_NtkForEachPo(pNtk, pPo, indexForEachPo){
        Abc_Ntk_t* cone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 0);
        if( Abc_ObjFaninC0(pPo) ){
            Abc_ObjSetFaninC( Abc_NtkPo(cone, 0), 0);   // PO might not be negated correctly; fixed here
        }

        Aig_Man_t* pAig = Abc_NtkToDar(cone, 0, 0);
        if(DEBUG_MODE)
            Aig_ManShow(pAig, 0, NULL);

        printf("PO %s support partition: ", Abc_ObjName(pPo));
        int result = solve_for_singlePo(pAig);
        if(result==-1){                                 // if the parition can be any
            if(Aig_ManCiNum(pAig)<2){                   // if there are less than two Pi
                printf("0\n1");
            }
            else{
                printf("1\n1");
                for(int i=1; i<Aig_ManCiNum(pAig); i++){
                    printf("2");
                }
                printf("\n");
            }
        }
    }
    return;

}

