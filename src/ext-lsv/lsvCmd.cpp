#include "base/abc/abc.h"
#include "aig/aig/aig.h"
#include "sat/cnf/cnf.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include <iostream>
#include <algorithm>
#include <map>
#include <vector>
using namespace std;

static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandOrBidec(Abc_Frame_t* pAbc, int argc, char** argv);
static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);

void init(Abc_Frame_t* pAbc) {
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_or_bidec", Lsv_CommandOrBidec, 0);
  Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
}

void destroy(Abc_Frame_t* pAbc) {}

Abc_FrameInitializer_t frame_initializer = {init, destroy};

struct PackageRegistrationManager {
  PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
} lsvPackageRegistrationManager;

// ---------- lsv_or_bidec ----------
extern "C" {Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters);} // see line 233, abcDar.c 

void Lsv_NtkOrBidec(Abc_Ntk_t* pNtk){
  Abc_Obj_t* pPo;
  int po_id;
  Abc_NtkForEachPo(pNtk, pPo, po_id) { // For each PO, we need to know whether it's bi-decomposable
    // build the CNF of f(X) and f'(X)
    Abc_Ntk_t* pPoCone = Abc_NtkCreateCone(pNtk, Abc_ObjFanin0(pPo), Abc_ObjName(pPo), 0); // extract the cone of the PO and its support set. Last argument is fUseAllCis 
    if( Abc_ObjFaninC0(pPo) ) Abc_ObjXorFaninC( Abc_NtkPo(pPoCone, 0), 0 );  
    Aig_Man_t* pPoCone_aig = Abc_NtkToDar(pPoCone, 0, 0); // AIG of f(X) obtained
    Cnf_Dat_t* pPo_cnf = Cnf_Derive(pPoCone_aig, Aig_ManCoNum(pPoCone_aig)); // f(X) in CNF form
    int nCnfVar = pPo_cnf->nVars; // # of variables in f(X), f'(X) and f''(X)
    int nCi = Aig_ManCiNum(pPoCone_aig); // # of alphas and betas 

    // cout <<"PO cone"<<Abc_NtkName(pPoCone)<<"has "<<Abc_NtkPiNum(pPoCone)<<"PIs\n";

    sat_solver* pSat = (sat_solver*)Cnf_DataWriteIntoSolver(pPo_cnf, 1, 0); // CNF of f(X)
    sat_solver_setnvars(pSat, 3*nCnfVar+2*nCi);
    int Cid, i_out;
    lit lits[3]; 
    Aig_Obj_t * pVarOut;
    /*
    lits[0] = 2*(pPo_cnf->pVarNums[Aig_ManCo(pPoCone_aig, 0)->Id]);
    Cid = sat_solver_addclause( pSat, lits, lits+1);
    Cnf_DataLift(pPo_cnf, nCnfVar);
    Cnf_DataWriteIntoSolverInt(pSat, pPo_cnf, 1, 0); // CNF of neg f'(X)
    lits[0] = 2*(pPo_cnf->pVarNums[Aig_ManCo(pPoCone_aig, 0)->Id])+1;
    Cid = sat_solver_addclause( pSat, lits, lits+1);
    Cnf_DataLift(pPo_cnf, nCnfVar);
    Cnf_DataWriteIntoSolverInt(pSat, pPo_cnf, 1, 0); // CNF of neg f''(X)
    lits[0] = 2*(pPo_cnf->pVarNums[Aig_ManCo(pPoCone_aig, 0)->Id])+1;
    Cid = sat_solver_addclause( pSat, lits, lits+1);
    */
    
    Aig_ManForEachCo(pPoCone_aig, pVarOut, i_out){
      lits[0] = 2*(pPo_cnf->pVarNums[pVarOut->Id]);
      Cid = sat_solver_addclause( pSat, lits, lits+1);
    }
    Cnf_DataLift(pPo_cnf, nCnfVar);
    Cnf_DataWriteIntoSolverInt(pSat, pPo_cnf, 1, 0); // CNF of neg f'(X)
    Aig_ManForEachCo(pPoCone_aig, pVarOut, i_out){
      lits[0] = 2*(pPo_cnf->pVarNums[pVarOut->Id])+1;
      Cid = sat_solver_addclause( pSat, lits, lits+1);
    }
    Cnf_DataLift(pPo_cnf, nCnfVar);
    Cnf_DataWriteIntoSolverInt(pSat, pPo_cnf, 1, 0); // CNF of neg f''(X)
    Aig_ManForEachCo(pPoCone_aig, pVarOut, i_out){
      lits[0] = 2*(pPo_cnf->pVarNums[pVarOut->Id])+1;
      Cid = sat_solver_addclause( pSat, lits, lits+1);
    }
    
    // initialize empty SAT solver ordering of variables: 
    /*  x_1,     neg x_1,     ..., x_n,     neg x_n;     2*nCnfVar
        x'_1,    neg x'_1,    ..., x'_n,    neg x'_n;    4*nCnfVar
        x''_1,   neg x''_1,   ..., x''_n,   neg x''_n;   2*alpha_temp
        alpha_1, neg alpha_1, ..., alpha_n, neg alpha_n; 8*nCnfVar
        beta_1,  neg beta_1,  ..., beta_n,  neg beta_n   10*nCnfVar
    */
    
    // cout << "start writing a,b clauses:\n";
    Aig_Obj_t * pVarIn;
    int pos = 6*nCnfVar+2;
    int i_in;
    //vector<int> alphas, betas;
    //int alpha_temp, beta_temp;
    Aig_ManForEachCi(pPoCone_aig, pVarIn, i_in){
      int inId = pPo_cnf->pVarNums[pVarIn->Id];
      lits[0] = 2*(inId-2*nCnfVar) + 1;  
      lits[1] = 2*(inId-nCnfVar);
      lits[2] = pos;
      sat_solver_addclause(pSat, lits, lits+3);
      lits[0] = 2*(inId-nCnfVar) + 1;  
      lits[1] = 2*(inId-2*nCnfVar);
      lits[2] = pos; 
      pos += 2; // beta = alpha + 1
      sat_solver_addclause(pSat, lits, lits+3);
      lits[0] = 2*(inId-2*nCnfVar) + 1;  
      lits[1] = 2*inId;
      lits[2] = pos;
      sat_solver_addclause(pSat, lits, lits+3);
      lits[0] = 2*inId + 1;  
      lits[1] = 2*(inId-2*nCnfVar);
      lits[2] = pos;
      sat_solver_addclause(pSat, lits, lits+3);   
      pos += 2; 
    }
    /*
    Aig_ManForEachCi(pPoCone_aig, pVarIn, i){
      int inId = pPo_cnf->pVarNums[pVarIn->Id];
      alpha_temp = sat_solver_addvar(pSat);
      beta_temp = sat_solver_addvar(pSat);
      alphas.push_back(alpha_temp);
      betas.push_back(beta_temp);
      lits[0] = 2*inId + 1;  
      lits[1] = 2*nCnfVar + 2*inId;
      lits[2] = 2*alpha_temp;
      lits[3] = 2*nCnfVar + 2*inId + 1;  
      lits[4] = 2*inId;
      lits[5] = 2*alpha_temp;
      lits[6] = 2*inId + 1;  
      lits[7] = 4*nCnfVar + 2*inId;
      lits[8] = 2*beta_temp;
      lits[9] = 4*nCnfVar + 2*inId + 1;  
      lits[10] = 2*inId;
      lits[11] = 2*beta_temp;

      sat_solver_addclause(pSat, lits, lits+3);
      sat_solver_addclause(pSat, lits+3, lits+6);
      sat_solver_addclause(pSat, lits+6, lits+9);
      sat_solver_addclause(pSat, lits+9, lits+12);      
    }
    */
    /*
    cout<<"alpha array = [";
    for (int i=0; i<=alphas.size(); i++) cout<<alphas[i]<<", ";
    cout<<"]\n";
    cout<<"beta array = [";
    for (int i=0; i<=betas.size(); i++) cout<<betas[i]<<", ";
    cout<<"]\n";
    */
    // cout << "start setting seeds:\n";
    int seed10, seed01; 
    lit assumptList[2*nCi];
    int status;
    for (int i = 0; i < 2*nCi; i++){ 
      assumptList[i] = 6*nCnfVar + 2 + 2*i + 1;
    }
    for (seed10 = 0; seed10 < nCi-1 ; seed10++){
      for (seed01 = seed10+1; seed01 < nCi; seed01++){ 
        assumptList[2*seed10]--; // alpha
        assumptList[2*seed01+1]--; // beta
        status = sat_solver_solve(pSat, assumptList, assumptList + 2*nCi, 0, 0, 0, 0);
        // cout <<"Output"<<Abc_ObjName(pPo)<<"under (seed 10, seed 01) = (" << seed10 << ", " << seed01 << ") supports bi-decomp, status = "<<status<<'\n';
        assumptList[2*seed10]++; // alpha
        assumptList[2*seed01+1]++; // beta
        /*
        int nfinalConflict, * pfinalConflict;
        nfinalConflict = sat_solver_final(pSat, &pfinalConflict);
        */
        assert(status != l_Undef);
        if(status == -1) break; //unsat -> found some XA | XB | XC
      }
      if(status == -1) break; //unsat -> found some XA | XB | XC
    }

    // output
    if((status == -1) && Aig_ManCiNum(pPoCone_aig)){ // find the most balanced partition
      std::cout<<"PO "<<Abc_ObjName(pPo)<<" support partition: 1\n";

      // from now on, assumption list stores the value of alphas and betas for the final de-composition
      for(int i = 0; i < 2*nCi; i++){ 
        assumptList[i] = 1; 
      }

      int *pClauses, *pLit, *pStop;
      int nFinalClause = sat_solver_final(pSat, &pClauses); // find the final clause 

      // from now on, assumption list stores the value of alphas and betas for the final de-composition
      for(int i = 0; i < nFinalClause; i++){ 
        for ( pLit = &pClauses[i], pStop = &pClauses[i+1]; pLit < pStop; pLit++ ){
          assumptList[Abc_Lit2Var(*pLit)-(nCnfVar*3)-1]=0;
        } 
      } 

      //just try to make XA and XB balance
      int nXA=0, nXB=0;
      for(int i = 0; i < nCi; i++){
        if     (assumptList[(2*i)]==0 && assumptList[(2*i)+1]==1) cout<<"1"; //alpha=0, beta=1 => XB
        else if(assumptList[(2*i)]==1 && assumptList[(2*i)+1]==0) cout<<"2"; //alpha=1, beta=0 => XA
        else if(assumptList[(2*i)]==0 && assumptList[(2*i)+1]==0) cout<<"0"; //alpha=0, beta=0 => XC
        else if(nXA>nXB){ cout<<"1"; ++nXB; } 
        else            { cout<<"2"; ++nXA; }  
      }
      cout<<"\n";
    }
    else{
      cout<<"PO "<<Abc_ObjName(pPo)<<" support partition: 0\n";
    }
    /*
    for (seed10 = 0; seed10 < nCi-1 ; seed10++){
      for (seed01 = seed10+1; seed01 < nCi; seed01++){ 
        // assumption list 
        for (int i=0; i < nCi; i++){
          if (i == seed10){
            assumptList[i*2] = 6*nCnfVar;
            assumptList[i*2+1] = 6*nCnfVar + 2*nCi + 1;
          }
          else if (i == seed01){
            assumptList[i*2] = 6*nCnfVar + 1;
            assumptList[i*2+1] = 6*nCnfVar + 2*nCi;
          }
          else {
            assumptList[i*2] = 6*nCnfVar + 1;
            assumptList[i*2+1] = 6*nCnfVar + 2*nCi + 1;
          }
        }
        int status = sat_solver_solve(pSat, assumptList, assumptList + 2*nCi, 0, 0, 0, 0);
        cout <<"Output"<<Abc_ObjName(pPo)<<"under (seed 10, seed 01) = (" << seed10 << ", " << seed01 << ") supports bi-decomp: "<<status<<'\n';
        int nfinalConflict, * pfinalConflict;
        nfinalConflict = sat_solver_final(pSat, &pfinalConflict);
      }
    }
    */
    // Create alpha, beta clauses in the following form:
    // (neg x_i | x_i' | alpha_i)(neg x_i' | x_i | alpha_i)(neg x_i | x_i'' | beta_i)(neg x_i'' | x_i | beta_i)
    /*
    for (int clause_id = 0; clause_id < nVarTotal; clause_id++){
      int lits[4][3]; // lits[0], lits[1]: alpha; lits[2], lits[3]: beta
      lits[0][0] = 2*clause_id + 1;  
      lits[0][1] = 2*nVarTotal + 2*clause_id;
      lits[0][2] = 6*nVarTotal + 2*clause_id;
      lits[1][0] = 2*nVarTotal + 2*clause_id + 1;  
      lits[1][1] = 2*clause_id;
      lits[1][2] = 6*nVarTotal + 2*clause_id;
      lits[2][0] = 2*clause_id + 1;  
      lits[2][1] = 4*nVarTotal + 2*clause_id;
      lits[2][2] = 8*nVarTotal + 2*clause_id;
      lits[3][0] = 4*nVarTotal + 2*clause_id + 1;  
      lits[3][1] = 2*clause_id;
      lits[3][2] = 8*nVarTotal + 2*clause_id; 
      sat_solver_addclause(pSat, &lits[0][0], &lits[0][2]);
      sat_solver_addclause(pSat, &lits[1][0], &lits[1][2]);
      sat_solver_addclause(pSat, &lits[2][0], &lits[2][2]);
      sat_solver_addclause(pSat, &lits[3][0], &lits[3][2]);
    }
    int seed10, seed01; 
    lit assumptList[2*nVarTotal];
    int status;
    for (seed10 = 0; seed10 < nVarTotal-1 ; seed10++){
      for (seed01 = seed10+1; seed01 < nVarTotal; seed01++){ 
        // assumption list 
        for (int i=0; i < nVarTotal; i++){
          if (i == seed10){
            assumptList[i*2] = 6*nVarTotal + 2*i;
            assumptList[i*2+1] = 8*nVarTotal + 2*i + 1;
          }
          else if (i == seed01){
            assumptList[i*2] = 6*nVarTotal + 2*i + 1;
            assumptList[i*2+1] = 8*nVarTotal + 2*i;
          }
          else {
            assumptList[i*2] = 6*nVarTotal + 2*i + 1;
            assumptList[i*2+1] = 8*nVarTotal + 2*i + 1;
          }
        }
        int status = sat_solver_solve(pSat, assumptList, assumptList + 2*nVarTotal, 0, 0, 0, 0);
        cout <<"Output"<<Abc_ObjName(pPo)<<"under (seed 10, seed 01) = (" << seed10 << ", " << seed01 << ") supports bi-decomp: "<<status<<'\n';
        int nfinalConflict; 
        int * pfinalConflict;
        nfinalConflict = sat_solver_final(pSat, &pfinalConflict);
      }
    } 
    */
    // Implement the seed variable partition
    

  }

  // Use 
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
  Abc_Print(-2, "\t        determine and print the bi-decomposition of the output cone.\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ---------- lsv_print_msfc ----------

bool compareByID (Abc_Obj_t* Obj1, Abc_Obj_t* Obj2){
  return (Abc_ObjId(Obj1) < Abc_ObjId(Obj2) );
}

bool compareByTreeID (vector<Abc_Obj_t*> tree1, vector<Abc_Obj_t*> tree2){
  return (Abc_ObjId(tree1.front()) < Abc_ObjId(tree2.front()) );
}

int findMSFCLeaves(vector<Abc_Obj_t*> &msfc_nodes_vec, Abc_Obj_t* pObj, map<Abc_Obj_t*, int>& obj2vec, int& sortedNodeCount){ // find MSFC nodes recursively
  Abc_Obj_t* pFanin;
  int size = msfc_nodes_vec.size();
  int msfcNodeCount = 1;
  int i;
  
  // select and swap the current MSFC node to the back of the vector.
  sortedNodeCount++;
  Abc_Obj_t* Obj_temp = pObj; 
  int index_temp = obj2vec[pObj];
  msfc_nodes_vec[obj2vec[pObj]] = msfc_nodes_vec[size-sortedNodeCount];
  msfc_nodes_vec[size-sortedNodeCount] = Obj_temp;
  obj2vec[msfc_nodes_vec[size-sortedNodeCount]] = size-sortedNodeCount;
  obj2vec[msfc_nodes_vec[index_temp]] = index_temp;

  // check fanin nodes
  Abc_ObjForEachFanin(pObj, pFanin, i){ 
    if ((Abc_ObjFanoutNum(pFanin) == 1) 
    && ( Abc_ObjType(pFanin) != ABC_OBJ_PI)
    && (obj2vec[pFanin] < size-sortedNodeCount) ){ // find the children 
      msfcNodeCount += findMSFCLeaves (msfc_nodes_vec, pFanin, obj2vec, sortedNodeCount);
    }
  }
  return msfcNodeCount; // return total msfc node count when reading a leaf.
}

int findMSFCRoot(vector<Abc_Obj_t*> msfc_nodes_vec, Abc_Obj_t* pObj, map<Abc_Obj_t*, int> obj2vec, int sortedNodeCount){
  //cout <<"node name = "<<Abc_ObjName(pObj)<<", # of fanouts = "<<Abc_ObjFanoutNum(pObj)<<"\n";
  int size = msfc_nodes_vec.size();
  while(pObj != NULL){    
    if ((Abc_ObjFanoutNum(pObj) == 1) 
    && (Abc_ObjType(Abc_ObjFanout(pObj, 0)) != ABC_OBJ_PO) 
    && (obj2vec[Abc_ObjFanout(pObj, 0)] < size-sortedNodeCount)){ // keep searching the root of msfc tree
      Abc_Obj_t* pFanout = Abc_ObjFanout(pObj, 0);
      pObj = pFanout;
    }
    else return obj2vec[pObj]; // the root of msfc tree is found
  }
  return 0; 
}

void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  vector<Abc_Obj_t*> p_msfc_nodes;  
  vector<vector<Abc_Obj_t*> > msfc_trees; // stores SORTED msfc trees
  map<Abc_Obj_t*, int> obj2vec;
  int sortedNodeCount = 0;
  int msfcNodeCount = 0;
  int msfc_tree_index = 0;
  int node_index = 0;
  int i = 0;
  // load all nodes, including Const1
  for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ ){ 
    if ( (pObj) != NULL && (Abc_ObjIsNode(pObj) || (pObj->Type == ABC_OBJ_CONST1 && Abc_ObjFanoutNum(pObj) != 0))) { 
      p_msfc_nodes.push_back(pObj);
      obj2vec[pObj] = node_index;
      node_index++;
    }
  }
  int totalNodeCount = p_msfc_nodes.size();
  // if ((Abc_ObjFanoutNum(p_msfc_nodes.front()) == 1) || (Abc_ObjFanoutNum(p_msfc_nodes.front()) == 1)) p_msfc_nodes.erase(p_msfc_nodes.begin());
  while(sortedNodeCount < totalNodeCount){ // find an MSFC in the AIG
    // if (msfc_tree_index % 100 == 0) printf("msfc_tree %d\n", msfc_tree_index); 
    /*
    for (i=0; i<totalNodeCount; i++){
      cout<<"p_msfc_nodes["<<i<<"] = "<< Abc_ObjName(p_msfc_nodes[i])<<"\n";
      cout<<"obj2vec["<<Abc_ObjName(p_msfc_nodes[i])<<"] = "<<obj2vec[p_msfc_nodes[i]]<<"\n";
    }
    */
    // Find the root of the MSFC tree
    int rootID = findMSFCRoot(p_msfc_nodes, p_msfc_nodes.front(), obj2vec, sortedNodeCount); 
    //cout <<"root = "<<Abc_ObjName(p_msfc_nodes[rootID])<<"\n";
    
    // Find the nodes of the MSFC tree
    msfcNodeCount = findMSFCLeaves(p_msfc_nodes, p_msfc_nodes[rootID], obj2vec, sortedNodeCount); 
    
    // Sort the nodes in the MSFC tree by ID
    // sort(p_msfc_nodes.begin(), p_msfc_nodes.end() - sortedNodeCount, compareByID);
    sort(p_msfc_nodes.end() - sortedNodeCount, p_msfc_nodes.end() - sortedNodeCount + msfcNodeCount, compareByID);
    vector<Abc_Obj_t*> msfc_tree_temp;
    for (i=0; i<msfcNodeCount; i++){
      msfc_tree_temp.push_back(p_msfc_nodes[i + totalNodeCount - sortedNodeCount]);
      obj2vec[p_msfc_nodes[i + totalNodeCount - sortedNodeCount]] = i + totalNodeCount - sortedNodeCount;      
    }
    msfc_trees.push_back(msfc_tree_temp);
    msfc_tree_temp.clear();

    // print out info
    /*
    printf("MSFC %d: %s", msfc_tree_index, Abc_ObjName(p_msfc_nodes[totalNodeCount - sortedNodeCount]));
    if (msfcNodeCount > 1){
      for (node_index = totalNodeCount - sortedNodeCount + 1; node_index <= totalNodeCount - sortedNodeCount + msfcNodeCount - 1; ++node_index){
        printf(",%s",Abc_ObjName(p_msfc_nodes[node_index]));
      }
    }
    printf("\n");
    */
    msfc_tree_index++;
    msfcNodeCount=0;
  }
  // print out info
  sort(msfc_trees.begin(), msfc_trees.end(), compareByTreeID);
  for (int i=0; i < msfc_trees.size(); i++){
    printf("MSFC %d: %s", i, Abc_ObjName(msfc_trees[i][0]));
    if (msfc_trees[i].size() > 1){
      for (int j=1; j < msfc_trees[i].size(); j++){
        printf(",%s", Abc_ObjName(msfc_trees[i][j]));
      }
    }
    printf("\n"); 
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
  Abc_Print(-2, "\t        prints the maximum single-fanout cones (MSFCs) of the AIG\n");
  Abc_Print(-2, "\t-h    : print the command usage\n");
  return 1;
}

// ---------- lsv_print_nodes ----------

void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
  Abc_Obj_t* pObj;
  int i;
  Abc_NtkForEachNode(pNtk, pObj, i) {
    printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
    int j;
    Abc_Obj_t* pFanin;
    Abc_ObjForEachFanin(pObj, pFanin, j) {
      printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
             Abc_ObjName(pFanin));
    }
    Abc_Obj_t* pFanout;
    Abc_ObjForEachFanout(pObj, pFanout, j) {
      printf("  Fanout-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanout),
             Abc_ObjName(pFanout));
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