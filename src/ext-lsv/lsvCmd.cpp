#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include "opt/dar/darInt.h"
#include "aig/aig/aig.h"
#include "misc/vec/vec.h"
#include "vector"
#include "set"
#include "queue"
#include <iostream>
//debug//
extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t *, int, int);
using namespace::std;

class obj_comparator
{
public:
    bool operator() (Abc_Obj_t* a, Abc_Obj_t* b) const;

};

class obj_set_comparator
{
public:
    bool operator() (set<Abc_Obj_t*, obj_comparator>* a, set<Abc_Obj_t*, obj_comparator>* b) const;
};
bool obj_comparator::operator() (Abc_Obj_t* a, Abc_Obj_t* b) const
{
    return (Abc_ObjId(a) <= Abc_ObjId(b));
}
bool obj_set_comparator::operator() (set<Abc_Obj_t*, obj_comparator>* a, set<Abc_Obj_t*, obj_comparator>* b) const
{
    return (Abc_ObjId((*a->begin())) <= Abc_ObjId((*b->begin())));
}

static int Lsv_CommandMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandORBIDEC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);


void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "MSFC", "lsv_print_msfc", Lsv_CommandMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "BIDEC", "lsv_or_bidec", Lsv_CommandORBIDEC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_ORBIDEC(Abc_Ntk_t* pNtk) {
  int k = 0;
  int l = 0;
  int q = 0;
  int s = 0;
  int* begin;
  int* end;
  int po_num;
  int po_num_2;
  int po_num_3;
  int a;
  int b;
  int var_num;
  Abc_Obj_t* pPo;
  Abc_Obj_t* pfanin;
  Aig_Obj_t* entry;
  Abc_Ntk_t* pCone;
  vector<vector<int>> ab_record;
  pNtk = Abc_NtkStrash(pNtk, 1, 1, 0);
  sat_solver* sat;
  Abc_NtkForEachPo( pNtk, pPo, k )
  {   pfanin  = Abc_ObjFanin0(pPo);
	  pCone = Abc_NtkCreateCone(pNtk, pfanin, Abc_ObjName(pPo),0);
	  if(Abc_ObjFaninC0(pPo))
	  {
		  Abc_ObjXorFaninC(Abc_NtkPo(pCone,0),0);
	  }
	  pCone = Abc_NtkStrash(pCone, 1, 1, 0);
	  Aig_Man_t* t_aig = Abc_NtkToDar(pCone,0,0);
	  Cnf_Dat_t* t_cnf = Cnf_Derive(t_aig,1);
	  var_num = t_cnf->nVars;
	  po_num = *(t_cnf->pVarNums + Aig_ObjId(Aig_ManCo(t_aig,0)));
	  sat = (sat_solver *) Cnf_DataWriteIntoSolver(t_cnf, 1, 0);
	  lit lit_ar1[1] = {toLitCond(po_num,0)};
	  sat_solver_addclause(sat, lit_ar1, lit_ar1+1);
	  Cnf_DataLift(t_cnf, var_num);
	  Cnf_CnfForClause( t_cnf, begin, end,  q)
	  {
		  sat_solver_addclause(sat, begin, end);
	  }
	  lit lit_ar2[1] = {toLitCond(po_num+var_num,1)};
	  sat_solver_addclause(sat, lit_ar2, lit_ar2+1);
	  Cnf_DataLift(t_cnf, var_num);
	  Cnf_CnfForClause( t_cnf, begin, end,  s)
	  {
		  sat_solver_addclause(sat, begin, end);
	  }
	  lit lit_ar3[1] = {toLitCond(po_num+2*var_num,1)};
	  sat_solver_addclause(sat, lit_ar3, lit_ar3+1);
	  int ab_start = 3*t_cnf->nVars;
	  Aig_ManForEachCi(t_cnf->pMan, entry, l) //Vec_IntForEachEntry((Vec_Int_t*)pi->pArray[0], w,  z)
	  {
		  po_num_3 = *(t_cnf->pVarNums + Aig_ObjId(entry));//*(t_cnf->pVarNums + w);
		  po_num_2 = po_num_3 - var_num;
		  po_num = po_num_3 - 2*var_num;
		  a = sat_solver_addvar(sat);
		  b = sat_solver_addvar(sat);
		  vector<int> temp;
		  temp.push_back(a);
		  temp.push_back(b);
		  ab_record.push_back(temp);
		  
		  lit a_array[3] = {toLitCond(po_num,1), toLitCond(po_num_2,0), toLitCond(a,0)};
		  lit a_array_2[3] = {toLitCond(po_num,0), toLitCond(po_num_2,1), toLitCond(a,0)};
		  lit b_array[3] = {toLitCond(po_num,1), toLitCond(po_num_3,0), toLitCond(b,0)};
		  lit b_array_2[3] = {toLitCond(po_num,0), toLitCond(po_num_3,1), toLitCond(b,0)};
		  sat_solver_addclause(sat, a_array, a_array+3);
		  sat_solver_addclause(sat, a_array_2, a_array_2+3);
		  sat_solver_addclause(sat, b_array, b_array+3);
		  sat_solver_addclause(sat, b_array_2, b_array_2+3);
		  
	  }
	  bool solve = 0;
	  lit* lit_record = new lit [2*ab_record.size()];
	  for(int i=0; i<ab_record.size(); ++i)
	  {
		  lit_record[2*i]   =  toLitCond(ab_record[i][0],1);
		  lit_record[2*i+1] =  toLitCond(ab_record[i][1],1);
	  }
	  for(int i=0; i<ab_record.size(); ++i)
	  {
		  if(i != 0)
		  {
			  lit_record[2*(i-1)] = toLitCond(ab_record[i-1][0],1);
			  lit_record[2*(i-1)+1] = toLitCond(ab_record[i-1][1],1);
		  }
		  lit_record[2*i] = toLitCond(ab_record[i][0],0);
		  lit_record[2*i+1] = toLitCond(ab_record[i][1],1);
		  for(int j=i+1; j<ab_record.size(); ++j)
		  {
			  if(j != i+1)
			  {
				  lit_record[2*(j-1)] = toLitCond(ab_record[j-1][0],1);
				  lit_record[2*(j-1)+1] = toLitCond(ab_record[j-1][1],1);
			  }
			  lit_record[2*j] = toLitCond(ab_record[j][0],1);
			  lit_record[2*j+1] = toLitCond(ab_record[j][1],0);


			  lbool result = (lbool)sat_solver_solve(sat, lit_record, lit_record+2*ab_record.size(), 0, 0, 0, 0);
			  assert(result != 0);
			  if(result == -1)
			  {
				  int* pparray;
				  int t_l_mod;
				  int t_l_que;
				  int final_clause_size = sat_solver_final(sat, &pparray);
				  for(int h=0; h<ab_record.size(); ++h)
				  {
					  ab_record[h][0] = 0;
					  ab_record[h][1] = 0;
				  }
				  for(int h=0; h<final_clause_size; ++h)
				  {
					  t_l_mod = (pparray[h]/2 - ab_start)%2;
					  t_l_que = (pparray[h]/2 - ab_start)/2;
					  ab_record[t_l_que][t_l_mod] = 1;
				  }
				  int* x_array = new int[ab_record.size()];
				  for(int h=0; h<ab_record.size(); ++h)
				  {
					  if( (ab_record[h][0] == 0) && (ab_record[h][1] == 0) )
					  {
						  x_array[h] = 0;
					  }
					  else if( (ab_record[h][0] == 1) && (ab_record[h][1] == 0) )
					  {
						  x_array[h] = 1;
					  }
					  else if( (ab_record[h][0] == 0) && (ab_record[h][1] == 1) )
					  {
						  x_array[h] = 2;
					  }
					  else if( (ab_record[h][0] == 1) && (ab_record[h][1] == 1) )
					  {
						  x_array[h] = 0;
					  }
				  }
				  cout << "PO " << Abc_ObjName(pPo) << " support partition: 1" << endl;
				  for(int h=0; h<ab_record.size(); ++h)
				  {
					  cout << x_array[h];
				  }
				  cout << endl;
				  solve = 1;
				  delete x_array;
				  break;
			  }
		  }
		  if(solve == 1)
		  {
			  break;
		  }
	  }
	  if(!solve)
	  {
		  cout << "PO " << Abc_ObjName(pPo) << " support partition: 0" << endl;
	  }
	  delete lit_record;
	  ab_record.clear();
  }
}
void Lsv_NtkMSFC(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pPo;
	Abc_Obj_t* pFan;
    int i;
    queue<Abc_Obj_t*> root;
    set<set<Abc_Obj_t*, obj_comparator>*, obj_set_comparator> msfc_list;
    set<Abc_Obj_t*> traversed;
	/*
    Abc_NtkForEachObj( pNtk, pPo, i ) 
    {
		if(!Abc_ObjIsPi(pPo)&&!Abc_ObjIsPo(pPo))
		{
			if(((Abc_ObjFanoutNum(pPo) >1)||(Abc_ObjFanoutNum(pPo) <= 0))&&( Abc_ObjIsNode(pPo) ))
			{
				root.push(pPo);
				traversed.insert(pPo);
			}
			else if((Abc_ObjIsPo(Abc_ObjFanout0(pPo)))&&( Abc_ObjIsNode(pPo)))
			{				
				root.push(pPo);
				traversed.insert(pPo);
			}
		}
    }*/
	Abc_NtkForEachPo( pNtk, pPo, i )
	{
		int j;
		traversed.insert(pPo);
		Abc_ObjForEachFanin(pPo, pFan, j)
		if((traversed.find(pFan) == traversed.end())&&(!Abc_ObjIsPi(pFan)))
		{
			root.push(pFan);
			traversed.insert(pFan);
		}
	} 
	while(root.size() > 0)
	{
		Abc_Obj_t* pFanin;
		int k;
		queue<Abc_Obj_t*> track;
		track.push(root.front());
		set<Abc_Obj_t*, obj_comparator>* track_list = new set<Abc_Obj_t*, obj_comparator>;
		track_list->insert(root.front());
		while(track.size() > 0)
		{
			Abc_Obj_t* current_node = track.front();
			track.pop();
			Abc_ObjForEachFanin(current_node, pFanin, k)
			{
				if((traversed.find(pFanin)==traversed.end()))
				{
					traversed.insert(pFanin);
					if(!Abc_ObjIsPi(pFanin))
					{
						if((Abc_ObjFanoutNum(pFanin) == 1))
						{
							track_list->insert(pFanin);
							track.push(pFanin);
						}
						else
						{
							root.push(pFanin);
						}
					}
				}
			}
		}
		msfc_list.insert(track_list);
		root.pop();
	}
	int counter = 0;
	for(auto x=msfc_list.begin(); x!=msfc_list.end(); ++x)
	{
		cout << "MSFC " << counter << ": ";
		int counter_2 = (*x)->size(); 
		for(auto y=(*x)->begin(); y!=(*x)->end(); ++y)
		{
			if(counter_2 != 1)
			{
				cout << Abc_ObjName((*y)) << ",";
			}
			else
			{
				cout << Abc_ObjName((*y));
			}
			counter_2--;
		}
		cout << endl;
		counter++;
	}
	for(auto x=msfc_list.begin(); x!=msfc_list.end(); ++x)
	{
		delete (*x);
	}
	

}
int Lsv_CommandORBIDEC(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_ORBIDEC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;	
}
int Lsv_CommandMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Lsv_NtkMSFC(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
  Abc_Print(-2, "\t        prints the nodes in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
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