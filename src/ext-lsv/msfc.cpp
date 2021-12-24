#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include <map>
#include <vector>

using namespace std;

extern "C" {
	Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
	Cnf_Dat_t *	Cnf_Derive	(Aig_Man_t * pAig, int nOutputs);	
}

static int Lsv_CommandMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_Commandbidec(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "MSFC", "lsv_print_msfc", Lsv_CommandMSFC, 0);
  Cmd_CommandAdd(pAbc, "bidec", "lsv_or_bidec", Lsv_Commandbidec, 0);
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

int sat_solver_my_enable( sat_solver * pSat, int iVarA, int iVarB, int iVarEn)
{
    lit Lits[3];
    int Cid;
    assert( iVarA >= 0 && iVarB >= 0 && iVarEn >= 0 );

    Lits[0] = toLitCond( iVarA, 1);//not x
    Lits[1] = toLitCond( iVarB, 0);//x'
    Lits[2] = toLitCond( iVarEn, 0);//alpha
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );

    Lits[0] = toLitCond( iVarA, 0);//x
    Lits[1] = toLitCond( iVarB, 1);//not x'
    Lits[2] = toLitCond( iVarEn, 0);//alpha
    Cid = sat_solver_addclause( pSat, Lits, Lits + 3 );
    assert( Cid );
    return 2;
}

int Lsv_Commandbidec(Abc_Frame_t* pAbc, int argc, char** argv) {
	Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
	if (!pNtk) {
		Abc_Print(-1, "Empty network.\n");
		return 1;
	}
	else	{
		//Aig_Man_t*	aigckt	=	Abc_NtkToDar(pNtk, 0, 0);
		//Cnf_Dat_t*	CNF		=	Cnf_Derive(aigckt, Aig_ManCoNum(aigckt));
		Abc_Obj_t* pObj;
		Aig_Obj_t* pObj_aig;
		Abc_Obj_t* pFanin;
		int	i, j, k;
		
		vector	<Abc_Ntk_t*>	cone_array;//output cone
		vector	<int>	PO_id_array;//PO id
		vector	<bool>	PO_com_array;//whether PO is complement or not
		vector	<char*>	PO_name;//name of PO
		vector	<int>	pObj_com_0;
		vector	<int>	pFanin_com_0;
		vector	<int>	pObj_com_1;
		vector	<int>	pFanin_com_1;
		Abc_NtkForEachPo(pNtk, pObj, i ) {
			PO_name.push_back(Abc_ObjName(pObj));
			Abc_ObjForEachFanin(pObj, pFanin, j ) {
				//printf("i = %d\n", i);
				//printf("j = %d\n", j);
				//printf("fanout = %d\n", Abc_ObjFanoutNum(pFanin));
				//Abc_ObjXorFaninC(pObj, Abc_ObjIsComplement(pObj));//consider not output
				//if(Abc_ObjFaninC0(pObj)!=0)
				//	pFanin	=	Abc_ObjNot(pFanin);
				Abc_Ntk_t*	new_cone	=	Abc_NtkCreateCone(pNtk, pFanin, Abc_ObjName(pFanin), 0);
				//if(Abc_ObjFaninC0(pObj)!=0)
				//	pFanin	=	Abc_ObjNot(pFanin);
				if(Abc_ObjFaninC0(pObj))
					Abc_ObjXorFaninC(Abc_NtkPo(new_cone, 0), 0);//consider not output
				pObj_com_0.push_back(Abc_ObjIsComplement(pObj));
				pFanin_com_0.push_back(Abc_ObjFaninC0(pFanin));
				pObj_com_1.push_back(Abc_ObjFaninC1(pObj));
				pFanin_com_1.push_back(Abc_ObjFaninC1(pFanin));
				cone_array.push_back(new_cone);
				//PO_id_array.push_back(Abc_ObjId(pFanin));
			}
		}
		vector	<Aig_Man_t*>	aig_array;//aig array
		for(i=0;i<cone_array.size();i++) {
			aig_array.push_back(Abc_NtkToDar(cone_array[i], 0, 1));
			Aig_ManForEachObj(aig_array[i], pObj_aig, j) {
				if(Aig_ObjIsCo(pObj_aig)) {
					PO_id_array.push_back(Aig_ObjId(pObj_aig));
					bool	iscomplement	=	false;
					if(Aig_IsComplement(pObj_aig))
						iscomplement	=	true;
					PO_com_array.push_back(iscomplement);
				}
				//printf("i=%d j=%d\n", i,j);
			}
		}
		vector	<Cnf_Dat_t*>	cnf_array;//cnf formula array
		for(i=0;i<aig_array.size();i++)
			cnf_array.push_back(Cnf_Derive(aig_array[i], 1));
		//for(i=0;i<aig_array.size();i++)
		//	printf("Output size is %d\n", Aig_ManCoNum(aig_array[i]));
		for(i=0;i<cnf_array.size();i++) {
		//for(i=0;i<1;i++) {
			int*	pBeg;
			int*	pEnd;
			int		Cid;
			//add constraint to PI
			//construct PI array
			vector	<int>	PI_var_array;//record variable of PI
			Aig_ManForEachCi(aig_array[i], pObj_aig, j) {
				//add constant 1
				if (true) {
					int	aig_id	=	Aig_ObjId(pObj_aig);
					PI_var_array.push_back(cnf_array[i]->pVarNums[aig_id]);
				}
			}
			int	input_size	=	PI_var_array.size();
			/*
			//construct constant-1 array
			vector	<int>	constant_array;//record variable of constant
			Aig_ManForEachObj(aig_array[i], pObj_aig, j) {
				if (Aig_ObjIsConst1(pObj_aig)) {
					int	aig_id	=	Aig_ObjId(pObj_aig);
					constant_array.push_back(cnf_array[i]->pVarNums[aig_id]);
				}
			}
			*/
			//printf("# PI IS %d\n", input_size);
			
			//printf("i = %d\n", i);
			sat_solver* solver	=	sat_solver_new();
			//printf("# of clause initial %d\n", sat_solver_nclauses(solver));
			//int	num_of_var	=	cnf_array[i]->nVars;			
			//sat_solver_setnvars(solver, 5*num_of_var);
			//add PO=1 to CNF for f(x)
			int	PO_id	=	cnf_array[i]->pVarNums[PO_id_array[i]];
			//printf("1st PO ID = %d\n", PO_id);
			lit	out_clause_fx[1];
			//if(!PO_com_array[i])
			out_clause_fx[0]	=	toLitCond(PO_id, 0);
			//else
			//	out_clause_fx[0]	=	toLitCond(PO_id, 1);
			//solver	=	(sat_solver*)Cnf_DataWriteIntoSolver(cnf_array[i], 1, 0);
			//int	num_of_var	=	solver->size;
			Cnf_CnfForClause(cnf_array[i], pBeg, pEnd, j) {		
				sat_solver_addclause(solver, pBeg, pEnd);
			}
			Cid	=	sat_solver_addclause(solver, out_clause_fx, out_clause_fx+1);//f(x)=1
			//assert(Cid);
			//printf("# of var after f(x) is %d\n", sat_solver_nvars(solver));
			//printf("# of clause after f(x) %d\n", sat_solver_nclauses(solver));
			int	num_of_var	=	solver->size;
			//printf("# of var is %d\n", num_of_var);
			//create a fresh copy of f(x) as f(x')
			Cnf_DataLift(cnf_array[i], num_of_var);
			//write every clause into the solver
			//solver	=	(sat_solver*)Cnf_DataWriteIntoSolver(cnf_array[i], 1, 0);
			Cnf_CnfForClause(cnf_array[i], pBeg, pEnd, j) {		
				sat_solver_addclause(solver, pBeg, pEnd);
			}
			//write (f(x'))' into solver
			PO_id	+=	num_of_var;
			//printf("2nd PO ID = %d\n", PO_id);
			lit	out_clause_fxx[1];
			//if(!PO_com_array[i])
			out_clause_fxx[0]	=	toLitCond(PO_id, 1);
			//else
			//out_clause_fxx[0]	=	toLitCond(PO_id, 0);
			//printf("PO ID is %d\n", PO_id);
			Cid	=	sat_solver_addclause(solver, out_clause_fxx, out_clause_fxx+1);//f(x')=0
			//assert(Cid);
			//printf("# of var after f(x') is %d\n", sat_solver_nvars(solver));
			//printf("# of clause after f(x') %d\n", sat_solver_nclauses(solver));
			
			
			//create a fresh copy of f(x') as f(x'')
			//sat_solver_setnvars(solver, 3*num_of_var);
			Cnf_DataLift(cnf_array[i], num_of_var);
			//write every clause into the solver
			//solver	=	(sat_solver*)Cnf_DataWriteIntoSolver(cnf_array[i], 1, 0);
			Cnf_CnfForClause(cnf_array[i], pBeg, pEnd, j) {		
				sat_solver_addclause(solver, pBeg, pEnd);
			}
			//write (f(x''))' into solver
			PO_id	+=	num_of_var;
			//printf("3rd PO ID = %d\n", PO_id);
			lit out_clause_fxxx[1];
			//if(!PO_com_array[i])
			out_clause_fxxx[0]	=	toLitCond(PO_id, 1);
			//else
			//	out_clause_fxxx[0]	=	toLitCond(PO_id, 0);
			Cid	=	sat_solver_addclause(solver, out_clause_fxxx, out_clause_fxxx+1);//f(x'')=0
			//printf("# of clause after f(x'') %d\n", sat_solver_nclauses(solver));
			
			//create for alpha 1 - alpha n
			//iterate variable x1-xn
			//printf("# of clause before alpha %d\n", sat_solver_nclauses(solver));
			for(j=0;j<input_size;j++) {
				//sat_solver_addvar(solver);//for alpha
				sat_solver_my_enable(solver, PI_var_array[j], PI_var_array[j]+num_of_var,j+3*num_of_var);//add clause for alpha
			}			
			//printf("# of var after alpha is %d\n", sat_solver_nvars(solver));
			for(j=0;j<input_size;j++) {
				//sat_solver_addvar(solver);//for beta
				sat_solver_my_enable(solver, PI_var_array[j], PI_var_array[j]+2*num_of_var, j+input_size+3*num_of_var);//add clause for beta
			}
			/*
			//constant condition
			for(j=0;j<constant_array.size();j++) {
				sat_solver_my_enable(solver, constant_array[j], constant_array[j]+num_of_var, j+2*input_size+3*num_of_var);//add clause for alpha constant
			}
			for(j=0;j<constant_array.size();j++) {
				sat_solver_my_enable(solver, constant_array[j], constant_array[j]+2*num_of_var, j+2*input_size+3*num_of_var+constant_array.size());//add clause for beta constant
			}
			*/
			//printf("# of var after beta is %d\n", sat_solver_nvars(solver));
			//printf("# of clause after beta %d\n", sat_solver_nclauses(solver));
			lit	assumplist[2*input_size];
			//lit	assumplist	=	new	lit(2*num_of_var);
			for(j=0;j<(2*input_size);j++) {
				assumplist[j]	=	toLitCond(3*num_of_var+j, 1);//all alpha and beta are reset to 0
			}
			int clause_num	=	0;
			int*	assump_result;
			bool	unsat	=	false;
			int	alpha_var	=	0;
			int	beta_var	=	0;
			int	result	=	0;//solver result 
			//seed variable for n(n-1)/2 cases
			//printf("size of assump list %d\n", sizeof(assumplist));
			for(j=0;j<input_size-1;j++) {
				for(k=j+1;k<input_size;k++) {
					alpha_var	=	j;
					beta_var	=	k;
					//printf("alpha = %d\n", j);
					//printf("beta = %d\n", input_size+k);
					assumplist[j]	=	toLitCond(3*num_of_var+j, 0);//alpha j=1
					//assumplist[j]--;
					assumplist[input_size+k]	=	toLitCond(3*num_of_var+k+input_size, 0);//beta k=1
					//assumplist[input_size+k]--;
					result	=	sat_solver_solve(solver, assumplist, assumplist+(2*input_size), 0, 0, 0, 0);
					if(result==l_Undef)
						printf("iLLEgal\n");
					if(result==l_False) {//find a valid variable partition !
						unsat	=	true;
						clause_num	=	sat_solver_final((sat_solver*) solver, &assump_result);
						break;
					}
					//assumplist[j]++;
					//assumplist[input_size+k]++;
					assumplist[j]	=	toLitCond(3*num_of_var+j, 1);//alpha j=1
					//assumplist[j]--;
					assumplist[input_size+k]	=	toLitCond(3*num_of_var+k+input_size, 1);//beta k=1
				}
				if(unsat)
					break;
			}
			//printf("# of var after solving is %d\n", sat_solver_nvars(solver));
			//record the alpha and beta for each variable
			vector	<int>	alpha_array;//reset to 1 for each element
			vector	<int>	beta_array;
			//printf("seed A = %d\n", j);
			//printf("seed B = %d\n", k);
			for(j=0;j<input_size;j++) {
				if(j==beta_var)
					alpha_array.push_back(0);
				else
					alpha_array.push_back(1);
				if(j==alpha_var)
					beta_array.push_back(0);
				else
					beta_array.push_back(1);
			}
		
			//if(pFanin_com_0[i]!=0)
			//	printf("Need modify\n");
			//printf("pFanin_com_0 = %d\n", pFanin_com_0[i]);
			//printf("pFanin_com_1 = %d\n", pFanin_com_1[i]);
			//printf("pObj_com_0 = %d\n", pObj_com_0[i]);
			//printf("pObj_com_1 = %d\n", pObj_com_1[i]);
			//write partition result if unsat
			vector	<int>	partition_array;//0=c, 1=b, 2=a
			//reset every 
			if(!unsat)
				printf("PO %s support partition: 0\n", PO_name[i]);
			else {
				//set alpha, beta for each variable
				//printf("clause num = %d\n", clause_num);
				//int	size_array	=	sizeof(assump_result)/sizeof(assump_result[0]);
				//printf("size of clause is %d\n", size_array);
				for(j=0;j<clause_num;j++) {
					//printf("j= %d\n", j);
					if(assump_result[j]>=2*3*num_of_var && assump_result[j]<2*3*num_of_var+2*input_size) {//alpha i =0
						int	var_id	=	assump_result[j]-(2*3*num_of_var);
						var_id	=	var_id/2;
						//printf("A Illegal= %d\n", var_id);
						alpha_array[var_id]	=	0;
					}
					if(assump_result[j]>=2*3*num_of_var+2*input_size && assump_result[j]<2*3*num_of_var+2*2*input_size) {//beta i =0
						int	var_id	=	assump_result[j]-(2*3*num_of_var+2*input_size);
						var_id	=	var_id/2;
						//printf("B Illegal= %d\n", var_id);
						beta_array[var_id]	=	0;
					}
				}
				//assign partition for each variable
				printf("PO %s support partition: 1\n", PO_name[i]);
				int	num_of_A	=	0;	//number of var in A
				int	num_of_B	=	0;	//number of var in B
				for(j=0;j<input_size;j++) {
					if(alpha_array[j]==0 && beta_array[j]==0)	//C
						printf("0");
					else if(alpha_array[j]==0 && beta_array[j]==1) {//B
						num_of_B++;
						printf("1");
					}
					else if(alpha_array[j]==1 && beta_array[j]==0) {//A
						num_of_A++;
						printf("2");
					}
					else {	//either A or B depends on balance
						if(num_of_A>=num_of_B) {
							printf("1");
							num_of_B++;
						}
						else {
							printf("2");
							num_of_A++;
						}
					}
				}
				printf("\n");
			}
		}
		return 0;
	}	
}
