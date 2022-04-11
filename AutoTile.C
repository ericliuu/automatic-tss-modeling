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

// Performs a bottom up search of the AST, finding all enclosing
// analyzable loops of the given node
void findEnclosingLoops(
  SgNode* node,
  vector<SgForStatement*> &loops,
  set<SgForStatement*> &analyzableLoops
) {
  ROSE_ASSERT(node);

  SgStatement* refStmt = SageInterface::getEnclosingStatement(node);
  ROSE_ASSERT(refStmt);
  SgForStatement* fl = isSgForStatement(
    SageInterface::findEnclosingLoop(refStmt)
  );

  while (fl) {
    if (analyzableLoops.find(fl) == analyzableLoops.end())
      continue;
    loops.push_back(fl);
    refStmt = SageInterface::getEnclosingStatement(fl->get_parent());
    ROSE_ASSERT(refStmt);
    fl = isSgForStatement(SageInterface::findEnclosingLoop(refStmt));
  }
}

void collectLoopRefInfo(SgForStatement* forLoop) {

  vector<SgForStatement*> loops;
//  findEnclosingLoops(forLoop, loops);



}

int main(int argc, char *argv[]) {

  // Build a project
  SgProject *project = frontend(argc,argv);
  ROSE_ASSERT(project != NULL);

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

        // Skip imperfectly nested loops
        if (SageInterface::isCanonicalForLoop(fl)) {
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
