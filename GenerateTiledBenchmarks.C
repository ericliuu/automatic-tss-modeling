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
      SageInterface::findEnclosingLoop(refStmt));

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
 * Get all read and write array references contained in the scope of node
 */
void getAllArrayRefs(
  SgNode* node,
  vector<SgNode*> &arrReadRefs,
  vector<SgNode*> &arrWriteRefs
) {

  // Collect all references in this loop nest
  SgStatement *loopScope = SageInterface::getScope(node);
  vector<SgNode*> readRefs;
  vector<SgNode*> writeRefs;
  SageInterface::collectReadWriteRefs(loopScope, readRefs, writeRefs);

  // get all array reads
  for (SgNode *read : readRefs){
    SgPntrArrRefExp* readArrRef = isSgPntrArrRefExp(read);
    if (!readArrRef)
      continue;
    arrReadRefs.push_back(readArrRef);
  }

  // get all array writes
  for (SgNode *write : writeRefs) {
    SgPntrArrRefExp* writeArrRef = isSgPntrArrRefExp(write);
    if (!writeArrRef)
      continue;
    arrWriteRefs.push_back(writeArrRef);
  }
}

/*
 * Filter read and write references such that only those in the immediate body
 * of the "dominating" loop are stored.
 * - For a perfect loop nest, the dominating loop is the one whose body
 *   contains the most number of array references
 * - If we have more than one dominating loop (i.e. if reference counts are
 *   equal), then we pick the loop that appear later based on line
 *   number
 * @ret dominating for loop's SgForStatement node
 */
SgForStatement* filterRefsNotInDominatingLoop(
  vector<SgNode*> &arrReadRefs,
  vector<SgNode*> &arrWriteRefs,
  vector<SgNode*> &filteredReadRefs,
  vector<SgNode*> &filteredWriteRefs
) {

  // find how many references are in the immediate body of each for loop
  map<SgForStatement*, int> loopCounts;
  for (SgNode* cur : arrReadRefs) {
    SgStatement* refStmt = SageInterface::getEnclosingStatement(cur);
    ROSE_ASSERT(refStmt);
    SgForStatement* fl = isSgForStatement(
        SageInterface::findEnclosingLoop(refStmt));
    loopCounts[fl]++;
  }
  for (SgNode* cur : arrWriteRefs) {
    SgStatement* refStmt = SageInterface::getEnclosingStatement(cur);
    ROSE_ASSERT(refStmt);
    SgForStatement* fl = isSgForStatement(
        SageInterface::findEnclosingLoop(refStmt));
    loopCounts[fl]++;
  }

  // get the dominating loop
  int max = 0;
  SgForStatement* dominatingLoop = NULL;
  for (const auto &pair : loopCounts) {
    if (pair.second >= max) {
      dominatingLoop = pair.first;
    }
  }
  ROSE_ASSERT(dominatingLoop); // this should not happen

  // filter array references to containonly those in the dominating loop
  for (SgNode *cur : arrReadRefs) {
    SgStatement* refStmt = SageInterface::getEnclosingStatement(cur);
    ROSE_ASSERT(refStmt);
    SgForStatement* fl = isSgForStatement(
        SageInterface::findEnclosingLoop(refStmt));
    if (fl == dominatingLoop) {
      filteredReadRefs.push_back(cur);
    }
  }
  for (SgNode *cur : arrWriteRefs) {
    SgStatement* refStmt = SageInterface::getEnclosingStatement(cur);
    ROSE_ASSERT(refStmt);
    SgForStatement* fl = isSgForStatement(
        SageInterface::findEnclosingLoop(refStmt));
    if (fl == dominatingLoop) {
      filteredWriteRefs.push_back(cur);
    }
  }
  return dominatingLoop;
}

/*
 * Calculates the number of for loops separating an ancestor and descendant
 * for statement. 1 if there are the same loop
 */
int loopDistance(SgForStatement* ancestor, SgForStatement* descendant) {
  ROSE_ASSERT(SageInterface::isAncestor(ancestor, descendant->get_loop_body()));

  SgStatement* stmt = SageInterface::getEnclosingStatement(
      descendant);
  ROSE_ASSERT(stmt);
  SgForStatement* fl = isSgForStatement(SageInterface::findEnclosingLoop(stmt));
  int dist = 0;

  while (fl) {
    dist++;
    if (fl == ancestor)
      break;
    stmt = SageInterface::getEnclosingStatement(fl->get_parent());
    ROSE_ASSERT(stmt);
    fl = isSgForStatement(SageInterface::findEnclosingLoop(stmt));
  }

  return dist;
}


/*
 * Collects features according the Yuki et al.'s implementation, however
 * their work only considers perfectly nested 3-dimensional loops with
 * two dimensional data with one tiling orientation (i.e. they perform
 * a square tile on the innermost two loops). Since we are tiling every
 * loop sequentially (hence not square), we decide which loop "dominates"
 * the loop tiling transformation. This is the loop that contains the most
 * array references in its body. We introduce an additional distance
 * feature to be passed into the model, which signifies how far we are from
 * the array references in the dominating loop. This is require to
 * differentiate between tiling different outer loops that have the same
 * dominating loop. Features:
 * - [read/write] invariant references
 * - [read/write] prefetched references
 * - [read/write] non-prefetched references
 * - distance from dominating references
 * @ret true if forLoop is a valid candidate for tiling, false otherwise
 */
void collectLoopRefAndDist(
  SgForStatement* forLoop,
  map<string, int> &refFeatures
) {

  // Collect all loops nested in forLoop, including forLoop
  Rose_STL_Container<SgNode*> loops = NodeQuery::querySubTree(
      forLoop->get_parent(), V_SgForStatement);

  // Collect all array references in this loop nest
  vector<SgNode*> arrReadRefs;
  vector<SgNode*> arrWriteRefs;
  getAllArrayRefs(forLoop, arrReadRefs, arrWriteRefs);

  // Filter array references to those only contained in the dominating loop
  vector<SgNode*> filteredReadRefs;
  vector<SgNode*> filteredWriteRefs;
  SgForStatement* dominatingLoop = filterRefsNotInDominatingLoop(
      arrReadRefs, arrWriteRefs, filteredReadRefs, filteredWriteRefs);
  ROSE_ASSERT(dominatingLoop);

  SgInitializedName* dominatingLoopIdx = SageInterface::getLoopIndexVariable(
      dominatingLoop);

  // Collect features
  for (SgNode* read : filteredReadRefs) {
    SgExpression* ref = isSgExpression(read);
    ROSE_ASSERT(ref);
    SgExpression* nameExp = NULL;
    vector<SgExpression*> *subscripts = new vector<SgExpression*>;
    ROSE_ASSERT(SageInterface::isArrayReference(ref, &nameExp, &subscripts));

    // We only consider 2D data
    // TODO: Should 3+ dimensional data be filtered?
    if (subscripts->size() != 2)
      continue;

    SgInitializedName* rowIdxName = SageInterface::convertRefToInitializedName(
        (*subscripts)[0]);
    SgInitializedName* colIdxName = SageInterface::convertRefToInitializedName(
        (*subscripts)[1]);

    // if columns of 2D data are indexed with dominating loop index,
    // then the reference is prefetched
    if (colIdxName == dominatingLoopIdx) {
      refFeatures["readPrefetched"]++;
    }

    // if rows and not columns are indexed with dominating loop index,
    // then the reference is non-prefetched
    else if (rowIdxName == dominatingLoopIdx) {
      refFeatures["readNonPrefetched"]++;
    }

    // otherwise we have an invariant index
    else {
      refFeatures["readInvariant"]++;
    }
  }
  refFeatures["distToDominatingLoop"] = loopDistance(forLoop, dominatingLoop);
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
        if (!SageInterface::isCanonicalForLoop(fl)
            || (findNumberOfEnclosingLoops(fl) <= 1
                && findNumberOfEnclosedLoops(fl->get_loop_body()) == 0)) {
          cout << "\t\t\t Skipped malformed or single non-nested loop" << endl;
          continue;
        }

        // collect loop features
        map<string, int> loopFeatures;
        collectLoopRefAndDist(fl, loopFeatures);

        //generateTiledProg(argc, argv, fileName, func->get_name().getString(),
        //                  flInfo->get_line(), flInfo->get_col(), testcaseNum);
        testcaseNum++;

      } // End for-loops loop

    } // End declarations loop

  } // End files loop

  cout << "Done ...\n";
}
