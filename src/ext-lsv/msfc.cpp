#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <map>
#include <vector>

using namespace std;

static int Lsv_CommandMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "MSFC", "lsv_print_msfc", Lsv_CommandMSFC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct MSFC_register {
  MSFC_register() { Abc_FrameAddInitializer(&frame_initializer); }
} MSFCregister;

class disjointset {
  public:
    disjointset(int id) { value=id; parent=NULL; rank=0; }
    ~disjointset() {}
    disjointset* findset(disjointset* x);
    void unionset(disjointset* x, disjointset* y);
    void link(disjointset* x, disjointset* y);
    
    
    int  value; //id of the gate
    disjointset*  parent; //id of the parent
    int rank; 
};

disjointset* disjointset::findset(disjointset* x)
{
  if(x->parent==NULL)
    return x;
  else {
    x->parent = findset(x->parent);
    return x->parent;  
  }
}

void disjointset::unionset(disjointset* x, disjointset* y)
{
  if(findset(x)!=findset(y))
    link(findset(x),findset(y));
}

void disjointset::link(disjointset* x, disjointset* y)
{
  if(x->rank > y->rank) 
    y->parent = x;
  else {
    x->parent = y;
    if(x->rank == y->rank)
      y->rank++;
  }
}

//can track to fanout num = 0
void print_singlefanout (Abc_Obj_t* node_t) {
  printf("Object Id = %d, name = %s\n", Abc_ObjId(node_t), Abc_ObjName(node_t));
  printf("Fanout number = %d\n", Abc_ObjFanoutNum(node_t));
  if(Abc_ObjFanoutNum(node_t)==1)
    print_singlefanout(Abc_ObjFanout0(node_t));
}

//union the fanout of the gate until its limit
//1: Abc_ObjIsPo
//2: fanout gate number is >= 2
void union_fanout (Abc_Obj_t* node_t, map <int, disjointset*>& id2set) {
  int gateid = Abc_ObjId(node_t);
  int fanoutid = Abc_ObjId(Abc_ObjFanout0(node_t));
  id2set[gateid]->unionset(id2set[gateid],id2set[fanoutid]);
  if(Abc_ObjFanoutNum(Abc_ObjFanout0(node_t))==1 && Abc_ObjIsPo(Abc_ObjFanout0(Abc_ObjFanout0(node_t)))==0)
    union_fanout(Abc_ObjFanout0(node_t), id2set);
}

//develop MSFC algorithm here
int Lsv_CommandMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
  //recursive union the gate with only one fanout
  //set may be used
  Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
  if (!pNtk) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }
  else {
    Abc_Obj_t* pObj; //traverse gate
    int i;
    map <int, Abc_Obj_t*> id2gate;
    map <int, disjointset*> id2set;
    vector <disjointset*> setcontainer;
    Abc_NtkForEachObj(pNtk, pObj, i) {
      //try print out fanout number of each gates
      /*
      printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
      printf("Abc_AigNodeIsAnd %d\n", Abc_AigNodeIsAnd(pObj));
      printf("Abc_AigNodeIsConst %d\n", Abc_AigNodeIsConst(pObj));
      printf("Abc_AigNodeIsChoice %d\n", Abc_AigNodeIsChoice(pObj));
      
      printf("Fanin number = %d\n", Abc_ObjFaninNum(pObj));
      printf("Fanout number = %d\n", Abc_ObjFanoutNum(pObj));
      */
      //printf("Is output ? = %d \n", Abc_ObjIsPo(pObj));
      //printf("Is input ? = %d \n", Abc_ObjIsPi(pObj));
      
      //ignore input and output nodes
      //if(Abc_ObjIsPo(pObj)==0 && Abc_ObjIsPi(pObj)==0) {
      id2gate[Abc_ObjId(pObj)] = pObj;
      disjointset* newset = new disjointset(Abc_ObjId(pObj));
      setcontainer.push_back(newset);
      id2set[Abc_ObjId(pObj)] = newset;
      //}
      //printf("Is output ? = %d \n", Abc_ObjIsPi(pObj));
      /*
      Abc_Obj_t* pFanout;
      int j;
      Abc_ObjForEachFanout(pObj, pFanout, j) {
        printf("  Fanout-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanout),
               Abc_ObjName(pFanout));
      }
      */
      /*
      Abc_Obj_t* pFanin;
      int j;
      Abc_ObjForEachFanout(pObj, pFanin, j) {
        printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
               Abc_ObjName(pFanin));
      }
      */    
    }
    //if the gate has more than 1 fanout, terminate and make it self its own set
    //otherwise, recursive find the fanout gate untile it is:
    //1: the output node Abc_ObjIsPo
    //2: fanout gate number is >= 2
    //printf("start union\n");
    Abc_NtkForEachObj(pNtk, pObj, i) {
      //ignore input and output nodes
      if(Abc_ObjIsPo(pObj)==0 && Abc_ObjIsPi(pObj)==0 && Abc_ObjFanoutNum(pObj)>0) {
      
      //examine fanout number
      //note the case the only fanout gate is NOT PO to avoid bug when traversal
      if(Abc_ObjFanoutNum(pObj)==1 && Abc_ObjIsPo(Abc_ObjFanout0(pObj))==0) {
        union_fanout (pObj, id2set);
      }        
      }
    }
    //printf("end union\n");
    /*
    Abc_NtkForEachNode(pNtk, pObj, i) {
      //try print out fanout number of each gates
      //if(Abc_ObjIsPo(pObj)==0 && Abc_ObjIsPi(pObj)==0 && Abc_ObjFanoutNum(pObj)) {
        printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
        printf("Fanout number = %d\n", Abc_ObjFanoutNum(pObj));
        int gateid = Abc_ObjId(pObj);
        printf("Set = %d\n", id2set[gateid]->findset(id2set[gateid])->value);
      //}
    }
    */
    //traverse sequentially according to object ID can ignore sorting
    //print a set in each iteration
    int numofMSFC = 0;
    int numofnodes = Abc_NtkNodeNum(pNtk);
    //printf("There are %d nodes in the network\n", numofnodes);
    //int lastnodenum = numofnodes;
    //new algorithm for print
    //label each Obj in ONE iteration
    map <Abc_Obj_t*, int> node2group; //node to MSFC group
    map <int, int> id2group; //already MSFCed (parent) node id to MSFC group num
    map <int, bool> isgroup; //the group is a MSFC already (int is from value(id) of parent)
    map <int, bool>::iterator iter;
    
    Abc_NtkForEachObj(pNtk, pObj, i) {
      if(Abc_ObjIsPo(pObj)==0 && Abc_ObjIsPi(pObj)==0 && Abc_ObjFanoutNum(pObj)>0) {
        int currentid = Abc_ObjId(pObj);
        int MSFCid = id2set[currentid]->findset(id2set[currentid])->value; //parent id of the set
        //if a MSFC group hasn't appeared, add as new group MSFC++
        iter = isgroup.find(MSFCid);
        if(iter==isgroup.end()) {
          isgroup[MSFCid] = true;
          numofMSFC++;
          node2group[pObj] = numofMSFC;
          id2group[MSFCid] = numofMSFC;
        }
        else {
          node2group[pObj] =  id2group[id2set[currentid]->findset(id2set[currentid])->value];
        }
      }
    }
    //printf("Yes\n");
    //create 2d vector for print
    vector < vector<Abc_Obj_t*> > MSFCvec;
    MSFCvec.resize(numofMSFC);
    //create the print out table
    Abc_NtkForEachObj(pNtk, pObj, i) {
      if(Abc_ObjIsPo(pObj)==0 && Abc_ObjIsPi(pObj)==0 && Abc_ObjFanoutNum(pObj)>0) {
        int currentid = Abc_ObjId(pObj);
        int MSFCid = id2set[currentid]->findset(id2set[currentid])->value; //parent id of the set
        int MSFCvecid = id2group[MSFCid]-1;
        MSFCvec[MSFCvecid].push_back(pObj);
      }
    }
    //printf("Yes1\n");
    for (int j=0;j<numofMSFC;j++) {
      printf("MSFC %d: ", j);
      for(int k=0;k<MSFCvec[j].size();k++) {
        if(k==0)
          printf("%s", Abc_ObjName(MSFCvec[j][k]));
        else
          printf(",%s", Abc_ObjName(MSFCvec[j][k]));
      }
      printf("\n");
    }
    /*
    while (lastnodenum>0) {
      int printset = -1453;
      bool setdetermine = false;// whether the set to be printed out is determined
      Abc_NtkForEachNode(pNtk, pObj, i) {
        //print false node
        int currentid = Abc_ObjId(pObj);
        if(id2print[currentid]==false && setdetermine==false) {
          //print this set of nodes in this iteration
          printset = id2set[currentid]->findset(id2set[currentid])->value;
          setdetermine = true;
          printf("MSFC %d: %s", numofMSFC, Abc_ObjName(pObj));
          numofMSFC++;
          lastnodenum--;
          id2print[currentid]=true;
        }
        if(id2print[currentid]==false && setdetermine==true && id2set[currentid]->findset(id2set[currentid])->value==printset) {
          printf(",%s", Abc_ObjName(pObj));
          lastnodenum--;
          id2print[currentid]=true;
        }
      }
      printf("\n");
    }
    */
    return 0; 
  }
}