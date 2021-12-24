#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "sat/cnf/cnf.h"
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
using namespace std;

extern "C" Aig_Man_t*  Abc_NtkToDar(Abc_Ntk_t*, int, int);


static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_MSFC(Abc_Frame_t* pAbc, int argc, char** argv);

static int Lsv_bidec(Abc_Frame_t* pAbc, int argc, char** argv);

set<int> get_MFSC(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, set<int> s);



void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_MSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_bidec, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;


void Lsv_NtkBidec(Abc_Ntk_t* pNtk)
{
	
	Abc_Obj_t* pObj;
	int i;
	Abc_NtkForEachPo(pNtk, pObj, i)
	{
		Abc_Ntk_t*  cone;
		cone = Abc_NtkCreateCone( pNtk, Abc_ObjFanin0(pObj), Abc_ObjName(pObj), 0 );
		//if ( Abc_ObjFaninC0(pObj) )
		//	Abc_ObjXorFaninC( Abc_NtkPo(cone, 0), 0 );
		//Abc_Print(-1, "checkpoint1\n");
		Aig_Man_t * pAig;
		pAig = Abc_NtkToDar( cone, 0, 1 );
		//Abc_Print(-1, "checkpoint2\n");
		Cnf_Dat_t * pCnf; 
		pCnf = Cnf_Derive( pAig, Aig_ManCoNum(pAig) );
		//cout << "c3"<<endl;
		int shift = pCnf -> nVars;
		//cout << shift << endl;
		
		sat_solver * pSat = (sat_solver *)Cnf_DataWriteIntoSolver( pCnf, 1, 0 );
		//cout << "c4" <<endl;
		sat_solver_setnvars(pSat, shift*3);	
		
		
		Aig_Obj_t* Po;
		int PO_CNF;
		int z;
		Aig_ManForEachCo(pAig, Po, z)
		{
			PO_CNF = pCnf->pVarNums[Po->Id];
			//cout<<z<<endl;
		}

		Aig_Obj_t* pObj2;
		int i2;
		Aig_ManForEachCi(pAig, pObj2, i2)
		{
			int PI_CNF = pCnf->pVarNums[pObj2->Id];
			int alpha = sat_solver_addvar(pSat);//*shift + i2;
			int beta = sat_solver_addvar(pSat);//4*shift + i2;
			//if (i == 0)
			//cout <<shift<<" "<<alpha<<endl;
			//cout << PI_CNF<<endl;
			sat_solver_add_buffer_enable(pSat, PI_CNF, PI_CNF + shift, alpha, 0);
			//cout << sat_solver_nvars(pSat)<<" "<< sat_solver_nclauses(pSat)<<endl;
			sat_solver_add_buffer_enable(pSat, PI_CNF, PI_CNF + 2*shift, beta, 0);
			//cout << sat_solver_nvars(pSat)<<" "<< sat_solver_nclauses(pSat)<<endl;
			//cout << i2 <<endl;
		}
		//cout << i2<<endl;
		//++i2;
		lit Lits[1];
		Lits[0] = toLitCond( PO_CNF, 0 );
		sat_solver_addclause(pSat, Lits, Lits+1);		

		for (int k=0; k<2; k++)
		{
			Cnf_DataLift( pCnf, shift);
			Lits[0] = toLitCond( PO_CNF + (k+1)*shift, 1 );
			for ( int m = 0; m < pCnf->nClauses; m++ )
			{
				sat_solver_addclause( pSat, pCnf->pClauses[m], pCnf->pClauses[m+1] ) ;
				//cout << sat_solver_nvars(pSat)<<" "<< sat_solver_nclauses(pSat)<<endl;
			}
			sat_solver_addclause(pSat, Lits, Lits+1);
			//cout << sat_solver_nvars(pSat)<<" "<< sat_solver_nclauses(pSat)<<endl;
		}

		int state;

		lit assumption[2*i2];
		for (int k=0; k<i2-1; k++)
		{
			for(int m = k+1; m<i2;m++)
			{
				for(int n = 0; n<i2*2;n++)
				{
					assumption[n] = toLitCond(shift*3+n,1);
				}
				assumption[2*k] = toLitCond(shift*3+2*k,0);
				
				assumption[2*m+1]= toLitCond(shift*3+2*m+1,0);
				//cout<<shift<<endl;
				//for(int n = 0; n<i2*2;n++)
				//{
	   			//	cout<<assumption[n] <<endl;//= toLitCond(shift*3+n,1)
				//	}
				//SAT
				state = sat_solver_solve(pSat, assumption, assumption+2*i2,0,0,0,0);
				//cout << state << endl;
				if(state == l_False)
					break;
			}
			if(state == l_False)
				break;
		}
		if(state == l_False)
		{
			cout<<"PO "<<Abc_ObjName(pObj)<<" support partition: 1\n";
		
			int* conflict_clause;
			int num;	
			num = sat_solver_final(pSat, &conflict_clause);
			//cout << conflict_clause <<" "<<num<<endl;
			int a_v[i2], b_v[i2];
			for (int j=0; j<i2;j++)
			{
				a_v[j]=0;
				b_v[j]=0;
			}
			for (int j=0; j<num;j++)
			{
				int q = conflict_clause[j]/2-3*shift;
				if(q%2==0)
					a_v[i/2]=0;
				else
					b_v[i/2]=0;
			}
			for (int j=0; j<i2;j++)
			{
				if(a_v[j]==0)
				{
					if(b_v[j]==0)
						cout<<0;
					else
						cout<<1;
				}
				else
					cout<<2;
			}
			cout << endl;
		}
		else
			cout<<"PO "<<Abc_ObjName(pObj)<<" support partition: 0\n";
	}


}

int Lsv_bidec(Abc_Frame_t* pAbc, int argc, char** argv) {
  
  //Abc_Print(-1,"Hello\n");
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
  Lsv_NtkBidec(pNtk);
  return 0;

usage:
  Abc_Print(-2, "usage: lsv_or_bidec [-h]\n");
  Abc_Print(-2, "\t        show if each node can be or bidec\n");
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

void Lsv_NtkMSFC(Abc_Ntk_t* pNtk)
{
	Abc_Obj_t* pObj;
	int i;
	vector< set<int> > MFSC;
	Abc_NtkForEachObj(pNtk, pObj, i)
	{

		//printf("Obj Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
		bool root = false;
		//cout << Abc_ObjFanoutNum(pObj) <<endl;
		if(Abc_ObjFanoutNum(pObj)==1)
		{
			int y;
			y = Abc_ObjFanout(pObj,0) -> Type;
			//cout << y <<endl;
			if( y!=1 && y!=7 )
			{
				root = true;
			}
		}
		else if(Abc_ObjFanoutNum(pObj)>1)
			root = true;

		int z;
		z = pObj -> Type;
		//cout << z <<endl;

		if(pObj -> Type !=1 && pObj -> Type != 7)
			root = false;
		set<int> s;
		if(root)
		
		{
			//printf("root\n");
			s.insert(Abc_ObjId(pObj));
			s = get_MFSC(pNtk, pObj,s);	
			MFSC.push_back(s);
		}

	}
	sort(MFSC.begin(), MFSC.end());
	for(int j=0; j<MFSC.size() ; j++)
	{
		printf("MSFC %d: ",j);
		for (auto it = MFSC[j].begin(); it!= MFSC[j].end(); it++)
		{
			if( it != MFSC[j].begin() )
				printf(",");
				//cout <<",";
			//cout<<"n"<< *it ;
			printf("n%d",*it);
		}
		printf("\n");
	}
}

set<int> get_MFSC(Abc_Ntk_t* pNtk, Abc_Obj_t* pObj, set<int> s)
{
	int i;
	Abc_Obj_t* pFi;
	Abc_ObjForEachFanin(pObj,pFi, i)
	{
		if(Abc_ObjFanoutNum(pFi) == 1)
		{
			int y = pFi -> Type;
			if(y==1 || y==7)
			{
				//printf("%d\n", Abc_ObjId(pFi));
				s.insert(Abc_ObjId(pFi));
				s = get_MFSC(pNtk, pFi, s);
		
			}
		}
	}
	return s;
}


int Lsv_MSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
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
  Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
  Abc_Print(-2, "\t        prints the msfc in the network\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}
