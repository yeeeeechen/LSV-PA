#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

static int Lsv_CommandORBiDec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandORBiDec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
    PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager2;

void Lsv_ORBiDec(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pObj;
    Abc_Ntk_t* PONtk;
    Aig_Man_t* pAig;
    Cnf_Dat_t* pCnf;
    sat_solver* pSat;
    int i, j, k;
    int *begin,*end;
    int aigPoId, originCnfVar, originCnfId;
    pNtk = Abc_NtkStrash(pNtk, 1, 1, 0);
    Abc_NtkForEachPo(pNtk, pObj, i) {    
        Aig_Obj_t* supportObj;
        lit Lit[3];
        PONtk = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0);
        if(Abc_ObjFaninC0(pObj)) {
            Abc_ObjXorFaninC(Abc_NtkPo(PONtk, 0), 0);
        }
        PONtk = Abc_NtkStrash(PONtk, 1, 1, 0);
        pAig = Abc_NtkToDar(PONtk, 0, 0);
        pCnf = Cnf_Derive(pAig, 1);
        pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);

        aigPoId = Aig_ManCo(pAig, 0)->Id;
        originCnfId = pCnf->pVarNums[aigPoId];
        originCnfVar = pCnf->nVars;

        Lit[0] = toLitCond(originCnfId, 0);
        sat_solver_addclause(pSat, Lit, Lit+1);

        Cnf_DataLift(pCnf, originCnfVar);
        Cnf_CnfForClause(pCnf, begin, end, j) {
            sat_solver_addclause(pSat, begin, end);
        }
        Lit[0] = toLitCond(originCnfId+originCnfVar, 1);
        sat_solver_addclause(pSat, Lit, Lit+1);
        Cnf_DataLift(pCnf, originCnfVar);
        Cnf_CnfForClause(pCnf, begin, end, j) {
            sat_solver_addclause(pSat, begin, end);
        }
        Lit[0] = toLitCond(originCnfId + originCnfVar*2, 1);
        sat_solver_addclause(pSat, Lit, Lit+1);
        int alphaBegin = sat_solver_nvars(pSat);
        int alphaSize = Aig_ManCiNum(pAig);
        Aig_ManForEachCi(pAig, supportObj, k) {
            int alpha = sat_solver_addvar(pSat);
            int beta = sat_solver_addvar(pSat);
            int x = pCnf->pVarNums[Aig_ObjId(supportObj)] - 2*originCnfVar;
            int xx = x + originCnfVar;
            int xxx = x + originCnfVar*2;
            Lit[0] = toLitCond(x, 1);
            Lit[1] = toLitCond(xx, 0);
            Lit[2] = toLitCond(alpha, 0);
            sat_solver_addclause(pSat, Lit, Lit+3);
            Lit[0] = toLitCond(x, 0);
            Lit[1] = toLitCond(xx, 1);
            Lit[2] = toLitCond(alpha, 0);
            sat_solver_addclause(pSat, Lit, Lit+3);
            Lit[0] = toLitCond(x, 1);
            Lit[1] = toLitCond(xxx, 0);
            Lit[2] = toLitCond(beta, 0);
            sat_solver_addclause(pSat, Lit, Lit+3);
            Lit[0] = toLitCond(x, 0);
            Lit[1] = toLitCond(xxx, 1);
            Lit[2] = toLitCond(beta, 0);
            sat_solver_addclause(pSat, Lit, Lit+3);
        }
        lbool status = l_True;
        int alpha,beta;
        lit* assumps = new lit[2*alphaSize];
        for(int a = 0; a < alphaSize; ++a) {
            for(int b = a+1; b < alphaSize; ++b) {
                for(int q = 0; q < alphaSize; ++q) {
                    alpha = alphaBegin + 2*q;
                    beta = alpha + 1;
                    if(q == a) {
                        assumps[q] = toLitCond(alpha, 0);
                        assumps[alphaSize+q] = toLitCond(beta, 1);
                    }
                    else if(q == b) {
                        assumps[q] = toLitCond(alpha, 1);
                        assumps[alphaSize+q] = toLitCond(beta, 0);
                    }
                    else {
                        assumps[q] = toLitCond(alpha, 1);
                        assumps[alphaSize+q] = toLitCond(beta, 1);
                    }
                }
                status = sat_solver_solve(pSat, assumps, assumps + 2*alphaSize,0,0,0,0);
                if(status == l_False) {
                    break;
                }
            }
            if(status == l_False) {
                break;
            }
        }
        int *pFinalConf;
        int solSize = 0;
        int solution[alphaSize][2];
        for(int i = 0; i < alphaSize; ++i) {
            solution[i][0] = 0;
            solution[i][1] = 0;
        }
        // 0 -> Xc, 1 -> Xb, 2 -> Xa
        if(status == l_False) {
            solSize = sat_solver_final(pSat, &pFinalConf);
            for(int solIdx = 0; solIdx < solSize; ++solIdx) {
                int varible = (pFinalConf[solIdx]/2);
                solution[(varible-alphaBegin)/2][varible%2] = 1;
            }
            printf("PO %s support partition: 1\n",Abc_ObjName(pObj));
            for(int i = 0; i < alphaSize; ++i) {
                int sol_alpha = solution[i][0];
                int  sol_beta = solution[i][1];
                if(sol_alpha == 1 && sol_beta == 1) {
                    // 0,0 -> Xc 
                    printf("0");
                }
                else if(sol_alpha == 1 && sol_beta == 0) {
                    // 0,1 -> Xb
                    printf("2");
                }
                else if(sol_alpha == 0 && sol_beta == 1) {
                    // 0,1 -> Xa
                    printf("1");
                }
                else {
                    // 1,1 -> Xa | Xb
                    printf("0");
                }
            }
            printf("\n");
        }
        else {
            printf("PO %s support partition: 0\n",Abc_ObjName(pObj));
        }
        delete []assumps;
    }
}

int Lsv_CommandORBiDec(Abc_Frame_t* pAbc, int argc, char** argv) {
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
    Lsv_ORBiDec(pNtk);
    return 0;

usage:
    Abc_Print(-2, "usage: lsv_bi_decomposition [-h]\n");
    Abc_Print(-2, "\t        non-trivial or bi-decomposition\n");
    Abc_Print(-2, "\t-h    : print the command usage\n");
    return 1;
}

