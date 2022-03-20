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

int main(int argc, char *argv[]) {

  // Build a project
  SgProject *project = frontend (argc,argv);
  ROSE_ASSERT (project != NULL);

  // For each source file in the project
  SgFilePtrList & ptr_list = project->get_fileList();

  for (SgFilePtrList::iterator iter = ptr_list.begin(); 
               iter!=ptr_list.end(); iter++) { 
    SgFile* sageFile = (*iter); 
    SgSourceFile * sfile = isSgSourceFile(sageFile);
    ROSE_ASSERT(sfile);
    SgGlobal *root = sfile->get_globalScope();
    SgDeclarationStatementPtrList& declList = root->get_declarations ();

    cout << "Found a file: " << sfile->getFileName() << endl;

    //For each function body in the scope
    for (SgDeclarationStatementPtrList::iterator p = declList.begin(); 
            p != declList.end(); ++p) {
      SgFunctionDeclaration *func = isSgFunctionDeclaration(*p);
      if (func == 0)  continue;
      SgFunctionDefinition *defn = func->get_definition();
      if (defn == 0)  continue;
       //ignore functions in system headers, Can keep them to test robustness
      if (defn->get_file_info()->get_filename()!=sageFile->get_file_info()->get_filename())
        continue;

      cout << "\t Found a function" << endl;

      // Collect all analyable loops in this function
      set<SgForStatement*> analyzableLoops;

      // For each loop 
      Rose_STL_Container<SgNode*> loops = NodeQuery::querySubTree(defn,V_SgForStatement); 
      if (loops.size()==0) continue;

      for (Rose_STL_Container<SgNode*>::iterator iter = loops.begin(); iter!= loops.end(); iter++) {
        SgNode *currentLoop = *iter;
        SgForStatement *fl = isSgForStatement(currentLoop);
        cout << "\t\t Found a for loop at: " << fl->get_file_info()->get_line() << endl;
        
        analyzableLoops.insert(fl);
      }// end for-loop for loops

      vector<SgNode*> writeRefs;
      vector<SgNode*> readRefs;

      SgStatement *fnDefnScope = SageInterface::getScope(defn);
      SageInterface::collectReadWriteRefs(fnDefnScope, readRefs, writeRefs);

      backend(project);

    } // end for-loop for declarations

  } //end for-loop for files

  cout << "Done ...\n";
  
  // Generate the source code
  // return backend (project);
}

