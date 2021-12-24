#include <iostream>
#include <algorithm>
#include <stack>
#include <vector>
#include <queue>
#include "base/abc/abc.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satSolver.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/bsat/satVec.h"

using namespace std;
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

extern "C" {
 Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
}


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandPrintNodes, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct initializer {
  initializer() { Abc_FrameAddInitializer(&frame_initializer); }
} TEST;

bool compare_level(Abc_Obj_t* a,Abc_Obj_t* b){
  
  return (Abc_ObjLevel(a)>Abc_ObjLevel(b));

}

void Cnf_DataPrint1( Cnf_Dat_t * p, int fReadable )
{
    FILE * pFile = stdout;
    int * pLit, * pStop, i;
    fprintf( pFile, "p cnf %d %d\n", p->nVars, p->nClauses );
    for ( i = 0; i < p->nClauses; i++ )
    {
        for ( pLit = p->pClauses[i], pStop = p->pClauses[i+1]; pLit < pStop; pLit++ )
            fprintf( pFile, "%d ", *pLit );
        fprintf( pFile, "\n" );
    }
    fprintf( pFile, "\n" );
}

void Cnf_DataCollectPiSatNums_1( Cnf_Dat_t * pCnf, Aig_Man_t * p )
{
    Aig_Obj_t * pObj;
    int i;
    cout<<"pi_num:"<<p->nTruePis<<endl;
    Aig_ManForEachCi( p, pObj, i )
        cout<<i<<" "<< pCnf->pVarNums[pObj->Id]<<endl;
   
}

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {

  Abc_Obj_t* pObj_po;


  int i;
  Abc_NtkForEachPo( pNtk, pObj_po, i ){
    
    Aig_Obj_t * pObj;
    int temp_var;
    int * pBeg;
    int * pEnd;
    int i1;
    Abc_Ntk_t* pNtk_part;
    Aig_Man_t * aig_man1;
    Cnf_Dat_t * pCnf;
    int status;

    pNtk_part=Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pObj_po), Abc_ObjName(pObj_po), 0 );
    if ( Abc_ObjFaninC0(pObj_po) )
        Abc_ObjXorFaninC( Abc_NtkPo(pNtk_part, 0), 0 ); 

    aig_man1 = Abc_NtkToDar( pNtk_part, 0, 0 );

    //cout<<Aig_ManCiNum( aig_man1 )<<endl;


    pCnf=Cnf_Derive( aig_man1,  Aig_ManCoNum(aig_man1) );

    sat_solver * pSat = (sat_solver *)Cnf_DataWriteIntoSolver(pCnf, 1, 0); 

    //pSat->fPrintClause=1;
    //status = sat_solver_solve( pSat, NULL,  NULL , 0, 0, 0, 0 );

    //cout<<status<<endl;


    int num_vars= pCnf->nVars;
    sat_solver_setnvars(pSat,num_vars*2);
    //Cnf_DataCollectPiSatNums_1( pCnf, aig_man1);

    //Cnf_DataPrint1( pCnf, 0);
    

    Aig_ManForEachCo( pCnf->pMan, pObj, i1 ){

      int a[1];
      a[0]= 2*(pCnf->pVarNums[pObj->Id]);
      //cout<<"f"<<a[0]<<endl;
      sat_solver_addclause(pSat, a , a+1);            

    }

    Cnf_DataLift( pCnf, num_vars );
    
    //Cnf_DataPrint1( pCnf, 0);
    //add f(x),not f(x'),not f(x'') clause

    Aig_ManForEachCo( pCnf->pMan, pObj, i1 ){

      int a[1];
      a[0]= 2*(pCnf->pVarNums[pObj->Id])+1;
      //cout<<"not f' "<<a[0]<<endl;
      sat_solver_addclause(pSat, a , a+1);            

    }


    Cnf_CnfForClause( pCnf, pBeg, pEnd, i1 ){

       sat_solver_addclause(pSat, pBeg , pEnd);
    }

    //Cnf_DataPrint1( pCnf, 0);

    Cnf_DataLift( pCnf, num_vars );
    
    //Cnf_DataPrint1( pCnf, 0);

    Aig_ManForEachCo( pCnf->pMan, pObj, i1 ){

      int a[1];
      a[0]= 2*(pCnf->pVarNums[pObj->Id])+1;
      //cout<<"not f'' "<<a[0]<<endl;
      sat_solver_addclause(pSat, a , a+1);            

    }
    Cnf_CnfForClause( pCnf, pBeg, pEnd, i1 ){

       sat_solver_addclause(pSat, pBeg , pEnd);
    }

    //Cnf_DataPrint1( pCnf, 0);

    //add clause
    

    //Cnf_Dat_t * pCnf1;
    //pCnf1=Cnf_Derive( aig_man1,  Aig_ManCoNum(aig_man1) );

    //cout<<"pi_num:"<<p->nTruePis<<endl;
    Aig_ManForEachCi( pCnf->pMan, pObj, i1 ){

      //cout<<"for each ci"<<endl;
      //control variable a
      int temp_clause[3];
      temp_var=sat_solver_addvar(pSat);
      //cout<<"temp_var:"<<temp_var<<endl;
      //add 2 clause(note that the pVarNum is x'')
      //cout<<"var_num"<<i1<<":"<<pCnf->pVarNums[pObj->Id]<<endl;
      temp_clause[0] = 2*(pCnf->pVarNums[pObj->Id] - num_vars*2)+1;//not x
      temp_clause[1] = 2*(pCnf->pVarNums[pObj->Id] - num_vars );//x'   
      temp_clause[2] = 2*temp_var;//a

      //cout<<temp_clause[0]<<" "<<temp_clause[1]<<" "<<temp_clause[2]<<endl;

      sat_solver_addclause(pSat, temp_clause , temp_clause+3);


      temp_clause[0] = 2*(pCnf->pVarNums[pObj->Id]- num_vars*2);//x
      temp_clause[1] = 2*(pCnf->pVarNums[pObj->Id] - num_vars )+1;// not x'   
      temp_clause[2] = 2*temp_var;//b

      //cout<<temp_clause[0]<<" "<<temp_clause[1]<<" "<<temp_clause[2]<<endl;
   
      sat_solver_addclause(pSat, temp_clause , temp_clause+3);

      //control variable b

      temp_var=sat_solver_addvar(pSat);
      //cout<<"temp_var:"<<temp_var<<endl;

      temp_clause[0] = 2*(pCnf->pVarNums[pObj->Id] - num_vars*2)+1;//not x
      temp_clause[1] = 2*(pCnf->pVarNums[pObj->Id] );//x''   
      temp_clause[2] = 2*temp_var;//a
     
      //cout<<temp_clause[0]<<" "<<temp_clause[1]<<" "<<temp_clause[2]<<endl;
   
      sat_solver_addclause(pSat, temp_clause , temp_clause+3);


      temp_clause[0] = 2*(pCnf->pVarNums[pObj->Id]- num_vars*2);//x
      temp_clause[1] = 2*(pCnf->pVarNums[pObj->Id] )+1;// not x''  
      temp_clause[2] = 2*temp_var;//b
      sat_solver_addclause(pSat, temp_clause , temp_clause+3); 

      
      //cout<<temp_clause[0]<<" "<<temp_clause[1]<<" "<<temp_clause[2]<<endl;
     

    }
   

    //set assumption
    int assumpList[100000];
    int j=0;
    int k=0;
    int start = num_vars*3;
    int  * pp;
    //control variables start from 
    
   

    for(i1=0;i1<  Aig_ManCiNum( aig_man1 ) ;i1++){

      for(j=i1+1;j<  Aig_ManCiNum( aig_man1 ) ;j++){


        for(k=0;k<  Aig_ManCiNum( aig_man1 ) ;k++){
          
          //cout<<"k:"<<k<<endl;

          if(k==i1){
            assumpList[k*2] = 2*(start+2*k);//a=1
            assumpList[k*2+1] = 2*(start+2*k+1)+1;//b=0

          }
          else if(k==j){
            assumpList[k*2] = 2*(start+2*k)+1;//a=0
            assumpList[k*2+1] = 2*(start+2*k+1);//b=1
          }
          else{
            assumpList[k*2] = 2*(start+2*k)+1;//a=0
            assumpList[k*2+1] = 2*(start+2*k+1)+1;//b=0
          }

        }
        status = sat_solver_solve( pSat, assumpList, assumpList + 2 * aig_man1->nTruePis , 0, 0, 0, 0 );
        if(status == -1 ){
          //k=sat_solver_final(pSat, &pp);


          int counter;
          cout<<"PO "<<Abc_ObjName(pObj_po)<<" support partition: "<<1<<endl;
          for(counter=0;counter< Aig_ManCiNum( aig_man1 ) ;counter++){
            if(counter==i1)cout<<2;
            else if(counter==j)cout<<1;
            else cout<<0;
          }
          cout<<endl;
          break;
       
        }       
   

      }

      if(status == -1 )break;

    }

    if(status != -1 ){
      cout<<"PO "<<Abc_ObjName(pObj_po)<<" support partition: "<<0<<endl;  

    }
    else if(Aig_ManCiNum( aig_man1 )==0){
      cout<<"PO "<<Abc_ObjName(pObj_po)<<" support partition: "<<0<<endl; 
    }
    


  }

  /*
  Aig_Man_t * aig_man1;
  aig_man1 = Abc_NtkToDar( pNtk, 0, 0 );
  Cnf_Dat_t * pCnf;
  pCnf=Cnf_Derive( aig_man1, 0);

  sat_solver * pSat = (sat_solver *)Cnf_DataWriteIntoSolver(pCnf, 1, 0);
  int status;
  lit c[2]={5,6};
  lit c1[2]={4,7};
  
  sat_solver_addclause(pSat, c , c+2);
  //sat_solver_addclause(pSat, c1 , c1+2);
  Cnf_DataPrint1( pCnf, 0);

  status = sat_solver_solve( pSat, c1, c1+1, 0, 0, 0, 0 );

  if ( status == l_False )
    {cout<<"False"<<endl;}
  else if( status == l_True )
    {cout<<"True"<<endl;}

  int  * pp;
  int k;

  int * pLit, * pStop, i;

  k=sat_solver_final(pSat, &pp);


  for ( i = 0; i < k; i++ )
  {

    fprintf( stdout, "%d ", pp[i] );
   
  }

  fprintf( stdout, "\n" );

  status = sat_solver_solve( pSat, c, c+1, 0, 0, 0, 0 );


  k=sat_solver_final(pSat, &pp);


  for ( i = 0; i < k; i++ )
  {

    fprintf( stdout, "%d ", pp[i] );
   
  }

  fprintf( stdout, "\n" );


  if ( status == l_False )
    {cout<<"False"<<endl;}
  else if( status == l_True )
    {cout<<"True"<<endl;}
    
*/
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

