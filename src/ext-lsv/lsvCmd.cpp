#include<algorithm>
#include <vector>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#define Abc_NtkForEachNode_pa1(pNtk,pNode,i) for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ ) if ( (pNode) == NULL || (!Abc_ObjIsNode(pNode) &&!(pNode->Type == ABC_OBJ_CONST1) )) {} else
extern "C" Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);
//read ./lsv_fall_2021/pa1/4bitadder_s.blif 
//strash
//lsv_print_msfc
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrint_msfc(Abc_Frame_t* pAbc, int argc, char** argv);
static int lsv_or_bidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  //lsv_print_msfc
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrint_msfc, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", lsv_or_bidec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};
 int sat_solver_add_buffer_reverse_enable( sat_solver * pSat, int iVarA, int iVarB, int iVarEn, int fCompl )
{
    lit Lits[3];
    int Cid;
    assert( iVarA >= 0 && iVarB >= 0 && iVarEn >= 0 );

    Lits[0] = toLitCond( iVarA, 0 );
    Lits[1] = toLitCond( iVarB, !fCompl );
    Lits[2] = toLitCond( iVarEn, 0 );
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );

    Lits[0] = toLitCond( iVarA, 1 );
    Lits[1] = toLitCond( iVarB, fCompl );
    Lits[2] = toLitCond( iVarEn, 0 );
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );
    return 2;
}
struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

class Mfsc_set{
  public:
  static int merge(Mfsc_set* a,Mfsc_set* b, std::vector<Mfsc_set*>&total){
    Mfsc_set* big ;
    Mfsc_set*small;
    if(a->mfscset.size()>b->mfscset.size()){
      big=a;  
      small=b;
    }else{
      big=b;
      small=a;
    }
    int id;
    for(int i=0;i<small->mfscset.size();++i){
      id=small->mfscset[i];
      total[id]=big;
    }
    big->mergeInto(small);
    return 0;
  }
  Mfsc_set(int id){
    this->mfscset.push_back(id);
    mark=false;
  }
  int head(){return mfscset[0];}

  //a1 merge into this
  int mergeInto(Mfsc_set* a){
    for(int i=0;i<a->mfscset.size();++i){
      this->mfscset.push_back(a->mfscset[i]);
    }
    a->mfscset.clear();
    return 0;
  }
  void sort(){std::sort(mfscset.begin(),mfscset.end());}
  void show(int i){
    
    printf("MSFC %d: ",i);
    for(int i=0;i<mfscset.size();++i){
      if(i!=0)printf(",");
      printf ("n%d",mfscset[i]);
    }
    printf ("\n");
  }
  bool mark;
  private:
  std::vector<int> mfscset;
  
};
bool compareMFSC_set(Mfsc_set* a,Mfsc_set*b){
  return a->head() <b->head();
}
void Lsv_NtkPrintNodes(Abc_Ntk_t*  pNtk) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    Abc_Obj_t* pFanin;
    int j;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s ,type=%d \n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin),Abc_ObjType(pFanin));
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
void Lsv_NtkPrint_msfc(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  std::vector<Mfsc_set*>total;
  for (int i=0;i<Vec_PtrSize((pNtk)->vObjs);++i){
    total.push_back(nullptr);
  }
  // init  :for each  node -> set
  Abc_NtkForEachNode_pa1(pNtk, pObj, i) {
    int thisid=Abc_ObjId(pObj);
    //printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    total[thisid]=new Mfsc_set(Abc_ObjId(pObj));
    /*
    Abc_Obj_t* pFanin;
    int j;
    //Abc_ObjForEachFanout
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }
    */

  
  }
  // start merging mfsc set
  Abc_NtkForEachNode_pa1(pNtk, pObj, i) {
    //printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    int thisid=Abc_ObjId(pObj);
    int j;
    //Abc_ObjForEachFanout
    Abc_Obj_t* pFanout;
    if(Abc_ObjFanoutNum(pObj)==0){
      total[thisid]->mark=true;
    }
    if(Abc_ObjFanoutNum(pObj)>1)continue;
    Abc_ObjForEachFanout(pObj, pFanout, j){
      if(Abc_ObjType(pFanout)!=7  )
      {//printf("Object Id = %d not node\n",Abc_ObjId(pFanout));
      continue;
      }
      Mfsc_set::merge(total[thisid],total[Abc_ObjId(pFanout)],total);
      
      //total[thisid]->show(Abc_ObjId(pFanout));
    }
  }
  //printf("leave");
  //start collecting and sorting sets
  std::vector<Mfsc_set*> finalset; 
  for(int i=0;i<total.size();++i){
    if(total[i]==nullptr ||total[i]->mark)continue;
    total[i]->sort();
   // total[i]->show(i);
    finalset.push_back(total[i]);
    total[i]->mark=true;
  }
  total.clear();
  //print out the mfsc sets
  std::sort(finalset.begin(),finalset.end(),compareMFSC_set);
  for(int i=0;i<finalset.size();++i){
    finalset[i]->show(i);
  }
  //printf("end\n");
}
int Lsv_CommandPrint_msfc(Abc_Frame_t* pAbc, int argc, char** argv) {

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
  Lsv_NtkPrint_msfc(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the  maximum single-fanout cones\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
//return if it is valid seed
bool seed2assumption(lit *assumptions,const std::vector<int>&seeds,const std::vector<int>& ais,const std::vector<int>& bis){
  int tob=seeds.size();
  for(int i=0;i<tob;i++){
    if(seeds[i]==0){
      assumptions[i]=toLitCond(ais[i],1);
      assumptions[i+tob]=toLitCond(bis[i],1);
    }else if(seeds[i]==1){
      assumptions[i]=toLitCond(ais[i],1);
      assumptions[i+tob]=toLitCond(bis[i],0);
    }else if(seeds[i]==2){
      assumptions[i]=toLitCond(ais[i],0);
      assumptions[i+tob]=toLitCond(bis[i],1);
    }
  }
  return true;
}
void toans(int nFinal,int * pFinal,const std::vector<int>& ais,const std::vector<int>& bis){
  std::vector<int>ans;
  int id;
  for (int i=0;i<ais.size();i++){
    ans.push_back(3);
  }
  for (int i=0;i<nFinal;i++){
    id=pFinal[i]/2;
    for(int j=0;j<ais.size();j++){
      if(ais[j]==id){
        ans[j]^=1;
        break;
      }else if(bis[j]==id){
        ans[j]^=2;
        break;
      }
    }
  }
  for(int i=0;i<ans.size();i++){
    if(ans[i]==3){
      printf("1");
    }else{
      printf("%d",ans[i]);
    }
  }
  printf("\n");

}
void print_one_ORbid(sat_solver *solver,const std::vector<int>& ais,const std::vector<int>& bis){
  bool endflag=false;
  int tob=ais.size();
  lit *assumptions = new lit[ais.size()*2];
  std::vector<int>seeds;
  int result;
  int *pFinal;
  int nfinal;
  for (int i=0;i<tob;i++){
    seeds.push_back(0);
  }
  

    //generate next  seed
    for(int i=0;i<seeds.size();i++){
      for(int j=i+1;j<seeds.size();j++){
        seeds[i]=1;
        seeds[j]=2;
        //use seed to create assumption
        seed2assumption(assumptions,seeds,ais,bis);
        //use assumption to solve
        result=sat_solver_solve(solver,assumptions,assumptions+ais.size()*2,0,0,0,0);
        //check result
        if(result==l_True){//sat
         // printf("sat %d %d\n",i,j);
        }else{//unsat
          printf("1\n");
          nfinal=sat_solver_final(solver,&pFinal);
          
          
          toans(nfinal,pFinal,ais,bis);
          
          endflag=true;
          break;
        }
        //
        seeds[i]=0;
        seeds[j]=0;
      }
      
      if(endflag)break;
    }
    if(!endflag)printf("0\n");
    //delete [] assumptions;
  
}
void lsv_print_ORbid(Abc_Ntk_t*  pNtk){
  Abc_Obj_t* pObj;
  Aig_Obj_t* conePI;
  int i,j;
  Abc_Ntk_t * pNtk_cone;
  Aig_Man_t *pMan;
  Cnf_Dat_t *f1Cnf;
  Cnf_Dat_t *f2Cnf;
  Cnf_Dat_t *f3Cnf;
  int nclasues;
  int liftconst;
  std::vector<int> f1xis;
  std::vector<int> ais;
  std::vector<int> bis;
  sat_solver *solver;
  int count=0;
  Abc_NtkForEachPo(pNtk, pObj, i){
    f1xis.clear();
    ais.clear();
    bis.clear();
    pNtk_cone=Abc_NtkCreateCone(pNtk,Abc_ObjFanin0(pObj),Abc_ObjName(pObj),0);// 0 => don't keep all pi(DFS reached only)
    if (Abc_ObjFaninC0(pObj))
    {
      Abc_NtkPo(pNtk_cone, 0)->fCompl0 ^= 1;
    }
    printf("PO %s support partition: ",Abc_ObjName(pObj));
    count++;
    
    pMan = Abc_NtkToDar(pNtk_cone, 0, 0);
    assert(Aig_ManCoNum(pMan)==1);
    f1Cnf = Cnf_Derive(pMan, 0); //0 => assert all output to 1
    f2Cnf = Cnf_DataDup(f1Cnf);
    liftconst=f2Cnf->nVars;
    Cnf_DataLift(f2Cnf,liftconst);
    //todo:modify the output phase start
    nclasues=f2Cnf->nClauses;
    //printf("before %d",f2Cnf->pClauses[nclasues-1][0]);
    f2Cnf->pClauses[nclasues-1][0]=f2Cnf->pClauses[nclasues-1][0] ^ 1;  //change assert output 1 => output 0
    //printf("after %d",f2Cnf->pClauses[nclasues-1][0]);
    //end
    f3Cnf = Cnf_DataDup(f2Cnf);
    assert(liftconst==f3Cnf->nVars);
    Cnf_DataLift(f3Cnf,liftconst);
    //get x0-xi -> varID
    Aig_ManForEachCi(pMan,conePI,j){
      f1xis.push_back(f1Cnf->pVarNums[conePI->Id]);
    }
    //create sat solver ,add:  f(x) and !f'(x) and !f''(x)
    solver = (sat_solver *)Cnf_DataWriteIntoSolver(f1Cnf, 1, 0);
    if(!solver){
      printf("0\n");
      continue;
    }
    solver = (sat_solver *)Cnf_DataWriteIntoSolverInt(solver, f2Cnf, 1, 0);
    if(!solver){
      printf("0\n");
      continue;
    }
    solver = (sat_solver *)Cnf_DataWriteIntoSolverInt(solver, f3Cnf, 1, 0);
    //new ai , bi
    //printf("create ai bi\n");
    for(int k=0;k<f1xis.size();k++){
      ais.push_back(sat_solver_addvar(solver));
      bis.push_back(sat_solver_addvar(solver));
      //printf("a%d b%d ",ais[k],bis[k]);
    }
    //printf("create ai bi end\n");
    //create xi vs ai bi
    for(int k=0;k<f1xis.size();k++){
     // printf("f1%d f2%d ",f1xis[k],f1xis[k]+liftconst);
      sat_solver_add_buffer_reverse_enable(solver,f1xis[k],f1xis[k]+liftconst,ais[k],0);
      sat_solver_add_buffer_reverse_enable(solver,f1xis[k],f1xis[k]+2*liftconst,bis[k],0);
    }
   // printf("create buffer end\n");
    //assume ai ,bi  incremental sat solving
    print_one_ORbid(solver,ais,bis);
    //sat_solver_add_const()
    /*
    delete f1Cnf;
    delete f2Cnf;
    delete f3Cnf;
    delete conePI;*/
    

  }


}

int lsv_or_bidec(Abc_Frame_t* pAbc, int argc, char** argv){
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
  lsv_print_ORbid(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t      find OR bi-decomposable under a variable partition \n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

