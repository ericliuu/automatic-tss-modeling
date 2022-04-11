/*
 * A boiler plate starter file for using ROSE
 *
 * Input: sequential C/C++ code
 * Output: same C/C++ code 
 *
 */

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

void generateTiledProg(int argc, char *argv[], string fileName,
                       string funcName, SgName loopName, int counter) {

  // Build a project
  SgProject *project = frontend(argc,argv);
  ROSE_ASSERT(project != NULL);

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
    if (fl->get_mangled_name() == loopName) {
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

      // For each loop, tile the loop and unparse the program into a new file
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
          generateTiledProg(argc, argv, fileName, func->get_name().getString(),
                            fl->get_mangled_name(), testcaseNum);
          testcaseNum++;
        } else {
          cout << "\t\t\t Skipped malformed and/or single non-nested loop" << endl;
        }

      } // End for-loops loop

    } // End declarations loop

  } // End files loop

  cout << "Done ...\n";
}
