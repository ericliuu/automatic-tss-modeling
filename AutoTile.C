#include "rose.h"
#include <iostream>
#include <string>

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

/*
 * Performs a bottom up search of the AST, finding all enclosing
 * analyzable loops of the given node
 */
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

/*
 * Get all read and write array reference contained in the scope of node
 */
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
    if (writeArrRef)
      continue;
    arrWriteRefs.push_back(writeArrRef);
  }

  // get all array reads
  for (int i = 0; i < readRefs.size(); i++) {
    SgPntrArrRefExp* readArrRef = isSgPntrArrRefExp(readRefs[i]);
    if (readArrRef)
      continue;
    arrReadRefs.push_back(readArrRef);
  }
}


/*
 * Collects features according the Yuki et al.'s implementation, however
 * their work only considers perfectly nested 3-dimensional loops with
 * two dimensional data with one tiling orientation (i.e. they perform
 * a square tile on the innermost two loops). Since we are tiling every
 * loop sequentially (hence not square), we introduce an additional distance
 * feature to be passed into the model, which signifies how far we are from
 * the array references in the innermost loop. Features:
 * - [read/write] invariant references
 * - [read/write] prefetched references
 * - [read/write] non-prefetched references
 * - distance from innermost references
 */
void collectLoopRefAndDist(SgForStatement* forLoop) {

  // Collect all loops nested in forLoop, including forLoop
  Rose_STL_Container<SgNode*> loops = NodeQuery::querySubTree(
      forLoop->get_parent(), V_SgForStatement);

  SgForStatement* innermostLoop = isSgForStatement(loops[loops.size()-1]);
  ROSE_ASSERT(innermostLoop);

  SageInterface::printAST(innermostLoop);

  // Collect all array references in this loop nest
  vector<SgNode*> arrReadRefs;
  vector<SgNode*> arrWriteRefs;
  getAllArrayRefs(forLoop, arrReadRefs, arrWriteRefs);

/*
  // Consider only array references in the innermost loop
  // Note: for now we allow imperfectly nested loops
  cout << "----writes:" << endl;
  for (int i = 0; i < arrWriteRefs.size(); i++) {
    SgPntrArrRefExp* writeVarRef = isSgPntrArrRefExp(arrWriteRefs[i]);

    if (writeVarRef)
      continue;

    SageInterface::printAST(writeVarRef);
  }

  cout << "----reads:" << endl;
  for (int i = 0; i < arrReadRefs.size(); i++) {
    SgPntrArrRefExp* readVarRef = isSgPntrArrRefExp(arrReadRefs[i]);

    if (readVarRef)
      continue;

    SageInterface::printAST(readVarRef);
  }
*/
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
void generateTiledProg(int argc, char *argv[], string fileName,
                       string funcName, int lineNum, int colNum, int counter) {

  // Build a project
  SgProject *project = frontend(argc,argv);
  ROSE_ASSERT(project);

  // Get the function with our target loop
  SgFunctionDeclaration *func = SageInterface::findFunctionDeclaration(
      project, funcName, NULL, true);
  ROSE_ASSERT(func);
  SgFunctionDefinition *defn = func->get_definition();
  ROSE_ASSERT(defn);
  Rose_STL_Container<SgNode*> loops = NodeQuery::querySubTree(
      defn, V_SgForStatement);

  // Find the target loop and tile it
  for (Rose_STL_Container<SgNode*>::iterator iter = loops.begin();
       iter != loops.end(); iter++) {
    SgNode *currentLoop = *iter;
    SgForStatement *fl = isSgForStatement(currentLoop);
    if (fl->get_file_info()->get_col() == colNum
        && fl->get_file_info()->get_line() == lineNum) {
      SageInterface::loopTiling(fl, 1, 66);
      break;
    }
  }

  // unparse tiled program
  backend(project);

  // Hacky solution to generate multiple tiled programs for test case
  string baseName = fileName.substr(fileName.find_last_of("/\\") + 1);
  string::size_type const extLoc(baseName.find_last_of('.'));
  string baseNameNoExt = baseName.substr(0, extLoc);
  const string mvBinary = "mv a.out " + baseNameNoExt +
                          to_string(counter) + ".out";
  const string mvSrc = "mv rose_" + baseName + " " + baseNameNoExt +
                       to_string(counter) + ".c";
  system(mvBinary.c_str());
  system(mvSrc.c_str());
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

      // For each loop, tile the loop and unparse the program into a new file
      for (Rose_STL_Container<SgNode*>::iterator iter = loops.begin();
           iter != loops.end(); iter++) {

        SgNode *currentLoop = *iter;
        SgForStatement *fl = isSgForStatement(currentLoop);

        Sg_File_Info* flInfo = fl->get_file_info();
        cout << "\t\t Found a for-loop to tile at: ";
        cout << flInfo->get_line() << endl;

        // Skip imperfectly nested loops and loops that are only singly nested
        if (SageInterface::isCanonicalForLoop(fl)
            && (findNumberOfEnclosingLoops(fl) > 1
            ||  findNumberOfEnclosedLoops(fl->get_loop_body()) > 0)) {
          //generateTiledProg(argc, argv, fileName, func->get_name().getString(),
          //                  flInfo->get_line(), flInfo->get_col(), testcaseNum);
          collectLoopRefAndDist(fl);
          testcaseNum++;
        } else {
          cout << "\t\t\t Skipped malformed or single non-nested loop" << endl;
        }

      } // End for-loops loop

    } // End declarations loop

  } // End files loop

  cout << "Done ...\n";
}
