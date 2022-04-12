/*
 * A boiler plate starter file for using ROSE
 *
 * Input: sequential C/C++ code
 * Output: same C/C++ code 
 *
 */

#include "rose.h"
#include <iostream>

using namespace std;

int findNumberOfEnclosingLoops(SgNode* node) {
  ROSE_ASSERT(node);

  int numLoops = 0;
  SgStatement* refStmt = SageInterface::getEnclosingStatement(node);
  ROSE_ASSERT(refStmt);
  SgForStatement* fl = isSgForStatement(
    SageInterface::findEnclosingLoop(refStmt)
  );

  while (fl) {
    numLoops++;
    refStmt = SageInterface::getEnclosingStatement(fl->get_parent());
    ROSE_ASSERT(refStmt);
    fl = isSgForStatement(SageInterface::findEnclosingLoop(refStmt));
  }

  return numLoops;
}

int findNumberOfEnclosedLoops(SgNode* node) {
  Rose_STL_Container<SgNode*> loops = NodeQuery::querySubTree(
      node, V_SgForStatement);
  return loops.size();
}

// Performs a bottom up search of the AST, finding all enclosing
// analyzable loops of the given node
void findEnclosingLoops(
  SgNode* node,
  vector<SgForStatement*> &loops
) {
  ROSE_ASSERT(node);

  SgStatement* refStmt = SageInterface::getEnclosingStatement(node);
  ROSE_ASSERT(refStmt);
  SgForStatement* fl = isSgForStatement(
    SageInterface::findEnclosingLoop(refStmt)
  );

  while (fl) {
    loops.push_back(fl);
    refStmt = SageInterface::getEnclosingStatement(fl->get_parent());
    ROSE_ASSERT(refStmt);
    fl = isSgForStatement(SageInterface::findEnclosingLoop(refStmt));
  }
}

// Get all read and write array reference contained in the scope of node
void getAllArrayRefs(
  SgNode* node,
  vector<SgNode*> &arrWriteRefs,
  vector<SgNode*> &arrReadRefs
) {

  // Collect all references in this loop nest
  SgStatement *loopScope = SageInterface::getScope(node);
  vector<SgNode*> readRefs;
  vector<SgNode*> writeRefs;
  SageInterface::collectReadWriteRefs(loopScope, readRefs, writeRefs);

  // get all array writes
  for (int i = 0; i < writeRefs.size(); i++) {
    SgPntrArrRefExp* writeArrRef = isSgPntrArrRefExp(writeRefs[i]);
    if (writeArrRef == NULL)
      continue;
    arrWriteRefs.push_back(writeArrRef);
  }

  // get all array reads
  for (int i = 0; i < readRefs.size(); i++) {
    SgPntrArrRefExp* readArrRef = isSgPntrArrRefExp(readRefs[i]);
    if (readArrRef == NULL)
      continue;
    arrReadRefs.push_back(readArrRef);
  }
}


void collectLoopRefInfo(SgForStatement* forLoop) {

  // Collect all loop nested in forLoop, including forLoop
  Rose_STL_Container<SgNode*> loops = NodeQuery::querySubTree(
      forLoop->get_parent(), V_SgForStatement);

  SgForStatement* innermostLoop = isSgForStatement(loops[loops.size()-1]);
  ROSE_ASSERT(innermostLoop);

  // Collect all array references in this loop nest
  vector<SgNode*> arrReadRefs;
  vector<SgNode*> arrWriteRefs;
  getAllArrayRefs(forLoop, arrReadRefs, arrWriteRefs);


  cout << "----writes:" << endl;
  for (int i = 0; i < arrWriteRefs.size(); i++) {
    SgPntrArrRefExp* writeVarRef = isSgPntrArrRefExp(arrWriteRefs[i]);

    if (writeVarRef == NULL)
      continue;

    SageInterface::printAST(writeVarRef);
  }

  cout << "----reads:" << endl;
  for (int i = 0; i < arrReadRefs.size(); i++) {
    SgPntrArrRefExp* readVarRef = isSgPntrArrRefExp(arrReadRefs[i]);

    if (readVarRef == NULL)
      continue;

    SageInterface::printAST(readVarRef);
  }

/*
  for (Rose_STL_Container<SgNode*>::iterator iter = loops.begin();
       iter != loops.end(); iter++) {
    SgNode *currentLoop = *iter;
    SgForStatement *fl = isSgForStatement(currentLoop);
    SageInterface::printAST(fl);
  }
*/

//  vector<SgForStatement*> loops;
//  findEnclosingLoops(forLoop, loops);



}

int main(int argc, char *argv[]) {

  // Build a project
  SgProject *project = frontend(argc,argv);
  ROSE_ASSERT(project);

  // For each source file in the project
  SgFilePtrList & ptr_list = project->get_fileList();
  int testcaseNum = 0;

  for (SgFilePtrList::iterator iter = ptr_list.begin(); iter != ptr_list.end();
       iter++) {

    SgFile *sageFile = (*iter);
    SgSourceFile *sfile = isSgSourceFile(sageFile);
    ROSE_ASSERT(sfile);
    SgGlobal *root = sfile->get_globalScope();
    SgDeclarationStatementPtrList& declList = root->get_declarations();
    string fileName = sfile->getFileName();

    cout << "Found a file: " << fileName << endl;

    // For each function body in the scope
    for (SgDeclarationStatementPtrList::iterator p = declList.begin();
         p != declList.end(); ++p) {

      SgFunctionDeclaration *func = isSgFunctionDeclaration(*p);
      if (func == 0)  continue;
      SgFunctionDefinition *defn = func->get_definition();
      if (defn == 0)  continue;
      // Ignore functions in system headers, Can keep them to test robustness
      if (defn->get_file_info()->get_filename()
          != sageFile->get_file_info()->get_filename()) {
        continue;
      }

      cout << "\t Found a function" << endl;

      // Collect all loops
      Rose_STL_Container<SgNode*> loops = NodeQuery::querySubTree(
          defn,V_SgForStatement);
      if (loops.size() == 0) continue;

      // For each loop, collect loop features:
      // - invariant, prefetched, non-prefetched references
      for (Rose_STL_Container<SgNode*>::iterator iter = loops.begin();
           iter != loops.end(); iter++) {

        SgNode *currentLoop = *iter;
        SgForStatement *fl = isSgForStatement(currentLoop);

        cout << "\t\t Found a for-loop to tile at: ";
        cout << fl->get_file_info()->get_line() << endl;

        // Skip imperfectly nested loops and loops that are only singley nested
        if (SageInterface::isCanonicalForLoop(fl)
            && (findNumberOfEnclosingLoops(fl) > 1
            ||  findNumberOfEnclosedLoops(fl->get_loop_body()) > 0)) {
          collectLoopRefInfo(fl);
          testcaseNum++;
        } else {
          cout << "\t\t\t Skipped single non-nested loop" << endl;
        }

      }// End for-loops loop

    } // End declarations loop

  } // End files loop

  cout << "Done ...\n";
}
