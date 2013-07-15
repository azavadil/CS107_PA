/**
 * File: rsg.cc
 * ------------
 * Provides the implementation of the full RSG application, which
 * relies on the services of the built-in string, ifstream, vector,
 * and map classes as well as the custom Production and Definition
 * classes provided with the assignment.
 */
 
#include <map>
#include <fstream>
#include "definition.h"
#include "production.h"
#include <vector> 

using namespace std;

/**
 * Takes a reference to a legitimate infile (one that's been set up
 * to layer over a file) and populates the grammar map with the
 * collection of definitions that are spelled out in the referenced
 * file.  The function is written under the assumption that the
 * referenced data file is really a grammar file that's properly
 * formatted.  You may assume that all grammars are in fact properly
 * formatted.
 *
 * @param infile a valid reference to a flat text file storing the grammar.
 * @param grammar a reference to the STL map, which maps nonterminal strings
 *                to their definitions.
 */

static void readGrammar(ifstream& infile, map<string, Definition>& grammar)
{
  while (true) {
    string uselessText;
    getline(infile, uselessText, '{');
    if (infile.eof()) return;  // true? we encountered EOF before we saw a '{': no more productions!
    infile.putback('{');
    Definition def(infile);
    grammar[def.getNonterminal()] = def;
  }
}


/**
 * Implementation note: lsearch
 * ----------------------------
 * linear search. returns the index of the target if the target is found, -1 otherwise.  
 * 
 * Used in the program to search for '<' to determine if there are more expansions required.
 * 
 * @param v:  vector of strings that we're searching
 * @param target: the character that we're searching for
 */ 
 

int lsearch(const vector<string>& v, const string target){ 
  for (int i = 0; i < v.size(); i++) {
	// string::npos is the constant that the string function .find returns if there is no match
    if( v[i].find(target) != string::npos ) 
      return i; 
  }
  return -1; 
}

/**
 * Implementation note: expandText
 * -------------------------------
 * expandText recursively expands the text. As long as the text contains non-terminals (i.e. <non-terminal>) 
 * then one of the non-terminal's productions is randomly selected, the production expands the sequence, and
 * then we recur.
 * 
 * @param text: the text that we're expanding. We store the text 
 *              in a vector.
 * @param grammar: a map with the non-terminal and their productions 
 */



void expandText(vector<string>& text, map<string, Definition>& grammar){ 
  
  // find a non-terminal 
  int nontermIdx = lsearch(text, "<"); 
  // base case: non-terminal not found, we're done
  if ( nontermIdx == -1 ) return; 
  
  
  string nonterminal = text[nontermIdx]; 
  // definition is a non-terminal and the non-terminals productions
  Definition newDefintion = grammar[nonterminal]; 
  Production newProduction = newDefintion.getRandomProduction(); 
  
  // declare a new vector and push the production onto the vector
  vector<string> insertion; 
  for (Production::iterator curr = newProduction.begin(); curr != newProduction.end(); ++curr) { 
    string word = *curr; 
    insertion.push_back(word);
  }
   
  // expand the insertion 
  expandText(insertion, grammar);
  
  // remove the non-terminal
  vector<string>::iterator it = text.begin(); 
  text.erase(it + nontermIdx); 

  // insert the expanded text starting at the location of non-terminal the expansion replaces
  for (int i = 0; i < insertion.size(); i++){
    it = text.begin(); 
    text.insert(it + nontermIdx + i, insertion[i]); 
  }
}
 
 /**
 * Performs the rudimentary error checking needed to confirm that
 * the client provided a grammar file.  It then continues to
 * open the file, read the grammar into a map<string, Definition>,
 * and then print out the total number of Definitions that were read
 * in.  You're to update and decompose the main function to print
 * three randomly generated sentences, as illustrated by the sample
 * application.
 *
 * @param argc the number of tokens making up the command that invoked
 *   		   the RSG executable.  There must be at least two arguments,
 *             and only the first two are used.
 * @param argv the sequence of tokens making up the command, where each
 *             token is represented as a '\0'-terminated C string.
 */
 
int main(int argc, char *argv[])
{
  if (argc == 1) {
    cerr << "You need to specify the name of a grammar file." << endl;
    cerr << "Usage: rsg <path to grammar text file>" << endl;
    return 1; // non-zero return value means something bad happened 
  }
  
  ifstream grammarFile(argv[1]);
  if (grammarFile.fail()) {
    cerr << "Failed to open the file named \"" << argv[1] << "\".  Check to ensure the file exists. " << endl;
    return 2; // each bad thing has its own bad return value
  }
  
  // things are looking good...
  map<string, Definition> grammar;
  readGrammar(grammarFile, grammar);
  cout << "The grammar file called \"" << argv[1] << "\" contains "
       << grammar.size() << " definitions." << endl;
  
  string s = "<start>"; 
  vector<string> output; 
  output.push_back(s); 

  int count = 0; 
  while ( lsearch(output,"<") != -1){
    expandText(output, grammar); 
    count++; 
  }

  for (int i = 0; i < output.size(); i++)
    cout << output[i] << " " ; 

  return 0;
}
