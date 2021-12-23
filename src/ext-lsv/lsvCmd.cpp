// #include "base/abc/abc.h"
// #include "base/main/main.h"
// #include "base/main/mainInt.h"
// #include <vector>
// #include <stack>
// #include <algorithm>

// static int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv);
// static int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv);

// void init(Abc_Frame_t* pAbc) {
//   Cmd_CommandAdd(pAbc, "LSV", "lsv_print_nodes", Lsv_CommandPrintNodes, 0);
//   Cmd_CommandAdd(pAbc, "LSV", "lsv_print_msfc", Lsv_CommandPrintMSFC, 0);
// }

// void destroy(Abc_Frame_t* pAbc) {}

// Abc_FrameInitializer_t frame_initializer = {init, destroy};

// struct PackageRegistrationManager {
//   PackageRegistrationManager() { Abc_FrameAddInitializer(&frame_initializer); }
// } lsvPackageRegistrationManager;

// void Lsv_NtkPrintNodes(Abc_Ntk_t* pNtk) {
//   Abc_Obj_t* pObj;
//   int i;
//   Abc_NtkForEachNode(pNtk, pObj, i) {
//     printf("Object Id = %d, name = %s\n", Abc_ObjId(pObj), Abc_ObjName(pObj));
//     Abc_Obj_t* pFanin;
//     int j;
//     Abc_ObjForEachFanin(pObj, pFanin, j) {
//       printf("  Fanin-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
//              Abc_ObjName(pFanin));
//     }
//     Abc_ObjForEachFanout(pObj, pFanin, j) {
//       printf("  Fanout-%d: Id = %d, name = %s\n", j, Abc_ObjId(pFanin),
//              Abc_ObjName(pFanin));
//     }
//     if (Abc_NtkHasSop(pNtk)) {
//       printf("The SOP of this node:\n%s", (char*)pObj->pData);
//     }
//   }
// }

// int Lsv_CommandPrintNodes(Abc_Frame_t* pAbc, int argc, char** argv) {
//   Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
//   int c;
//   Extra_UtilGetoptReset();
//   while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
//     switch (c) {
//       case 'h':
//         goto usage;
//       default:
//         goto usage;
//     }
//   }
//   if (!pNtk) {
//     Abc_Print(-1, "Empty network.\n");
//     return 1;
//   }
//   Lsv_NtkPrintNodes(pNtk);
//   return 0;

// usage:
//   Abc_Print(-2, "usage: lsv_print_nodes [-h]\n");
//   Abc_Print(-2, "\t        prints the nodes in the network\n");
//   Abc_Print(-2, "\t-h    : print the command usage\n");
//   return 1;
// }

// // find maximum single-fanout cones (MSFCs) that covers all nodes
// bool msfc_compare(Abc_Obj_t* pObjA, Abc_Obj_t* pObjB) {
//   return Abc_ObjId(pObjA) < Abc_ObjId(pObjB);
// }

// bool msfc_compare_v(std::vector<Abc_Obj_t*> pA, std::vector<Abc_Obj_t*> pB) {
//   return Abc_ObjId(pA[0]) < Abc_ObjId(pB[0]);
// }

// void Lsv_NtkPrintMSFC(Abc_Ntk_t* pNtk) {
//   int len = Vec_PtrSize((pNtk)->vObjs);
//   int* isCovered = new int[len];
//   for (int i=0; i<len; ++i)
//     isCovered[i] = false;
//   std::vector<std::vector<Abc_Obj_t*> > msfcs;
  
//   int i;
//   Abc_Obj_t* pObj;
//   // Traverse all nodes and constant1
//   for (i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ ) {
//     if ((pObj)==NULL || !(Abc_ObjIsNode(pObj) || (pObj->Type==ABC_OBJ_CONST1)) ) continue;
//     if (isCovered[Abc_ObjId(pObj)] | (Abc_ObjFanoutNum(pObj) <= 1)) continue;

//     // find all connected nodes that formed a msfc
//     std::vector<Abc_Obj_t*> searchStack, msfc;
//     searchStack.push_back(pObj);
//     msfc.push_back(pObj);
//     isCovered[Abc_ObjId(pObj)] = true;

//     while (!searchStack.empty()) {
//       Abc_Obj_t* pObjCurr = (Abc_Obj_t*)searchStack.back();
//       searchStack.pop_back();

//       Abc_Obj_t* pFanin;
//       int j;
//       Abc_ObjForEachFanin(pObjCurr, pFanin, j) {
//         if (Abc_ObjIsPi(pFanin) || Abc_ObjFanoutNum(pFanin) != 1) continue;
//         searchStack.push_back(pFanin);
//         msfc.push_back(pFanin);
//         isCovered[Abc_ObjId(pFanin)] = true;
//       }
//     }

//     std::sort(msfc.begin(), msfc.end(), msfc_compare);
//     msfcs.push_back(msfc);
//   }

//   // traverse from PO
//   Abc_NtkForEachPo(pNtk, pObj, i) {
//     if (isCovered[Abc_ObjId(pObj)] || Abc_ObjFanoutNum(pObj)==1) continue;

//     // find all connected nodes that formed a msfc
//     std::vector<Abc_Obj_t*> searchStack, msfc;
//     int j;
//     Abc_Obj_t* pFanin;
//     Abc_ObjForEachFanin(pObj, pFanin, j) {
//       if (Abc_ObjIsPi(pFanin) || Abc_ObjFanoutNum(pFanin) != 1) continue;
//       searchStack.push_back(pFanin);
//       msfc.push_back(pFanin);
//       isCovered[Abc_ObjId(pFanin)] = true;
//     }

//     while (!searchStack.empty()) {
//       Abc_Obj_t* pObjCurr = (Abc_Obj_t*)searchStack.back();
//       searchStack.pop_back();
//       Abc_ObjForEachFanin(pObjCurr, pFanin, j) {
//         if (Abc_ObjIsPi(pFanin) || Abc_ObjFanoutNum(pFanin) != 1) continue;
//         searchStack.push_back(pFanin);
//         msfc.push_back(pFanin);
//         isCovered[Abc_ObjId(pFanin)] = true;
//       }
//     }

//     if (!msfc.empty()) {
//       std::sort(msfc.begin(), msfc.end(), msfc_compare);
//       msfcs.push_back(msfc);
//     }
//   }

//   // print MSFCs
//   std::sort(msfcs.begin(), msfcs.end(), msfc_compare_v);
//   i = 0;
//   for (; i < msfcs.size(); ++i) {
//     printf("MSFC %d: %s", i, Abc_ObjName(msfcs[i][0]));
//     for (int j=1; j < msfcs[i].size(); ++j) printf(",%s", Abc_ObjName(msfcs[i][j]));
//     printf("\n");
//   }
// }

// int Lsv_CommandPrintMSFC(Abc_Frame_t* pAbc, int argc, char** argv) {
//   Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
//   int c;
//   Extra_UtilGetoptReset();
//   while ((c = Extra_UtilGetopt(argc, argv, "h")) != EOF) {
//     switch (c) {
//       case 'h':
//         goto usage;
//       default:
//         goto usage;
//     }
//   }
//   if (!pNtk) {
//     Abc_Print(-1, "Empty network.\n");
//     return 1;
//   }
//   int fAll, fClean, fRecord;
//   Abc_NtkStrash(pNtk, fAll, fClean, fRecord);
//   Lsv_NtkPrintMSFC(pNtk);
//   return 0;

// usage:
//   Abc_Print(-2, "usage: lsv_print_msfc [-h]\n");
//   Abc_Print(-2, "\t        prints the MSFCs in the network\n");
//   Abc_Print(-2, "\t-h    : print the command usage\n");
//   return 1;
// }