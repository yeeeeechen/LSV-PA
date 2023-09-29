#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <string>

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandLsvSimBdd( Abc_Frame_t * pAbc, int argc, char ** argv );

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_sim_bdd", Lsv_CommandLsvSimBdd, 0);
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


// my commands

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Lsv_CommandLsvSimBdd( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    // get input
    char* pInputSeq;
    int c = 0;
    int i, j;
    int piNum;
    Abc_Obj_t* pPo, * pPi;
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    std::vector<std::string> piNames;
    std::vector<bool> inputSeq;

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
    if (argc != globalUtilOptind + 1){
        Abc_Print( -1, "Wrong number of arguments.\n");
        return 1;
    }

    pInputSeq = argv[globalUtilOptind];
    if (strlen(pInputSeq) != Abc_NtkPiNum(pNtk))
    {
        Abc_Print(-1, "Input size does not match.\n");
        return 1;
    }

#ifdef ABC_USE_CUDD
    // record PI names
    piNum = Abc_NtkPiNum(pNtk);
    Abc_NtkForEachPi(pNtk, pPi, i)
    {
        assert( Abc_NtkIsBddLogic(pPi->pNtk) );
        piNames.push_back(std::string(Abc_ObjName(pPi)));
        if (pInputSeq[i] == '0')
        {
            inputSeq.push_back(false);
        }
        else if (pInputSeq[i] == '1') 
        {
            inputSeq.push_back(true);
        }
        else {
            Abc_Print(-1, "Only 0 or 1 allowed.\n");
            return 1;
        }
    }

    // for (i = 0; i < inputSeq.size(); i++){
    //   printf("%s ", piNames[i].c_str());
    //     if (inputSeq[i]){
    //         printf("1\t");
    //     }
    //     else {
    //         printf("0\t");
    //     }
    // }
    // printf("\n");

    Abc_NtkForEachPo(pNtk, pPo, i) 
    {
        Abc_Obj_t* pPoBdd = Abc_ObjFanin0(pPo);
        // Abc_Print(1, "pPo: %s\npPoBdd: %s\n", Abc_ObjName(pPo), Abc_ObjName(pPoBdd));
         
        assert( Abc_NtkIsBddLogic(pPoBdd->pNtk) );
        DdManager * dd = (DdManager *)pPoBdd->pNtk->pManFunc;  
        DdNode* poDdNode = (DdNode *)pPoBdd->pData;
        
        Vec_Int_t* pFaninIds = Abc_ObjFaninVec(pPoBdd);
        // printf("POOO: %s: ", Abc_ObjName(pPo));
        // for (j = 0; j < pFaninIds->nSize; j++){
        //     printf("%d ", pFaninIds->pArray[j]);
        // }
        // printf("\n");


        DdNode* cof = poDdNode;
        for (j = 0; j < pFaninIds->nSize; j++){
            int inputIndex = pFaninIds->pArray[j];
            DdNode* inputDdNode = Cudd_bddIthVar(dd, j);

            // printf("faninId %d:", inputIndex);
            // Cudd_PrintDebug(dd, inputDdNode, 4, 4);
            // printf("cof %d:", j);
            // Cudd_PrintDebug(dd, cof, 4, 4);
            // printf("inputindex:%d \n", inputIndex);
            cof = inputSeq[inputIndex-1] ? Cudd_Cofactor(dd, cof, inputDdNode) : Cudd_Cofactor(dd, cof, Cudd_Not(inputDdNode));
        }
        
        
        // Cudd_PrintDebug(dd, cof, 4, 4);
        if (cof == Cudd_ReadOne(dd))
        {
            Abc_Print(1, "%s: %d\n", Abc_ObjName(pPo), 1);
        }
        else if (cof == Cudd_Not(Cudd_ReadOne(dd)))
        {
            Abc_Print(1, "%s: %d\n", Abc_ObjName(pPo), 0);
        }
        else {
            Abc_Print(1, "LMAO idiot\n");
        }
    }

#endif
    
    return 0;
usage:
    Abc_Print( -2, "usage: lsv_bdd_sim [-h] <input_pattern>\n" );
    Abc_Print( -2, "\t        Function simulation using BDD\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}