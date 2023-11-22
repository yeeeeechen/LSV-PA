#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>

static int Lsv_CommandLsvSymBdd( Abc_Frame_t * pAbc, int argc, char ** argv );
// static int Lsv_CommandLsvSymSat( Abc_Frame_t * pAbc, int argc, char ** argv );

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_bdd", Lsv_CommandLsvSymBdd, 0);
//   Cmd_CommandAdd(pAbc, "LSV", "lsv_sym_sat", Lsv_CommandLsvSymSat, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

int Lsv_CommandLsvSymBdd(Abc_Frame_t* pAbc, int argc, char** argv)
{
    // get input
    int c = 0;
    int i, j;
    int inputPin1, inputPin2;
    int outputPin;
    int piNum, poNum;
    Abc_Obj_t* pPo, * pPi1, * pPi2;
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);

    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF )
    {
        switch ( c )
        {
            case 'h':
                goto usage;
            default:
                goto usage;
        }
    }
    if (!Abc_NtkIsBddLogic(pNtk))
    {
        Abc_Print( -1, "Convert to BDD first.\n");
        return 1;
    }
    if (argc != globalUtilOptind + 3){
        Abc_Print( -1, "Wrong number of arguments.\n");
        return 1;
    }

    outputPin = strtol(argv[globalUtilOptind], NULL, 10);
    inputPin1 = strtol(argv[globalUtilOptind+1], NULL, 10);
    inputPin2 = strtol(argv[globalUtilOptind+2], NULL, 10);
    piNum = Abc_NtkPiNum(pNtk);
    poNum = Abc_NtkPoNum(pNtk);
    if (!(outputPin < poNum && 0 <= outputPin)){
        Abc_Print( -1, "Output pin out of range.\n");
        return 1;
    } 
    
    if (!(inputPin1 < piNum && 0 <= inputPin1) || !(inputPin2 < piNum && 0 <= inputPin2)){
        Abc_Print( -1, "Input pin out of range.\n");
        return 1;
    } 

#ifdef ABC_USE_CUDD
    pPo = Abc_NtkPo(pNtk, outputPin);
    pPi1 = Abc_NtkPi(pNtk, inputPin1);
    pPi2 = Abc_NtkPi(pNtk, inputPin2);

    if (Abc_NtkIsBddLogic(pPo->pNtk)){
        Abc_Obj_t* pPoBdd = Abc_ObjFanin0(pPo);
        DdManager* dd = (DdManager*)pPoBdd->pNtk->pManFunc;
        DdNode* poDdNode = (DdNode*)pPoBdd->pData;

        Vec_Int_t* pFaninIds = Abc_ObjFaninVec(pPoBdd);

        bool pi1Found = false, pi2Found = false;
        for (i = 0; i < pFaninIds->nSize; i++){
            int inputIndex = pFaninIds->pArray[i];
            if (inputIndex == inputPin1){
                
            }
        }
    }

#endif
    
    return 0;
usage:
    Abc_Print( -2, "usage: lsv_bdd_sim [-h] <output pin> <input pin 1> <input pin 2>\n" );
    Abc_Print( -2, "\t        Tests whether the output pin is symmertric wrt the two input pins.\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}