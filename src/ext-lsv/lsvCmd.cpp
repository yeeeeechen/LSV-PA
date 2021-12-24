#include <vector>
//#include <queue>
#include <algorithm>
#include <iostream>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
using namespace std;

extern "C" Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);

static int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMsfc, 0);
    Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
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

int Msfc(Abc_Obj_t* pObj,
         vector<vector<Abc_Obj_t*>*>* msfcs,
         vector<Abc_Obj_t*>* msfc) {
    if(pObj->fMarkA)                  //if already is a root
        return 0;
    if(pObj->Type == ABC_OBJ_PI)
        return 0;
    if(Abc_ObjFanoutNum(pObj) > 1) {
        vector<Abc_Obj_t*>* temp = new vector<Abc_Obj_t*>();
        temp->push_back(pObj);
        msfcs->push_back(temp);
        pObj->fMarkA = 1;
        return 0;
    }
    Abc_Obj_t* pFanin;
    int i;
    msfc->push_back(pObj);
    Abc_ObjForEachFanin(pObj, pFanin, i) {
        //pFanin->fMarkA = 1;
        Msfc(pFanin, msfcs, msfc); 
    }
}

int FindMsfc(vector<vector<Abc_Obj_t*>*>* msfcs){
    Abc_Obj_t* pFanin;
    int j;
    for(int i = 0; i < msfcs->size(); ++i) {
        Abc_ObjForEachFanin(msfcs->at(i)->at(0), pFanin, j){
            Msfc(pFanin, msfcs, msfcs->at(i));
        }
    }
}

int Find_POs(Abc_Ntk_t* pNtk, vector<vector<Abc_Obj_t*>*>* msfcs) {
    int i, j;
    Abc_Obj_t* Po;
    Abc_Obj_t* pFanin;
    Abc_NtkForEachPo(pNtk, Po, i) {
        Abc_ObjForEachFanin(Po, pFanin, j) {
            if(pFanin->Type == ABC_OBJ_PI)
                continue;
            if(pFanin->fMarkA)
                continue;
            vector<Abc_Obj_t*>* temp = new vector<Abc_Obj_t*>();
            temp->push_back(pFanin);
            msfcs->push_back(temp);
            pFanin->fMarkA = 1;
        }
    }
    FindMsfc(msfcs);
}

bool cmp1(Abc_Obj_t* a, Abc_Obj_t* b){
    return Abc_ObjId(a) < Abc_ObjId(b);
}

bool cmp2(vector<Abc_Obj_t*>* a, vector<Abc_Obj_t*>* b) {
    return Abc_ObjId(a->at(0)) < Abc_ObjId(b->at(0));
}

int Lsv_CommandPrintMsfc(Abc_Frame_t* pAbc, int argc, char** argv) {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    Abc_NtkCleanMarkA(pNtk);
    //Abc_NtkCleanMarkB(pNtk);
    vector<vector<Abc_Obj_t*>*>* msfc = new vector<vector<Abc_Obj_t*>*>();
    Find_POs(pNtk, msfc);
    for(int i = 0; i < msfc->size(); ++i) {
        sort(msfc->at(i)->begin(), msfc->at(i)->end(), cmp1);
    }
    sort(msfc->begin(), msfc->end(), cmp2);
    //cout << "checkpoint\n";
    for(int i = 0; i < msfc->size(); ++i) {
        cout << "MSFC " << i << ": " << Abc_ObjName(msfc->at(i)->at(0));
        for(int j = 1; j < msfc->at(i)->size(); ++j){
            cout << "," << Abc_ObjName(msfc->at(i)->at(j));
        }
        cout << endl;
        delete msfc->at(i);
    }
    delete msfc;
    Abc_NtkCleanMarkA(pNtk);
    //Abc_NtkCleanMarkB(pNtk);
}

int Lsv_CommandOrBidec(Abc_Frame_t *pAbc, int argc, char **argv){
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    int i, j;
    Abc_Obj_t* Po;
    Abc_NtkForEachPo(pNtk, Po, i) {
        Abc_Ntk_t* cone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(Po), Abc_ObjName(Po), 0);
        if (Abc_ObjFaninC0(Po))
            Abc_ObjXorFaninC(Abc_NtkPo(cone, 0), 0);
        Aig_Man_t* aig = Abc_NtkToDar(cone, 0, 0);
        int pi[Aig_ManCiNum(aig)];
        int po[Aig_ManCoNum(aig)];
        Aig_Obj_t_ *pObj;
        Aig_ManForEachCi(aig, pObj, j){                  //pi
            pi[j] = pObj->Id;
        }
        Aig_ManForEachCo(aig, pObj, j){                  //po
            po[j] = pObj->Id;
        }
        
        Cnf_Dat_t* cnf = Cnf_Derive(aig, 1);
        sat_solver *sat = sat_solver_new();
        Cnf_DataWriteIntoSolverInt(sat, cnf, 1, 0);
        int nvars = cnf->nVars;
        Cnf_Dat_t *cnf1 = Cnf_DataDup(cnf);
        Cnf_DataLift(cnf1, nvars);
        Cnf_DataWriteIntoSolverInt(sat, cnf1, 1, 0);
        Cnf_DataLift(cnf1, nvars);
        Cnf_DataWriteIntoSolverInt(sat, cnf1, 1, 0);
        sat_solver_setnvars(sat, 2 * nvars);
        lit lits[3];
        lits[0] = toLitCond(cnf->pVarNums[po[0]], 0);
        sat_solver_addclause(sat, lits, lits + 1);
        for (int k = 1; k < 3; ++k){
            lits[0] = toLitCond(cnf->pVarNums[po[0]] + k * nvars, 1);
            sat_solver_addclause(sat, lits, lits + 1);
        }
        for(int k = 0; k < Aig_ManCiNum(aig); ++k) {
            lits[0] = toLitCond(cnf->pVarNums[pi[k]], 1);
            lits[1] = toLitCond(cnf->pVarNums[pi[k]] + nvars, 0);
            lits[2] = toLitCond(cnf->pVarNums[pi[k]] + 3 * nvars, 0);         //alpha
            sat_solver_addclause(sat, lits, lits + 3);                        //!x_k + x'_k + a_k
            lits[0] = toLitCond(cnf->pVarNums[pi[k]], 0);
            lits[1] = toLitCond(cnf->pVarNums[pi[k]] + nvars, 1);
            lits[2] = toLitCond(cnf->pVarNums[pi[k]] + 3 * nvars, 0);         //alpha
            sat_solver_addclause(sat, lits, lits + 3);
            lits[0] = toLitCond(cnf->pVarNums[pi[k]], 1);
            lits[1] = toLitCond(cnf->pVarNums[pi[k]] + 2 * nvars, 0);
            lits[2] = toLitCond(cnf->pVarNums[pi[k]] + 4 * nvars, 0);         //beta
            sat_solver_addclause(sat, lits, lits + 3);
            lits[0] = toLitCond(cnf->pVarNums[pi[k]], 0);
            lits[1] = toLitCond(cnf->pVarNums[pi[k]] + 2 * nvars, 1);
            lits[2] = toLitCond(cnf->pVarNums[pi[k]] + 4 * nvars, 0);
            sat_solver_addclause(sat, lits, lits + 3);
        }
        int check = 0;
        lit assumption[2 * Aig_ManCiNum(aig)];
        for(int k = 0; k < Aig_ManCiNum(aig); ++k) {
            assumption[2 * k] = toLitCond(cnf->pVarNums[pi[k]] + 3 * nvars, 1);      //alpha_k = 0
            assumption[2 * k + 1] = toLitCond(cnf->pVarNums[pi[k]] + 4 * nvars, 1);  //beta_k = 0
        }
        for(int k = 0; k < Aig_ManCiNum(aig); ++k) {                                 //incremental sat solving
            if (check) break;
            for(int l = k + 1; l < Aig_ManCiNum(aig); ++l) {
                assumption[2 * k] = toLitCond(cnf->pVarNums[pi[k]] + 3 * nvars, 0);      //alpha_k = 1
                assumption[2 * l + 1] = toLitCond(cnf->pVarNums[pi[l]] + 4 * nvars, 0);  //beta_l = 1
                if(sat_solver_solve(sat, assumption, assumption + 2 * Aig_ManCiNum(aig), 0, 0, 0, 0) == l_False) {
                    check = 1;
                    cout << "PO " << Abc_ObjName(Po) << " support partition: 1" << endl;
                    for(int m = 0; m < Aig_ManCiNum(aig); ++m) {
                        if(m == k)cout << "1";
                        else if(m == l)cout << "2";
                        else cout << "0";
                    }
                    cout << endl;
                    break;
                }
                assumption[2 * k] = toLitCond(cnf->pVarNums[pi[k]] + 3 * nvars, 1);      //alpha_k = 0
                assumption[2 * l + 1] = toLitCond(cnf->pVarNums[pi[l]] + 4 * nvars, 1);  //beta_l = 0
            }
        }
        if(!check)cout << "PO " << Abc_ObjName(Po) << " support partition: 0" << endl;
    }
    return 0;
}