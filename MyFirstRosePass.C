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

// A helper function to strip off possible type casting operations for an expression
// usually useful when compare two expressions to see if they actually refer to the same variable
static SgExpression* SkipCasting (SgExpression* exp)
{
  SgCastExp* cast_exp = isSgCastExp(exp);
   if (cast_exp != NULL)
   {
      SgExpression* operand = cast_exp->get_operand();
      assert(operand != 0);
      return SkipCasting(operand);
   }
  else
    return exp;
}

bool isVarReferenced(SgNode* var) {
  assert(var);
  SgNode* parent = var->get_parent();
  assert(parent);

  if (isSgAddressOfOp(parent))
    return true;

  return false;
}

bool isLoopAnalyzable(SgNode* loop) {
  ROSE_ASSERT(loop);

  SgForStatement* forLoop           = NULL;
  SgInitializedName* inducVarName   = NULL;
  SgExpression* loopCondition       = NULL;
  SgExpression* loopIncrement       = NULL;
  SgStatement* loopBody             = NULL;
  SgExpression* stepSize            = NULL;

  forLoop  = isSgForStatement(loop);
  ROSE_ASSERT(forLoop);
  loopBody = forLoop->get_loop_body();
  ROSE_ASSERT(loopBody);

  // a. one or more intialization variables
  SgStatementPtrList &initVars = forLoop->get_init_stmt();
  if (initVars.size() <= 0) {
    cout << "\t\t\t No loop induction variable ... skipping" << endl;
    return false;
  }

  // b. a single test expression that uses one of <, >, <=, >=
  loopCondition = forLoop->get_test_expr();
  SgBinaryOp* test = isSgBinaryOp(loopCondition);
  if (!loopCondition) {
    cout << "\t\t\t No loop condition ... skipping" << endl;
    return false;
  }
  if (!(test->variantT() == V_SgLessOrEqualOp
     || test->variantT() == V_SgLessThanOp
     || test->variantT() == V_SgGreaterOrEqualOp
     || test->variantT() == V_SgGreaterThanOp)) {
    cout << "\t\t\t Ill-formed loop condition ... skipping" << endl;
    return false;
  }

  // c. a single increment expression in the form i=i+c, i=i-c, ++i,
  //    --i, i++, or ++i, where c is a compile time constant, and  
  loopIncrement = forLoop->get_increment();
  if (!loopIncrement || isSgNullExpression(loopIncrement)) {
    cout << "\t\t\t Null loop increment ... skipping" << endl;
    return false;
  }

  // extract the increment variable and step size constant
  SgVarRefExp* loopIncrementVar = NULL;
  switch (loopIncrement->variantT()) {
    case V_SgPlusAssignOp:  //+=
    case V_SgMinusAssignOp: //-=
      loopIncrementVar = isSgVarRefExp(
        SkipCasting(isSgBinaryOp(loopIncrement)->get_lhs_operand())
      );
      stepSize = isSgBinaryOp(loopIncrement)->get_rhs_operand();
      break;
    case V_SgPlusPlusOp:    //++
    case V_SgMinusMinusOp:  //--
      loopIncrementVar = isSgVarRefExp(
        SkipCasting(isSgUnaryOp(loopIncrement)->get_operand())
      );
      stepSize = new SgIntVal(1,"");
      break;
    case V_SgAssignOp:      // =
      loopIncrementVar = isSgVarRefExp(
        SkipCasting(isSgBinaryOp(loopIncrement)->get_lhs_operand())
      );
      if (isSgPlusAssignOp(loopIncrement))
      stepSize = SkipCasting(
        isSgBinaryOp(isSgBinaryOp(loopIncrement)->get_rhs_operand())
      );
      break;
    default:
      cout << "\t\t\t Ill-formed loop increment ... skipping" << endl;
      return false;
  }

  // check the variable being incremented is ok
  if (!loopIncrementVar) {
    cout << "\t\t\t Invalid loop increment variable ... skipping" << endl;
    return false;
  }

  // since we have a list of potential induction variables, we iterate
  // over the induction var list and find a match to the LHS of our
  // increment instruction
  SgExpression* ivarast = NULL;
  for (auto init : initVars) {
    if (isSgVariableDeclaration(init)) {
      SgVariableDeclaration* decl = isSgVariableDeclaration(init);
      for (auto var : decl->get_variables()) {
        if (var->get_symbol_from_symbol_table() == loopIncrementVar->get_symbol()) {
          inducVarName = var;
          break;
        }
      }
      if (!inducVarName) {
        break;
      }
    } else if (SageInterface::isAssignmentStatement(init, &ivarast)) {
      SgVarRefExp* var = isSgVarRefExp(SkipCasting(ivarast));
      if (var && var->get_symbol() == loopIncrementVar->get_symbol()) {
        inducVarName = var->get_symbol()->get_declaration();
        break;
      }
    } else if (SgExprStatement* exp_stmt = isSgExprStatement(init)) {
      if (SgCommaOpExp* comma_exp = isSgCommaOpExp(exp_stmt->get_expression())) {
        SgCommaOpExp* leaf_exp = comma_exp;
        while (isSgCommaOpExp(leaf_exp->get_lhs_operand()))
          leaf_exp = isSgCommaOpExp(leaf_exp->get_lhs_operand());
        if (SgAssignOp* assign_op = isSgAssignOp(leaf_exp->get_lhs_operand())) {
          SgVarRefExp* var = isSgVarRefExp(assign_op->get_lhs_operand());
          if (var && var->get_symbol() == loopIncrementVar->get_symbol()) {
            inducVarName = var->get_symbol()->get_declaration();
            break;
          }
        }
      } else if (SgAssignOp* assign_op = isSgAssignOp(exp_stmt->get_expression())) {
        SgVarRefExp* var = isSgVarRefExp(assign_op->get_lhs_operand());
        if (var && var->get_symbol() == loopIncrementVar->get_symbol()) {
          inducVarName = var->get_symbol()->get_declaration();
          break;
        }
      }
    }
  }

  // check that we found a matching variable from the induction variable list
  if (!inducVarName) {
    cout << "\t\t\t Invalid loop increment variable ... skipping" << endl;
    return false;
  }

  // handle the the "i+c" part of "i=i+c" if we found an AssignOp
  if (isSgBinaryOp(stepSize)) {
    SgVarRefExp* test2 = isSgVarRefExp(
      SkipCasting(isSgBinaryOp(stepSize)->get_lhs_operand())
    );
    if (test2->get_symbol() != inducVarName->get_symbol_from_symbol_table()) {
      cout << "\t\t\t Ill-formed loop increment assignment... skipping" << endl;
      return false;
    }
    stepSize = isSgBinaryOp(stepSize)->get_rhs_operand();
  }

  // check the step size is constant
  if (!isSgValueExp(stepSize)) {
    cout << "\t\t\t Non constant step size variable ... skipping" << endl;
    return false;
  }

  // d. the value of i is not changed in the loop
  //    check this by iterating over all defs of the induction var
  SgStatement *loopScope = SageInterface::getScope(loop);
  vector<SgNode*> readRefs;
  vector<SgNode*> writeRefs;
  SageInterface::collectReadWriteRefs(loopScope, readRefs, writeRefs);

  for (int i = 0; i < writeRefs.size(); i++) {
    SgVarRefExp* writeVarRef = isSgVarRefExp(writeRefs[i]);

    if (writeVarRef == NULL)
      continue;

    if (writeVarRef->get_symbol() == inducVarName->get_symbol_from_symbol_table()
     && SageInterface::isAncestor(loopBody, writeVarRef)) {
      cout << "\t\t\t induction variable written to in body ... skipping" << endl;
      return false;
    }
  }

  // Also perform a conservative check that disallows any references to the induction var
  // since performing alias analysis would be a hassle
  Rose_STL_Container<SgNode*> varRefs = NodeQuery::querySubTree(loop,V_SgVarRefExp); 

  for (Rose_STL_Container<SgNode*>::iterator iter = varRefs.begin(); iter!= varRefs.end(); iter++) {
    SgNode* currentVar = *iter;
    SgVarRefExp* writeVarRef = isSgVarRefExp(currentVar);

    if (writeVarRef->get_symbol() == inducVarName->get_symbol_from_symbol_table()
     && SageInterface::isAncestor(loopBody, writeVarRef)
     && isVarReferenced(writeVarRef)) {
      cout << "\t\t\t induction variable may be modified via reference... skipping" << endl;
      return false;
    }
  }
    
  return true;
}

// Get the variable reference associated with the array of an
// array access
SgVarRefExp* getArrayVarRefExp(SgPntrArrRefExp* arrExp) {
  ROSE_ASSERT (arrExp != NULL);
  Rose_STL_Container<SgNode*> refList = NodeQuery::querySubTree(arrExp->get_lhs_operand(),V_SgVarRefExp);
  for (Rose_STL_Container<SgNode*>::iterator iter = refList.begin(); iter !=refList.end(); iter ++) {
    SgVarRefExp* cur_ref = isSgVarRefExp(*iter);
    ROSE_ASSERT(cur_ref != NULL);
    SgVariableSymbol * sym = cur_ref->get_symbol();
    ROSE_ASSERT(sym != NULL);
    SgInitializedName * i_name = sym->get_declaration();
    ROSE_ASSERT(i_name != NULL);
    SgArrayType * a_type = isSgArrayType(i_name->get_typeptr());
    if (a_type) {
      return cur_ref;
    }
  }
  ROSE_ASSERT(false); // this should never happen
  return NULL;
}

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

// Finds the lowest common ancestor in the AST between the
// two passed in nodes
SgNode* leastCommonAncestor(SgNode* a, SgNode* b) {
  if(!a || !b)
    return NULL;
  vector<SgNode*> ancestorsOfA;
  for (SgNode* p = a; !isSgProject(p); p = p->get_parent()) {
    ancestorsOfA.push_back(p);
  }
  while (b) {
    vector<SgNode*>::const_iterator i = std::find(ancestorsOfA.begin(), ancestorsOfA.end(), b);
    if (i != ancestorsOfA.end())
      return *i;
    b = b->get_parent();
  }
  return NULL;
}

// Populates the indices vector with all common loop indices
// that surround both array references. The vector is ordered
// from outermost index to innermost
bool getCommonLoopIndices(
  SgPntrArrRefExp* ref1,
  SgPntrArrRefExp* ref2,
  set<SgForStatement*> &analyzableLoops,
  vector<SgNode*> &indices
) {
  ROSE_ASSERT(ref1);
  ROSE_ASSERT(ref2);

  SgNode* ancestor = leastCommonAncestor(ref1, ref2);
  vector<SgForStatement*> loops;
  findEnclosingLoops(ancestor, loops, analyzableLoops);

  // push indices on in outermost to innermost order
  for (int i = loops.size()-1; i >= 0; --i) {
    indices.push_back(SageInterface::getLoopIndexVariable(loops[i]));
  }

  if (indices.size() > 0)
    return true;
  return false;
}

bool getDependencePairs(
  vector<SgNode*> writeRefs,
  vector<SgNode*> readRefs,
  set<SgForStatement*> analyzableLoops,
  set<tuple<SgNode*, SgNode*, vector<SgNode*>>> &dep
) {

  vector<SgNode*> arrWriteRefs;
  vector<SgNode*> arrReadRefs;

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

  // find all flow and anti-dependencies (RAW, WAR)
  for (SgNode* wRef : arrWriteRefs) {
    for (SgNode *rRef : arrReadRefs) {
      SgPntrArrRefExp* writeArrRef = isSgPntrArrRefExp(wRef);
      SgPntrArrRefExp* readArrRef = isSgPntrArrRefExp(rRef);

      // we only consider references to the same array
      if (getArrayVarRefExp(writeArrRef)->get_symbol() !=
          getArrayVarRefExp(readArrRef)->get_symbol()) {
        continue;
      }

      // get loop indices from common surrounding analyzable loops
      vector<SgNode*> indices;
      if (!getCommonLoopIndices(writeArrRef, readArrRef,
                                analyzableLoops, indices)) {
        continue;
      }

      cout << "\t\t Potential RAW/WAR Dependency found: " << writeArrRef->unparseToString();
      cout << " : " << readArrRef->unparseToString() << " : ";
      string separator = "";
      for (SgNode* i : indices) {
        cout << separator << i->unparseToString();
        separator = ", ";
      }
      cout << endl;

      dep.insert(make_tuple(writeArrRef, readArrRef, indices));
    }
  }

  // find all output dependencies (WAW)
  for (SgNode* wRef1 : arrWriteRefs) {
    for (SgNode *wRef2 : arrWriteRefs) {
      SgPntrArrRefExp* writeArrRef1 = isSgPntrArrRefExp(wRef1);
      SgPntrArrRefExp* writeArrRef2 = isSgPntrArrRefExp(wRef2);

      // we only consider references to the same array that are not
      // the exact same write
      if (getArrayVarRefExp(writeArrRef1)->get_symbol()
       != getArrayVarRefExp(writeArrRef2)->get_symbol()
       || writeArrRef1 == writeArrRef2){
        continue;
      }

      // get loop indices from common surrounding analyzable loops
      vector<SgNode*> indices;
      if (!getCommonLoopIndices(writeArrRef1, writeArrRef2,
                                analyzableLoops, indices)) {
        continue;
      }

      // ensure we only insert WAW in the order they appear
      if (( writeArrRef1->get_file_info()->get_line()
          < writeArrRef2->get_file_info()->get_line() )
       || ( writeArrRef1->get_file_info()->get_line()
         == writeArrRef2->get_file_info()->get_line()
         && writeArrRef1->get_file_info()->get_col()
          < writeArrRef2->get_file_info()->get_col() )) {

        cout << "\t\t Potential WAW Dependency found: " << writeArrRef1->unparseToString();
        cout << " : " << writeArrRef2->unparseToString() << " : ";
        string separator = "";
        for (SgNode* i : indices) {
          cout << separator << i->unparseToString();
          separator = ", ";
        }
        cout << endl;

        dep.insert(make_tuple(writeArrRef1, writeArrRef2, indices));
      }
    }
  }
  return true;
}

void printDependenciesToFile(
  set<tuple<SgNode*, SgNode*, vector<SgNode*>>> &dep,
  string filename
) {

  ofstream depFile(filename);
  for (auto triple : dep) {
    depFile << get<0>(triple)->unparseToString();
    depFile << " : " << get<1>(triple)->unparseToString() << " : ";
    string separator = "";
    for (SgNode* i : get<2>(triple)) {
      depFile << separator << i->unparseToString();
      separator = ", ";
    }
    depFile << endl;
  }
}

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

    // Collect all dependence pairs as well as surrounding loop indices
    // for each file
    // Each tuple contains:
    //     < writeRef, readRef/writeRef, enclosingLoops >
    set<tuple<SgNode*, SgNode*, vector<SgNode*>>> potentialDependencies;

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
        
        if (!isLoopAnalyzable(currentLoop)) {
          continue;
        }

        analyzableLoops.insert(fl);
      }// end for-loop for loops

      vector<SgNode*> writeRefs;
      vector<SgNode*> readRefs;

      SgStatement *fnDefnScope = SageInterface::getScope(defn);
      SageInterface::collectReadWriteRefs(fnDefnScope, readRefs, writeRefs);

      getDependencePairs(writeRefs, readRefs, analyzableLoops, potentialDependencies);

    } // end for-loop for declarations

    // create a new file to store the dependency table for each file
    string filename = sfile->getFileName();
    filename = filename.substr(filename.find_last_of("/") + 1);
    filename = filename.substr(0, filename.find_last_of("."));
    filename = filename + "_dependency_table.txt";
    printDependenciesToFile(potentialDependencies, filename);

  } //end for-loop for files

  cout << "Done ...\n";
  
  // Generate the source code
  // return backend (project);
}

