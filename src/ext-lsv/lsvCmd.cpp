#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/bsat/satStore.h"
#include "sat/cnf/cnf.h"
#include <vector>
#include <algorithm>
using namespace std;
extern "C" {
  Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBidec( Abc_Frame_t * pAbc, int argc, char **argv );
void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
}
int Lsv_OrBidec(Abc_Ntk_t* pNtk) {
  Abc_Obj_t *pPo;
  Abc_Ntk_t *pCone;
  Aig_Man_t *pAig;
  Aig_Obj_t *pAigObj;
  Cnf_Dat_t *pCnf;
  sat_solver_t *pSat;
  int sat;
  int *pFinalConf;
  int ConflictFinalSize;
  int *result;
  int i, j;
  int *pBeg,*pEnd;
  int POvarId,Shift,supportNum,aBegId,bBegId,varId;
  lit clause[3];
  lit *assumList;


  Abc_NtkForEachPo(pNtk, pPo, i) {
    
    pCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 0);
    if (Abc_ObjFaninC0(pPo))
      Abc_ObjXorFaninC(Abc_NtkPo(pCone, 0), 0);
    
    pAig = Abc_NtkToDar(pCone, 0, 0);

    
    pCnf = Cnf_Derive(pAig, 1);
    pSat = (sat_solver_t*)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
    Shift = pCnf->nVars;
    POvarId = pCnf->pVarNums[Aig_ManCo(pAig, 0)->Id];
    clause[0] = toLitCond(POvarId, 0);
    sat_solver_addclause(pSat, clause, clause+1);
    printf("PO %s support partition: ", Abc_ObjName(pPo));
    

    Cnf_DataLift(pCnf, Shift);
    Cnf_CnfForClause(pCnf, pBeg, pEnd, j) {
      sat_solver_addclause(pSat, pBeg, pEnd);
    }
    clause[0] = toLitCond(POvarId + Shift, 1);
    sat_solver_addclause(pSat, clause, clause+1);
    
    Cnf_DataLift(pCnf, Shift);
    Cnf_CnfForClause(pCnf, pBeg, pEnd, j) {
      sat_solver_addclause(pSat, pBeg, pEnd);
    }
    clause[0] = toLitCond(POvarId + 2*Shift, 1);
    sat_solver_addclause(pSat, clause, clause+1);
    
    
    supportNum = Aig_ManCiNum(pAig);
    aBegId = 3*Shift;
    bBegId = supportNum+3*Shift;
    sat_solver_setnvars(pSat, 3*Shift + 2*supportNum);
    
    
    Aig_ManForEachCi(pAig, pAigObj, j) {
      varId = pCnf->pVarNums[pAigObj->Id] - 2*Shift;
      
      clause[0] = toLitCond(varId, 1);
      clause[1] = toLitCond(varId + Shift, 0);
      clause[2] = toLitCond(aBegId + j, 0);
      sat_solver_addclause(pSat, clause, clause+3);
      
      clause[0] = toLitCond(varId + Shift, 1);
      clause[1] = toLitCond(varId, 0);
      clause[2] = toLitCond(aBegId + j, 0);
      sat_solver_addclause(pSat, clause, clause+3);
      
      clause[0] = toLitCond(varId, 1);
      clause[1] = toLitCond(varId + 2*Shift, 0);
      clause[2] = toLitCond(bBegId + j, 0);
      sat_solver_addclause(pSat, clause, clause+3);
      
      clause[0] = toLitCond(varId + 2*Shift, 1);
      clause[1] = toLitCond(varId, 0);
      clause[2] = toLitCond(bBegId + j, 0);
      sat_solver_addclause(pSat, clause, clause+3);
    }

    assumList = new lit[2*supportNum];
    result = new int[supportNum];
    for (int k = 0; k < 2*supportNum; k++) {
      assumList[k] = toLitCond(aBegId + k, 1);
    }
    for (int k = 0; k < supportNum; k++) {
      result[k] = 3;
    }

    sat = 0;
    for (int a = 0; a < supportNum; a++) {
      assumList[a] = lit_neg(assumList[a]);
      for (int b = a+1+supportNum; b < 2*supportNum; b++) {
        assumList[b] = lit_neg(assumList[b]);
        
        sat = sat_solver_solve(pSat, assumList, assumList + 2*supportNum, 0, 0, 0, 0);
        if (sat == -1) {
          ConflictFinalSize = sat_solver_final(pSat, &pFinalConf);
          for (int k = 0; k < ConflictFinalSize; k++){
            assert(lit_var(pFinalConf[k]) >= aBegId);
            if (lit_var(pFinalConf[k]) < bBegId){
              result[lit_var(pFinalConf[k])-aBegId] -= 2;
            }
            else {
              result[lit_var(pFinalConf[k])-bBegId] -= 1;
            }
          }

          printf("1\n");
          for (int k = 0; k < supportNum; k++) {
            printf("%d",result[k]);
          }
          break;
        }
        assumList[b] = lit_neg(assumList[b]);
      }
      assumList[a] = lit_neg(assumList[a]);
      if (sat == -1) break;
    }
    if (sat == -1) printf("\n");
    else printf("0\n");
    delete [] assumList;
    delete [] result;
    
  }
  return 0;
}
void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;
void recvisit(Abc_Obj_t* pObj,int count,vector <int>* a)
{
  for(int j=0;j<Abc_ObjFaninNum(pObj);j++)
  {
    if(Abc_ObjIsNode(Abc_ObjFanin(pObj,j))&&Abc_ObjFanoutNum(Abc_ObjFanin(pObj,j))==1)
    {
      a->push_back(Abc_ObjId(Abc_ObjFanin(pObj,j)));
      recvisit(Abc_ObjFanin(pObj,j),count,a);
    }
  }
}
bool vcompare(vector<int>& a,vector<int>& b)
{
  return a[0]<b[0];
}

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  int count=0;
  vector <vector <int> > a(Abc_NtkNodeNum(pNtk)+1);

  if(Abc_ObjFanoutNum(Abc_NtkObj(pNtk,0))!=0)
  {
    if(Abc_ObjFanoutNum(Abc_NtkObj(pNtk,0))>1||(Abc_ObjIsPo(Abc_ObjFanout0(Abc_NtkObj(pNtk,0)))))
    {
      vector<int> c;
      c.push_back(0);
      a[count]=c;
      count++;
    }
  }
  
  Abc_NtkForEachNode(pNtk, pObj, i){
      if(Abc_ObjFanoutNum(pObj)>1||Abc_ObjIsPo(Abc_ObjFanout0(pObj)))
      {
        vector<int> c;
        c.push_back(Abc_ObjId(pObj));
        recvisit(pObj,count,&c);
        a[count]=c;
        count++;
      }
  }
  for(int j=0;j<count;j++)
  {
    
    sort(a[j].begin(),a[j].end());
     
  }
  a.resize(count);
  sort(a.begin(),a.end(),vcompare);
  for(int j=0;j<count;j++)
  {

    printf("MSFC %d: ",j);
    for(int n=0;n<a[j].size();n++)
    {
      if(n==0)
        printf("n%d",a[j][0]);
      else
        printf(",n%d",a[j][n]);
    }
    printf("\n");
  }  
}
int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  Lsv_OrBidec(pNtk);
  return 0;
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
