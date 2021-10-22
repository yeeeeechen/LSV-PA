#include <iostream>
#include <algorithm>
#include <stack>
#include <vector>
#include <queue>
#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

using namespace std;
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintNodes, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct initializer {
  initializer() { Abc_FrameAddInitializer(&frame_initializer); }
} TEST;

bool compare_level(Abc_Obj_t* a,Abc_Obj_t* b){
  
  return (Abc_ObjLevel(a)>Abc_ObjLevel(b));

}

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  //std::cout<<"ABC command line: \"lsv_print_msfc\"."<<std::endl;
  Abc_Obj_t* pObj;
  int i;
  int new_group_num=0;
  std::vector<Abc_Obj_t*>v;

 
  int a[300000];
  for(int i=0;i<300000;i++)
    a[i]=0;

  int group_num= 1;

  
  Abc_NtkForEachPo(pNtk, pObj, i){
    v.push_back(pObj);
  }
 
  
  for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ ){   
    if ( (pObj) == NULL  ) {}
    else if(Abc_ObjIsNode(pObj)){
  //printf("Object level = %d, id = %d ,name=%s\n", Abc_ObjLevel( pObj ) , Abc_ObjId(pObj),Abc_ObjName(pObj));
      v.push_back(pObj);

    }
  }
  //sort
 
  std::sort (v.begin()+Abc_NtkPoNum(pNtk), v.end(),compare_level);

  for(int i=0 ; i<v.size() ; i++ ){
    

    if(Abc_ObjIsPo(v[i])){
      //std::cout<<"po name:"<<Abc_ObjName(v[i])<<" group num:"<<group_num<<std::endl;
      a[Abc_ObjId(v[i])]=group_num;
      group_num+=1;
    }
    else{

      
      if( Abc_ObjFanoutNum(v[i])>1){
        a[Abc_ObjId(v[i])]=group_num;
        group_num+=1;
      }
      else{
        a[Abc_ObjId(v[i])]=a[Abc_ObjId(Abc_ObjFanout(v[i], 0))];
      } 

    }

  }
  

  
  std::vector<std::queue<Abc_Obj_t*> >v1;
  //b maps original group num to sorted group num
  int b[300000];
  for(int i=0;i<300000;i++)
    b[i]=-1;


  for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
    if ( (pObj) == NULL || a[Abc_ObjId(pObj)]==0 || Abc_ObjIsPo(pObj)) {} 
    else{


      if(b[a[Abc_ObjId(pObj)]]==-1){
        std::queue<Abc_Obj_t*>q;
        q.push(pObj);
        v1.push_back(q);
        b[a[Abc_ObjId(pObj)]]=new_group_num;
        new_group_num++;
      }
      else{
        v1[b[a[Abc_ObjId(pObj)]]].push(pObj);
      }
      
      
    }
  
  int sig=0;
  if(Abc_ObjFanoutNum(Abc_NtkObj(pNtk, 0))>0){
    std::cout<<"MSFC "<< 0  <<":";
    std::cout<<Abc_ObjName(Abc_NtkObj(pNtk, 0));
    std::cout<<std::endl;
    sig=1;
  }
  for(int i=0;i<v1.size();i++){
    std::cout<<"MSFC "<< i+sig <<":";
    std::cout<<Abc_ObjName(v1[i].front());
    v1[i].pop();
    while(!v1[i].empty()){
      std::cout<<",";
      std::cout<< Abc_ObjName(v1[i].front());
      v1[i].pop();

    }
    std::cout<<std::endl;
  }
 
  //printf("Object Id = %d, name = %s,group_num=%d\n", Abc_ObjId(s.top()), Abc_ObjName(s.top()),group_num);
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
