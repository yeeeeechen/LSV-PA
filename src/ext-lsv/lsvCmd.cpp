#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <map>
#include "sat/cnf/cnf.h"
using namespace std;

extern "C" {
  Aig_Man_t *  Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}

static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

//------------------------------------
//PA1
bool comp(vector<Abc_Obj_t*> a,vector<Abc_Obj_t*>b){
  return Abc_ObjId(a[0])<Abc_ObjId(b[0]);
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk){
  Abc_Obj_t* pObj;
  int i;
  map<int,vector<Abc_Obj_t*>> msfc;


  Abc_NtkForEachObj(pNtk, pObj, i){
    if(pObj->Type == ABC_OBJ_CONST1  and  Abc_ObjFanoutNum(pObj) != 0) {
      msfc[Abc_ObjId(pObj)].push_back(pObj);
    }else if(Abc_ObjIsNode(pObj)){
      Abc_Obj_t* tmp=pObj;
      while(Abc_ObjFanoutNum(tmp)==1 and !Abc_ObjIsPo(Abc_ObjFanout0(tmp))){
        tmp=Abc_ObjFanout0(tmp);
      }
      msfc[Abc_ObjId(tmp)].push_back(pObj);
    }
    
    
  }

  
  int ind=0;
  vector<vector<Abc_Obj_t*>> sort_map;
  for(auto msfcnode:msfc){
    sort_map.push_back(msfcnode.second);
  }
  sort(sort_map.begin(),sort_map.end(),comp);

  for(auto msfcnode:sort_map){
    cout << "MSFC " << ind << ":";
    cout << " " << Abc_ObjName(msfcnode[0]);
    for(int j=1;j<msfcnode.size();j++){
      cout << "," << Abc_ObjName(msfcnode[j]);
    }
    cout <<endl;
    ind++;
  }
}




int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkPrintMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the msfc nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}



//--------------------------------------------------------
//PA2
void Lsv_NtkOrBidec(Abc_Ntk_t* pNtk){
  Abc_Obj_t* pObj;

  int i,j,k;
  Abc_NtkForEachPo(pNtk,pObj,i){    
    Aig_Obj_t* supObj;
    lit literals[3];
    int *start,*end;

    Abc_Ntk_t* POcone=Abc_NtkCreateCone(pNtk,Abc_ObjFanin0(pObj),Abc_ObjName(pObj),0);
    if(Abc_ObjFaninC0(pObj))Abc_ObjXorFaninC(Abc_NtkPo(POcone,0),0);
    Aig_Man_t* pAig=Abc_NtkToDar(POcone,0,0);
    Cnf_Dat_t* pCnf=Cnf_Derive(pAig,1);
    
    int start_id=pCnf->pVarNums[Aig_ManCo(pAig,0)->Id];
    int Fx_var_num=pCnf->nVars;

    sat_solver* Sat=(sat_solver*)Cnf_DataWriteIntoSolver(pCnf,1,0);
    literals[0]=toLitCond(start_id,0);
    sat_solver_addclause(Sat,literals,literals+1);

    Cnf_DataLift(pCnf,Fx_var_num);
    Cnf_CnfForClause(pCnf,start,end,j)sat_solver_addclause(Sat,start,end);
    literals[0]=toLitCond(start_id+Fx_var_num,1);
    sat_solver_addclause(Sat,literals,literals+1);

    Cnf_DataLift(pCnf,Fx_var_num);
    Cnf_CnfForClause(pCnf,start,end,j)sat_solver_addclause(Sat,start,end);
    literals[0]=toLitCond(start_id+Fx_var_num*2,1);
    sat_solver_addclause(Sat,literals,literals+1);

    int a_start=sat_solver_nvars(Sat);
    int num_sup =Aig_ManCiNum(pAig);

    Aig_ManForEachCi(pAig,supObj,k){
      int ai=sat_solver_addvar(Sat);
      int bi=sat_solver_addvar(Sat);
      int x=pCnf->pVarNums[Aig_ObjId(supObj)]-2*Fx_var_num;
      int xp=x+Fx_var_num;
      int xpp=x+Fx_var_num*2;
      literals[0]=toLitCond(x,1);
      literals[1]=toLitCond(xp,0);
      literals[2]=toLitCond(ai,0);
      sat_solver_addclause(Sat,literals,literals+3);

      literals[0]=toLitCond(x,0);
      literals[1]=toLitCond(xp,1);
      literals[2]=toLitCond(ai,0);
      sat_solver_addclause(Sat,literals,literals+3);

      literals[0]=toLitCond(x,1);
      literals[1]=toLitCond(xpp,0);
      literals[2]=toLitCond(bi,0);
      sat_solver_addclause(Sat,literals,literals+3);

      literals[0]=toLitCond(x,0);
      literals[1]=toLitCond(xpp,1);
      literals[2]=toLitCond(bi,0);
      sat_solver_addclause(Sat,literals,literals+3);

    }
    int sat_res=l_True;
    lit* assumplist=new lit[2*num_sup];
    for(int xi=0;xi<num_sup;xi++){
      for(int xj=xi+1;xj<num_sup;xj++){
        for(int xk=0;xk<num_sup;xk++){

          int ai=a_start+2*xk;
          int bi=a_start+2*xk+1;
          if(xk==xi){
            assumplist[xk]=toLitCond(ai,0);
            assumplist[num_sup+xk]=toLitCond(bi,1);
          }else if(xk==xj){
            assumplist[xk]=toLitCond(ai,1);
            assumplist[num_sup+xk]=toLitCond(bi,0);
          }else{
            assumplist[xk]=toLitCond(ai,1);
            assumplist[num_sup+xk]=toLitCond(bi,1);
          }
        }
        sat_res=sat_solver_solve(Sat,assumplist,assumplist+2*num_sup,0,0,0,0);
        if(sat_res==l_False){
          break;
        }
      }
      if(sat_res==l_False){
        break;
      }
    }
    if(sat_res==l_False){
      int *Final;
      int tmp=sat_solver_final(Sat, &Final);
      vector<vector<bool> > partition(num_sup,vector<bool>(2,false));
      for(int ind=0;ind<tmp;ind++){
        partition[((Final[ind]/2)-a_start)/2][(Final[ind]/2)%2]=true;
      }
      cout<<"PO"<<Abc_ObjName(pObj)<<"support partition: 1"<<endl;
      for(int ind=0;ind<num_sup;ind++){
        if(partition[ind][0] and partition[ind][1]){
          cout<<"0";
        }else if(!partition[ind][0] and partition[ind][1]){
          cout<<"1";
        }else if(partition[ind][0] and !partition[ind][1]){
          cout<<"2";
        }else{
          cout<<"0";
        }
      }
      cout<<endl;
    }else{
      cout<<"PO"<<Abc_ObjName(pObj)<<"support partition: 0"<<endl;
    }
    delete [] assumplist;
  }
}



int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkOrBidec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        prints the satisfiability\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
//--------------------------------------------------------

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