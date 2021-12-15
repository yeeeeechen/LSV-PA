#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "aig/aig/aig.h"
extern "C" Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);

namespace
{
    int Lsv_CommandOrBidec(Abc_Frame_t *pAbc, int argc, char **argv);

    void init(Abc_Frame_t *pAbc)
    {
        Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
    }

    void destroy(Abc_Frame_t *pAbc) {}

    Abc_FrameInitializer_t frame_initializer = {init, destroy};

    struct PackageRegistrationManager
    {
        PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
    } lsvPackageRegistrationManager;

    void Lsv_NtkOrBidec(Abc_Ntk_t *pNtk)
    {
        Abc_Ntk_t *pCone;
        Abc_Obj_t *pPONtk;
        Aig_Obj_t *pPI, *pPO;
        Aig_Man_t *pMan;
        Cnf_Dat_t *pCnf0, *pCnf1, *pCnf2;
        sat_solver *pSat;
        int i, j, k, m, entry;
        int offset, nPI, nFinal, status, *pFinal;
        // int Lits[4];
        Vec_Int_t *vPar, *vLits;
        vPar = Vec_IntStart(0);
        vLits = Vec_IntStart(0);

        Abc_NtkForEachCo(pNtk, pPONtk, i)
        {
            Abc_Print(1, "PO %s support partition: ", Abc_ObjName(pPONtk));

            pCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPONtk), Abc_ObjName(pPONtk), 0);
            if (Abc_ObjFaninC0(pPONtk))
                Abc_ObjXorFaninC(Abc_NtkPo(pCone, 0), 0);
            nPI = Abc_NtkCiNum(pCone);

            // Constant function
            if (nPI == 0)
            {
                Abc_Print(1, "1\n\n");
                Abc_NtkDelete(pCone);
                continue;
            }

            pMan = Abc_NtkToDar(pCone, 0, 0);
            pPO = Aig_ManCo(pMan, 0);

            pCnf0 = Cnf_Derive(pMan, 1);
            pCnf1 = Cnf_DataDup(pCnf0);
            pCnf2 = Cnf_DataDup(pCnf0);
            offset = pCnf0->nVars;
            Cnf_DataLift(pCnf1, offset);
            Cnf_DataLift(pCnf2, offset * 2);

            pSat = sat_solver_new();
            sat_solver_store_alloc(pSat);
            // Vars:                    0~offset-1: X   and structural
            //                   offset~2*offset-1: X'  and structural
            //                 2*offset~3*offset-1: X'' and structural
            //             3*offset~3*offset+nPI-1: alpha
            //       3*offset+nPI~3*offset+2*nPI-1: beta
            sat_solver_setnvars(pSat, offset * 3 + nPI * 2);

            // Encode structural
            for (j = 0; j < pCnf0->nClauses; ++j)
            {
                if (!sat_solver_addclause(pSat, pCnf0->pClauses[j], pCnf0->pClauses[j + 1]) || !sat_solver_addclause(pSat, pCnf1->pClauses[j], pCnf1->pClauses[j + 1]) || !sat_solver_addclause(pSat, pCnf2->pClauses[j], pCnf2->pClauses[j + 1]))
                {
                    Cnf_DataFree(pCnf0);
                    Cnf_DataFree(pCnf1);
                    Cnf_DataFree(pCnf2);
                    sat_solver_delete(pSat);
                    Aig_ManStop(pMan);
                    Abc_NtkDelete(pCone);
                    Vec_IntFree(vPar);
                    Vec_IntFree(vLits);
                    return;
                }
            }

            // Add constraints for alpha and beta
            Aig_ManForEachCi(pMan, pPI, j)
            {
                // Note that the equivalence is enforced when alpha/beta=1 instead of 0
                sat_solver_add_buffer_enable(pSat, pCnf0->pVarNums[pPI->Id], pCnf1->pVarNums[pPI->Id], 3 * offset + j, 0);
                sat_solver_add_buffer_enable(pSat, pCnf0->pVarNums[pPI->Id], pCnf2->pVarNums[pPI->Id], 3 * offset + nPI + j, 0);
            }

            // Add f(X) /\ !f(X') /\ !f(X'')
            sat_solver_add_const(pSat, pCnf0->pVarNums[pPO->Id], 0);
            sat_solver_add_const(pSat, pCnf1->pVarNums[pPO->Id], 1);
            sat_solver_add_const(pSat, pCnf2->pVarNums[pPO->Id], 1);

            // Set all to X_C initially
            Vec_IntGrowResize(vLits, nPI * 2);
            for (j = 0; j < nPI; ++j)
            {
                Vec_IntWriteEntry(vLits, j, toLitCond(3 * offset + j, 0));
                Vec_IntWriteEntry(vLits, j + nPI, toLitCond(3 * offset + nPI + j, 0));
            }

            // Traverse all combinations of seeds
            // Consider only j<k
            status = 1;
            for (j = 0; j < nPI; ++j)
            {
                if (status == l_False)
                    break;
                Vec_IntWriteEntry(vLits, j + nPI, toLitCond(3 * offset + nPI + j, 1));
                for (k = j + 1; k < nPI; ++k)
                {
                    if (status == l_False)
                        break;
                    // Lits[0] = toLitCond(3 * offset + j, 0);
                    // Lits[1] = toLitCond(3 * offset + k, 1);
                    // Lits[2] = toLitCond(3 * offset + nPI + j, 1);
                    // Lits[3] = toLitCond(3 * offset + nPI + k, 0);
                    // status = sat_solver_solve(pSat, Lits, Lits + 4, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
                    Vec_IntWriteEntry(vLits, k, toLitCond(3 * offset + k, 1));
                    status = sat_solver_solve(pSat, Vec_IntEntryP(vLits, 0), Vec_IntEntryP(vLits, 0) + 2 * nPI, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
                    // UNSAT
                    if (status == l_False)
                    {
                        nFinal = sat_solver_final(pSat, &pFinal);
                        Abc_Print(1, "1\n");

                        Vec_IntClear(vPar);
                        Vec_IntFill(vPar, nPI, 3);
                        // Check alpha
                        Vec_IntForEachEntry(vPar, entry, m)
                        {
                            for (int l = 0; l < nFinal; ++l)
                            {
                                if (lit_var(pFinal[l]) == m + 3 * offset)
                                {
                                    Vec_IntWriteEntry(vPar, m, 2);
                                    break;
                                }
                            }
                        }
                        // Check beta
                        Vec_IntForEachEntry(vPar, entry, m)
                        {
                            for (int l = 0; l < nFinal; ++l)
                            {
                                if (lit_var(pFinal[l]) == m + 3 * offset + nPI)
                                {
                                    // Not forced => forced in X_B
                                    if (entry == 3)
                                        Vec_IntWriteEntry(vPar, m, 1);
                                    // Already forced in X_A => forced in X_C
                                    else
                                    {
                                        assert(entry == 2);
                                        Vec_IntWriteEntry(vPar, m, 0);
                                    }
                                    break;
                                }
                            }
                            // Put all unforced to X_A
                            if (Vec_IntEntry(vPar, m) == 3)
                                Vec_IntWriteEntry(vPar, m, 2);
                        }
                        Vec_IntForEachEntry(vPar, entry, m)
                            Abc_Print(1, "%d", entry);
                        Abc_Print(1, "\n");
                    }
                    Vec_IntWriteEntry(vLits, k, toLitCond(3 * offset + k, 0));
                }
                Vec_IntWriteEntry(vLits, j + nPI, toLitCond(3 * offset + nPI + j, 0));
            }
            if (status != l_False)
                Abc_Print(1, "0\n");
            Cnf_DataFree(pCnf0);
            Cnf_DataFree(pCnf1);
            Cnf_DataFree(pCnf2);
            sat_solver_delete(pSat);
            Aig_ManStop(pMan);
            Abc_NtkDelete(pCone);
        }
        Vec_IntFree(vPar);
        Vec_IntFree(vLits);
    }

    int Lsv_CommandOrBidec(Abc_Frame_t *pAbc, int argc, char **argv)
    {
        Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
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
            return 1;
        }
        if (!Abc_NtkIsStrash(pNtk))
        {
            Abc_Print(-1, "Works only for strashed AIG (run \"strash\").\n");
            return 1;
        }
        Lsv_NtkOrBidec(pNtk);
        return 0;

    usage:
        Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
        Abc_Print(-2, "\t        decide whether each PO is OR bi-decomposable and if so, return the decomposition.\n");
        Abc_Print(-2, "\t-h    : print the command usage\n");
        return 1;
    }

} // unamed namespace