#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

#include "vector"
#include "set"
#include "queue"
#include <iostream>
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

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "MSFC", "lsv_print_msfc", Lsv_CommandMSFC, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

void Lsv_NtkMSFC(Abc_Ntk_t* pNtk) {
    Abc_Obj_t* pPo;
    int i;
    queue<Abc_Obj_t*> root;
    set<set<Abc_Obj_t*, obj_comparator>*, obj_set_comparator> msfc_list;
    set<Abc_Obj_t*> traversed;
    Abc_NtkForEachPo( pNtk, pPo, i )
    {
        Abc_Obj_t* pFanin;
        int j;
        Abc_ObjForEachFanin( pPo, pFanin, j )
        {
            if(traversed.find(pFanin)==traversed.end())
            {
				root.push(pFanin);
				traversed.insert(pFanin);
            }
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
				if(traversed.find(pFanin)==traversed.end())
				{
					traversed.insert(pFanin);
					if(!Abc_ObjIsPi(pFanin))
					{
						if(Abc_ObjFanoutNum(pFanin) > 1)
						{
							root.push(pFanin);
						}
						else
						{
							track_list->insert(pFanin);
							track.push(pFanin);
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
		for(auto y=(*x)->begin(); y!=(*x)->end(); ++y)
		{
			cout << Abc_ObjName((*y)) << ", ";
		}
		cout << endl;
		counter++;
	} 
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