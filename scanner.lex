%{
#include "nodes.hpp"
#include "output.hpp"
#include "parser.tab.h"
#include <string>

using namespace std;
using namespace output;


static std::string buffer;

%}

%option noyywrap

WS [ \t\r]
NEWLINE [\n]
LETTER [a-zA-Z]
DIGIT [0-9]
ID {LETTER}({LETTER}|{DIGIT})*
BINOP [+\-*/]
RELOP (==|!=|<=|>=|<|>)
COMMENT \/\/[^\n]*
NUM (0|[1-9]({DIGIT}*))
NUM_B {NUM}b

%x STR
%%

{NEWLINE} {
        yylineno++;
}
{WS} {/*nothin*/}
{COMMENT} {/* ignore comments */}
; {return SC;}
, {return COMMA;}
\( {return LPAREN;}
\) {return RPAREN;}
\{ {return LBRACE;}
\} {return RBRACE;}
= {return ASSIGN;}
{RELOP} {
    switch(yytext[0]) {
        case '<':
            if (yytext[1] == '=')
                yylval = make_shared<ast::RelOp>(nullptr,nullptr,ast::RelOpType::LE);
            else
                yylval = make_shared<ast::RelOp>(nullptr,nullptr,ast::RelOpType::LT);
            break;
        case '>':
            if (yytext[1] == '=')
                yylval = make_shared<ast::RelOp>(nullptr,nullptr,ast::RelOpType::GE);
            else
                yylval = make_shared<ast::RelOp>(nullptr,nullptr,ast::RelOpType::GT);
            break;
        case '=':
            yylval = make_shared<ast::RelOp>(nullptr,nullptr,ast::RelOpType::EQ);
            break;
        case '!':
            yylval = make_shared<ast::RelOp>(nullptr,nullptr,ast::RelOpType::NE);
            break;
    }
    return RELOP;
    }
{BINOP} {
    switch(yytext[0]) {
        case '+':
            yylval = make_shared<ast::BinOp>(nullptr, nullptr, ast::BinOpType::ADD);
            break;
        case '-':
            yylval = make_shared<ast::BinOp>(nullptr, nullptr, ast::BinOpType::SUB);
            break;
        case '*':
            yylval = make_shared<ast::BinOp>(nullptr, nullptr, ast::BinOpType::MUL);
            break;
        case '/':
            yylval = make_shared<ast::BinOp>(nullptr, nullptr, ast::BinOpType::DIV);
            break;
    }
    return BINOP;
    }
"void" { return VOID;}
"int" { return INT;}
"byte" { return BYTE;}
"bool" { return BOOL;}
"and" { return AND;}
"or" { return OR;}
"not" { return NOT;}
"true" { return TRUE;}
"false" { return FALSE;}
"return" {return RETURN;}
"if" {return IF;}
"else" {return ELSE;}
"while" {return WHILE;}
"break" {return BREAK;}
"continue" {return CONTINUE;}
{ID} {
    yylval = make_shared<ast::ID>(yytext);
    return ID;
    }
{NUM} {
    yylval = make_shared<ast::Num>(yytext);
    return NUM;
    }
{NUM_B} {
    yylval = make_shared<ast::NumB>(yytext);
    return NUM_B;
}

\" {
    buffer = "";
    BEGIN(STR);
}

<STR>\" {
    BEGIN(INITIAL);
    std::string quoted = "\"" + buffer + "\"";
    yylval = make_shared<ast::String>(quoted.c_str());
    return STRING;
}

<STR>\n {
    errorLex(yylineno);
    exit(0);
}

<STR><<EOF>> {
    errorLex(yylineno);
    exit(0);
}

<STR>\\x[0-9a-fA-F]{2} {
    std::string hexStr = std::string(yytext).substr(2, 2);
    int value = std::stoi(hexStr, nullptr, 16);
    if (value < 0x20 || value > 0x7E) {
        errorLex(yylineno);
        exit(0);
    }
    buffer += (char)value;
}

<STR>\\x[^("| \t\r|\n)]{1,2} {
    // invalid hex escape sequence
    errorLex(yylineno);
    exit(0);
}

<STR>\\n {
    buffer += '\n';
}

<STR>\\r {
    buffer += '\r';
}

<STR>\\t {
    buffer += '\t';
}

<STR>\\0 {
    errorLex(yylineno);
}

<STR>\\\\ {
    buffer += '\\';
}

<STR>\\\" {
    buffer += '\"';
}

<STR>\\. {
    std::string escSeq = "";
    escSeq += yytext[1];
    errorLex(yylineno);
    exit(0);
}

<STR>. {
    char c = yytext[0];
    buffer += c;
}

. { errorLex(yylineno); }


%%
