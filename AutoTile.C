#include "rose.h"
#include <iostream>
#include <fstream>
#include <string>

// #define DEBUG 1
# define MODEL_PATH "../models/mlp.pkl"
# define OUTPUT_PATH "model_predict.temp"

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
void getAllArrayRefs(SgNode* node, vector<SgNode*> &arrReadRefs,
                     vector<SgNode*> &arrWriteRefs) {

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
  vector<SgNode*> &filteredWriteRefs) {

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
 * Print feature map to stdout
 */
void printFeatures(map<string, int> &features) {
  cout << "\t\t\t Printing loop features:" << endl;
  for (const auto &pair : features) {
    cout << "\t\t\t\t " << pair.first << " " << pair.second << endl;
  }
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
bool collectLoopRefAndDist(SgForStatement* forLoop,
                           map<string, int> &refFeatures) {

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
  refFeatures["readInvariant"] = 0;
  refFeatures["readPrefetched"] = 0;
  refFeatures["readNonPrefetched"] = 0;
  refFeatures["writeInvariant"] = 0;
  refFeatures["writePrefetched"] = 0;
  refFeatures["writeNonPrefetched"] = 0;
  int num2DRef = 0;
  int num2PlusDRef = 0;

  for (SgNode* read : filteredReadRefs) {
    SgExpression* ref = isSgExpression(read);
    ROSE_ASSERT(ref);
    SgExpression* nameExp = NULL;
    vector<SgExpression*> *subscripts = new vector<SgExpression*>;
    ROSE_ASSERT(SageInterface::isArrayReference(ref, &nameExp, &subscripts));

    if (subscripts->size() > 2)
      num2PlusDRef++;

    // We only consider 2D data
    if (subscripts->size() != 2) {
      delete subscripts;
      continue;
    }

    num2DRef++;

    // Skip references with constant subscripts
    SgIntVal* testIdx0 = isSgIntVal((*subscripts)[0]);
    SgIntVal* testIdx1 = isSgIntVal((*subscripts)[1]);
    if (testIdx0 || testIdx1) {
      delete subscripts;
      continue;
    }

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

    delete subscripts;
  }

  for (SgNode* write : filteredWriteRefs) {
    SgExpression* ref = isSgExpression(write);
    ROSE_ASSERT(ref);
    SgExpression* nameExp = NULL;
    vector<SgExpression*> *subscripts = new vector<SgExpression*>;
    ROSE_ASSERT(SageInterface::isArrayReference(ref, &nameExp, &subscripts));

    if (subscripts->size() > 2)
      num2PlusDRef++;

    // We only consider 2D data
    if (subscripts->size() != 2) {
      delete subscripts;
      continue;
    }

    num2DRef++;

    // Skip references with constant subscripts
    SgIntVal* testIdx0 = isSgIntVal((*subscripts)[0]);
    SgIntVal* testIdx1 = isSgIntVal((*subscripts)[1]);
    if (testIdx0 || testIdx1) {
      delete subscripts;
      continue;
    }

    SgInitializedName* rowIdxName = SageInterface::convertRefToInitializedName(
        (*subscripts)[0]);
    SgInitializedName* colIdxName = SageInterface::convertRefToInitializedName(
        (*subscripts)[1]);

    // if columns of 2D data are indexed with dominating loop index,
    // then the reference is prefetched
    if (colIdxName == dominatingLoopIdx) {
      refFeatures["writePrefetched"]++;
    }

    // if rows and not columns are indexed with dominating loop index,
    // then the reference is non-prefetched
    else if (rowIdxName == dominatingLoopIdx) {
      refFeatures["writeNonPrefetched"]++;
    }

    // otherwise we have an invariant index
    else {
      refFeatures["writeInvariant"]++;
    }

    delete subscripts;
  }

  refFeatures["distToDominatingLoop"] = loopDistance(forLoop, dominatingLoop);

  // skip this loop if it contains no 2-dimensional references OR
  // if it has more 3+ dimensional references that 2-dimensional references

  return num2DRef > 0 && num2DRef > num2PlusDRef;
}

/*
 * Generate a tiled program with the specified loop tiled to the specified
 * size, then output both the tiled C code and binary. Finally concatenate
 * the loop features of this test case to the specified .csv file
 * @params
 * - argc    : needed to build a rose project
 * - argv    : needed to build a rose project
 * - fileName: name of the file which contains the loop we are tiling
 * - funcName: name of the function which contains the loop we are tiling
 * - lineNum : line number of the loop we are tiling
 * - colNum  : column number of the loop we are tiling
 * - tileSize: size of tiling to apply to the target loop
 * - features: loop features that are outputted to a specified .csv file
 * - csvName : name of the csv file
 */
void generateTiledProg(int argc, char *argv[], string fileName, string funcName,
                       int lineNum, int colNum, int tileSize,
                       map<string, int> &features, string csvName) {

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
      SageInterface::loopTiling(fl, 1, tileSize);
      break;
    }
  }

  // Unparse tiled program
  backend(project);

  // Hacky solution to generate multiple tiled programs for test case
  string baseName = fileName.substr(fileName.find_last_of("/\\") + 1);
  string::size_type const extLoc(baseName.find_last_of('.'));
  string baseNameNoExt = baseName.substr(0, extLoc);

  // Name outputs as {filename}_{lineNum}_{colNum}_{tileSize}
  string uniqueName = baseNameNoExt + "_" + to_string(lineNum) +
                      "_" + to_string(colNum) + "_" + to_string(tileSize);

  const string mvBinary = "mv a.out " + uniqueName + ".out";
  const string mvSrc = "mv rose_" + baseName + " " + uniqueName + ".c";
  system(mvBinary.c_str());
  system(mvSrc.c_str());

  // Append loop features to the input csv file
  ifstream fileExists(csvName);
  ofstream csvFile;

  // Add csv header line if the csv file does not yet exist
  if (!fileExists.is_open()) {
    fileExists.close();
    csvFile.open(csvName);
    csvFile << "uniqueFilename,rootFilename,tileSize,";
    csvFile << "readInvariant,readPrefetched,readNonPrefetched,";
    csvFile << "writeInvariant,writePrefetched,writeNonPrefetched,";
    csvFile << "distToDominatingLoop\n";
    csvFile.close();
  }

  csvFile.open (csvName, ios::out | ios::app);
  csvFile << uniqueName << "," << baseNameNoExt << "," << tileSize << ",";
  csvFile << features["readInvariant"] << ",";
  csvFile << features["readPrefetched"] << ",";
  csvFile << features["readNonPrefetched"] << ",";
  csvFile << features["writeInvariant"] << ",";
  csvFile << features["writePrefetched"] << ",";
  csvFile << features["writeNonPrefetched"] << ",";
  csvFile << features["distToDominatingLoop"] << "\n";

}

int getTileSizePrediction(map<string, int> loopFeatures, string modelPath,
                          string outputPath) {
  const string callModel = "python3 ../predict_tile_size.py " +
                            to_string(loopFeatures["readInvariant"]) +
                            " " + to_string(loopFeatures["readPrefetched"]) +
                            " " + to_string(loopFeatures["readNonPrefetched"]) +
                            " " + to_string(loopFeatures["writeInvariant"]) +
                            " " + to_string(loopFeatures["writePrefetched"]) +
                            " " + to_string(loopFeatures["writeNonPrefetched"]) +
                            " " + to_string(loopFeatures["distToDominatingLoop"]) +
                            " " + modelPath + " " + outputPath;
  system(callModel.c_str());

  ifstream predFile;
  string line = "";

  predFile.open(outputPath);
  ROSE_ASSERT(predFile.is_open());
  getline(predFile, line);

  int tileSize = atoi(line.c_str());
  return tileSize;
}

int main(int argc, char *argv[]) {

  // Build a project
  SgProject *project = frontend(argc,argv);
  ROSE_ASSERT(project);

  // For each source file in the project
  SgFilePtrList & ptr_list = project->get_fileList();

  for (SgFilePtrList::iterator iter = ptr_list.begin(); iter != ptr_list.end();
       iter++) {

    SgFile *sageFile = (*iter);
    SgSourceFile *sfile = isSgSourceFile(sageFile);
    ROSE_ASSERT(sfile);
    SgGlobal *root = sfile->get_globalScope();
    SgDeclarationStatementPtrList& declList = root->get_declarations();
    string fileName = sfile->getFileName();

    #ifdef DEBUG
    cout << "Found a file: " << fileName << endl;
    #endif

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

      #ifdef DEBUG
      cout << "\t Found a function" << endl;
      #endif

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
        #ifdef DEBUG
        cout << "\t\t Found a for-loop to tile at: ";
        cout << flInfo->get_line() << endl;
        #endif

        // Skip imperfectly nested loops and loops that are only singly nested
        if (!SageInterface::isCanonicalForLoop(fl)
            || (findNumberOfEnclosingLoops(fl) <= 1
                && findNumberOfEnclosedLoops(fl->get_loop_body()) == 0)) {
          #ifdef DEBUG
          cout << "\t\t\t Skipped malformed or single non-nested loop" << endl;
          #endif
          continue;
        }

        // Collect loop features
        map<string, int> loopFeatures;
        bool isCandidate = collectLoopRefAndDist(fl, loopFeatures);
        #ifdef DEBUG
        printFeatures(loopFeatures);
        #endif

        if (!isCandidate) {
          #ifdef DEBUG
          cout << "\t\t\t Loop doesn't have enough 2D array references" << endl;
          #endif
          continue;
        }

        int tileSize = getTileSizePrediction(loopFeatures, MODEL_PATH, OUTPUT_PATH);

        generateTiledProg(argc, argv, fileName, func->get_name().getString(),
                          flInfo->get_line(), flInfo->get_col(), tileSize,
                          loopFeatures, "features.csv");

      } // End for-loops loop

    } // End declarations loop

  } // End files loop

  #ifdef DEBUG
  cout << "Done ...\n";
  #endif
}
