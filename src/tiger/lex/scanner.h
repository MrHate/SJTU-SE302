#ifndef TIGER_LEX_SCANNER_H_
#define TIGER_LEX_SCANNER_H_

#include <algorithm>
#include <string>

#include "scannerbase.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/parse/parserbase.h"

extern EM::ErrorMsg errormsg;

class Scanner : public ScannerBase {
 public:
  explicit Scanner(std::istream &in = std::cin, std::ostream &out = std::cout);

  Scanner(std::string const &infile, std::string const &outfile);

  int lex();

 private:
  int lex__();
  int executeAction__(size_t ruleNr);

  void print();
  void preCode();
  void postCode(PostEnum__ type);
  void adjust();
  void adjustStr();

  int commentLevel_;
  std::string stringBuf_;
  int charPos_;
};

inline Scanner::Scanner(std::istream &in, std::ostream &out)
    : ScannerBase(in, out), charPos_(1), commentLevel_(0){}

inline Scanner::Scanner(std::string const &infile, std::string const &outfile)
    : ScannerBase(infile, outfile), charPos_(1), commentLevel_(0) {}

inline int Scanner::lex() { return lex__(); }

inline void Scanner::preCode() {
  // optionally replace by your own code
}

inline void Scanner::postCode(PostEnum__ type) {
  // optionally replace by your own code
}

inline void Scanner::print() { print__(); }

inline void Scanner::adjust() {
	errormsg.tokPos = charPos_;
	charPos_ += length();
	if(matched() == "\"")stringBuf_ = "";
}

inline void Scanner::adjustStr() { 
	charPos_ += length(); 

	if(matched() == "\\n"){
		stringBuf_ += "\n";
	}	
	else if(matched() == "\\t"){
		stringBuf_ += "\t";
	}
	else if(matched() == "\\\""){
		stringBuf_ += "\"";
	}
	else if(matched() == "\\\\"){
		stringBuf_ += "\\";
	}
	else if (matched().size() >=3 && matched()[0] == '\\' && matched().back() == '\\'){

	}
	else if (matched().size() == 4 && matched()[0] == '\\' && matched()[1] >= '0' && matched()[1] <= '1'){
		stringBuf_ += (char)atoi(matched().substr(1,4).c_str());
	}
	else if(matched() == "\\^C"){
		stringBuf_ += (char)3;

	}
	else if(matched() == "\\^O"){
		stringBuf_ += (char)15;

	}
	else if(matched() == "\\^M"){
		stringBuf_ += (char)13;

	}
	else if(matched() == "\\^P"){
		stringBuf_ += (char)16;

	}
	else if(matched() == "\\^I"){
		stringBuf_ += (char)9;

	}
	else if(matched() == "\\^L"){
		stringBuf_ += (char)12;

	}
	else if(matched() == "\\^E"){
		stringBuf_ += (char)5;

	}
	else if(matched() == "\\^R"){
		stringBuf_ += (char)18;

	}
	else if(matched() == "\\^S"){
		stringBuf_ += (char)19;

	}
	//else if(matched().size() == 3 && matched()[0] == '\\' && matched()[1] == '^'){

	//}
	else if(matched() != "\""){
		stringBuf_ += matched();
	}
	setMatched(stringBuf_);
}

#endif  // TIGER_LEX_SCANNER_H_

