%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */

%x COMMENT STR

%%

 /*
  * TODO: Put your codes here (lab2).
  *
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  */


 /*
  * Keywords here
  */

if {adjust(); return Parser::IF;}
then {adjust(); return Parser::THEN;}
else {adjust(); return Parser::ELSE;}

nil {adjust(); return Parser::NIL;}
of  {adjust(); return Parser::OF;}
var {adjust(); return Parser::VAR;}
type {adjust(); return Parser::TYPE;}
array {adjust(); return Parser::ARRAY;}

let {adjust(); return Parser::LET;}
in  {adjust(); return Parser::IN;}
end {adjust(); return Parser::END;}

for {adjust(); return Parser::FOR;}
while {adjust(); return Parser::WHILE;}
to {adjust(); return Parser::TO;}
do {adjust(); return Parser::DO;}
break {adjust(); return Parser::BREAK;}

function {adjust(); return Parser::FUNCTION;}

[a-zA-Z][a-zA-Z0-9_]* {adjust(); return Parser::ID;}
[0-9]+ {adjust(); return Parser::INT;}

 /*
  * operator here
  */

\+ {adjust(); return Parser::PLUS;}
\- {adjust(); return Parser::MINUS;}
\* {adjust(); return Parser::TIMES;}
\/  {adjust(); return Parser::DIVIDE;}
\. {adjust(); return Parser::DOT;}
\<\> {adjust(); return Parser::NEQ;}
\>= {adjust(); return Parser::GE;}
\>  {adjust(); return Parser::GT;}
\<= {adjust(); return Parser::LE;}
\<  {adjust(); return Parser::LT;}
\&  {adjust(); return Parser::AND;}
\|  {adjust(); return Parser::OR;}
,  {adjust(); return Parser::COMMA;}
;  {adjust(); return Parser::SEMICOLON;}
\{ {adjust(); return Parser::LBRACE;}
\} {adjust(); return Parser::RBRACE;}
\( {adjust(); return Parser::LPAREN;}
\) {adjust(); return Parser::RPAREN;}
:= {adjust(); return Parser::ASSIGN;}
:  {adjust(); return Parser::COLON;}
=  {adjust(); return Parser::EQ;}
\[ {adjust(); return Parser::LBRACK;}
\] {adjust(); return Parser::RBRACK;}

\" {adjust(); begin(StartCondition__::STR);}
<STR>\" {adjustStr(); begin(StartCondition__::INITIAL); return Parser::STRING;}
<STR>\\[\n\t\r ]+\\ {adjustStr();}
<STR>[\n\t\"\\] {adjustStr();}
<STR>\\[0-9]{3} {adjustStr();}
<STR>\\\^[A-Z] {adjustStr();}
<STR>\\.|. {adjustStr();}

\/\* {adjust(); ++commentLevel_; begin(StartCondition__::COMMENT);}
<COMMENT>\/\* {adjust(); ++commentLevel_;}
<COMMENT>\*\/ {adjust(); if(!--commentLevel_ )begin(StartCondition__::INITIAL);} 
<COMMENT>\n {adjust();}
<COMMENT>. {adjust();}

 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg.Newline();}

 /* reserved words */

. {adjust(); errormsg.Error(errormsg.tokPos, "illegal token");}
