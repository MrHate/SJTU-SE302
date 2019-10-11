%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h

 /*
  * Please don't modify the lines above.
  */

%union {
  int ival;
  std::string* sval;
  S::Symbol *sym;
  A::Exp *exp;
  A::ExpList *explist;
  A::Var *var;
  A::DecList *declist;
  A::Dec *dec;
  A::EFieldList *efieldlist;
  A::EField *efield;
  A::NameAndTyList *tydeclist;
  A::NameAndTy *tydec;
  A::FieldList *fieldlist;
  A::Field *field;
  A::FunDecList *fundeclist;
  A::FunDec *fundec;
  A::Ty *ty;
  }

%token <sym> ID
%token <sval> STRING
%token <ival> INT

%token 
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK 
  LBRACE RBRACE DOT 
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF 
  BREAK NIL
  FUNCTION VAR TYPE

%nonassoc ASSIGN
%left AND OR
%nonassoc EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE

%type <exp> exp expseq
%type <explist> actuals  nonemptyactuals sequencing  sequencing_exps
%type <var>  lvalue one oneormore
%type <declist> decs decs_nonempty
%type <dec>  decs_nonempty_s vardec
%type <efieldlist> rec rec_nonempty
%type <efield> rec_one
%type <tydeclist> tydec
%type <tydec>  tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <ty> ty
%type <fundeclist> fundec
%type <fundec> fundec_one

%start program


 /*
  * Put your codes here (lab3).
  */

%%
program:  exp  {absyn_root = $1;};

exp: lvalue {$$ = new A::VarExp(errormsg.tokPos, $1);}
	|	NIL {$$ = new A::NilExp(errormsg.tokPos);}
	|	INT {$$ = new A::IntExp(errormsg.tokPos, $1);}
	| STRING {$$ = new A::StringExp(errormsg.tokPos, $1);}
	| ID LPAREN actuals RPAREN {$$ = new A::CallExp(errormsg.tokPos, $1, $3);}
	| exp PLUS exp {$$ = new A::OpExp(errormsg.tokPos, A::PLUS_OP, $1, $3);}
	| exp MINUS exp {$$ = new A::OpExp(errormsg.tokPos, A::MINUS_OP, $1, $3);}
	| exp TIMES exp {$$ = new A::OpExp(errormsg.tokPos, A::TIMES_OP, $1, $3);}
	| exp DIVIDE exp {$$ = new A::OpExp(errormsg.tokPos, A::DIVIDE_OP, $1, $3);}
	| exp EQ exp {$$ = new A::OpExp(errormsg.tokPos, A::EQ_OP, $1, $3);}
	| exp NEQ exp {$$ = new A::OpExp(errormsg.tokPos, A::NEQ_OP, $1, $3);}
	| exp LT exp {$$ = new A::OpExp(errormsg.tokPos, A::LT_OP, $1, $3);}
	| exp LE exp {$$ = new A::OpExp(errormsg.tokPos, A::LE_OP, $1, $3);}
	| exp GT exp {$$ = new A::OpExp(errormsg.tokPos, A::GT_OP, $1, $3);}
	| exp GE exp {$$ = new A::OpExp(errormsg.tokPos, A::GE_OP, $1, $3);}
	| exp AND exp {$$ = new A::IfExp(errormsg.tokPos, $1, $3, new A::IntExp(errormsg.tokPos, 0));}
	| exp OR exp {$$ = new A::IfExp(errormsg.tokPos, $1, new A::IntExp(errormsg.tokPos, 1), $3);}
  ;

vardec:  VAR ID ASSIGN exp  {$$ = new A::VarDec(errormsg.tokPos,$2, nullptr, $4);}
  |  VAR ID COLON ID ASSIGN exp  {$$ = new A::VarDec(errormsg.tokPos, $2, $4, $6);}
  ;
