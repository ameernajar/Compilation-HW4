%code requires {
    #include "nodes.hpp"
    #include <memory>
    #define YYSTYPE std::shared_ptr<ast::Node>
}

%{
#include "output.hpp"

// bison declarations
extern int yylineno;
extern int yylex();

void yyerror(const char*);

// root of the AST, set by the parser and used by other parts of the compiler
std::shared_ptr<ast::Node> program;

using namespace std;

// TODO: Place any additional declarations here
%}

%token SC COMMA LPAREN RPAREN LBRACE RBRACE ASSIGN
%token VOID INT BYTE BOOL AND OR NOT TRUE FALSE RETURN IF ELSE WHILE BREAK CONTINUE
%token RELOP
%token BINOP
%token NUM NUM_B STRING
%token ID

%type Program Funcs FuncDecl RetType Formals FormalsList FormalDecl Statements Statement Call ExpList Type Exp

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%left OR
%left AND
%nonassoc RELOP
%left BINOP
%right NOT
%nonassoc LPAREN RPAREN

%%

Program:  Funcs { program = $1; }
;

// TODO: Define grammar here
Funcs: /* empty */ { $$ = make_shared<ast::Funcs>(); }
        | FuncDecl Funcs { $$ = make_shared<ast::Funcs>(dynamic_pointer_cast<ast::FuncDecl>($1), dynamic_pointer_cast<ast::Funcs>($2)); }
    ;
FuncDecl: RetType ID LPAREN Formals RPAREN LBRACE Statements RBRACE { $$ = make_shared<ast::FuncDecl>(dynamic_pointer_cast<ast::ID>($2), dynamic_pointer_cast<ast::Type>($1), dynamic_pointer_cast<ast::Formals>($4), dynamic_pointer_cast<ast::Statements>($7)); }
    ;
RetType: Type { $$ = dynamic_pointer_cast<ast::Type>($1); }
        | VOID { $$ = make_shared<ast::Type>(ast::BuiltInType::VOID); }
    ;
Formals: FormalsList { $$ = dynamic_pointer_cast<ast::Formals>($1); }
        | /* empty */ { $$ = make_shared<ast::Formals>(); }
        ;
FormalsList: FormalDecl COMMA FormalsList { $$ = make_shared<ast::Formals>(dynamic_pointer_cast<ast::FormalDecl>($1), dynamic_pointer_cast<ast::Formals>($3)); }
        | FormalDecl { $$ = make_shared<ast::Formals>(dynamic_pointer_cast<ast::FormalDecl>($1)); }
    ;
FormalDecl: Type ID { $$ = make_shared<ast::FormalDecl>(dynamic_pointer_cast<ast::ID>($2), dynamic_pointer_cast<ast::Type>($1)); }
    ;
Statements: Statements Statement { $$ = make_shared<ast::Statements>(dynamic_pointer_cast<ast::Statements>($1), dynamic_pointer_cast<ast::Statement>($2)); }
        | Statement { $$ = make_shared<ast::Statements>(dynamic_pointer_cast<ast::Statement>($1)); }
    ;
Statement: LBRACE Statements RBRACE { $$ = dynamic_pointer_cast<ast::Statements>($2); }
        | Type ID SC { $$ = make_shared<ast::VarDecl>(dynamic_pointer_cast<ast::ID>($2), dynamic_pointer_cast<ast::Type>($1)); }
        | Type ID ASSIGN Exp SC { $$ = make_shared<ast::VarDecl>(dynamic_pointer_cast<ast::ID>($2), dynamic_pointer_cast<ast::Type>($1), dynamic_pointer_cast<ast::Exp>($4)); }
        | ID ASSIGN Exp SC { $$ = make_shared<ast::Assign>(dynamic_pointer_cast<ast::ID>($1), dynamic_pointer_cast<ast::Exp>($3)); }
        | Call SC { $$ = dynamic_pointer_cast<ast::Call>($1); }
        | RETURN SC { $$ = make_shared<ast::Return>(nullptr); }
        | RETURN Exp SC { $$ = make_shared<ast::Return>( dynamic_pointer_cast<ast::Exp>($2)); }
        | IF LPAREN Exp RPAREN Statement ELSE Statement { $$ = make_shared<ast::If>(dynamic_pointer_cast<ast::Exp>($3), dynamic_pointer_cast<ast::Statement>($5), dynamic_pointer_cast<ast::Statement>($7)); }
        | IF LPAREN Exp RPAREN Statement %prec LOWER_THAN_ELSE { $$ = make_shared<ast::If>(dynamic_pointer_cast<ast::Exp>($3), dynamic_pointer_cast<ast::Statement>($5)); }
        | WHILE LPAREN Exp RPAREN Statement { $$ = make_shared<ast::While>(dynamic_pointer_cast<ast::Exp>($3), dynamic_pointer_cast<ast::Statement>($5)); }
        | BREAK SC { $$ = make_shared<ast::Break>(); }
        | CONTINUE SC { $$ = make_shared<ast::Continue>(); }
    ;
Call: ID LPAREN ExpList RPAREN { $$ = make_shared<ast::Call>(dynamic_pointer_cast<ast::ID>($1), dynamic_pointer_cast<ast::ExpList>($3)); }
    | ID LPAREN RPAREN { $$ = make_shared<ast::Call>(dynamic_pointer_cast<ast::ID>($1)); }
    ;

ExpList: Exp COMMA ExpList { $$ = make_shared<ast::ExpList>(dynamic_pointer_cast<ast::Exp>($1), dynamic_pointer_cast<ast::ExpList>($3)); }
        | Exp { $$ = make_shared<ast::ExpList>(dynamic_pointer_cast<ast::Exp>($1)); }
    ;
Type: INT { $$ = make_shared<ast::Type>(ast::BuiltInType::INT); }
        | BYTE { $$ = make_shared<ast::Type>(ast::BuiltInType::BYTE); }
        | BOOL { $$ = make_shared<ast::Type>(ast::BuiltInType::BOOL); }
    ;
Exp: Exp BINOP Exp {
            $$ = make_shared<ast::BinOp>(dynamic_pointer_cast<ast::Exp>($1), dynamic_pointer_cast<ast::Exp>($3), dynamic_pointer_cast<ast::BinOp>($2)->op);
        }
        | ID { $$ = dynamic_pointer_cast<ast::ID>($1); }
        | Call { $$ = dynamic_pointer_cast<ast::Call>($1); }
        | NUM { $$ = dynamic_pointer_cast<ast::Num>($1); }
        | NUM_B { $$ = dynamic_pointer_cast<ast::NumB>($1); }
        | STRING { $$ = dynamic_pointer_cast<ast::String>($1); }
        | TRUE { $$ = make_shared<ast::Bool>(true); }
        | FALSE { $$ = make_shared<ast::Bool>(false); }
        | NOT Exp { $$ = make_shared<ast::Not>(dynamic_pointer_cast<ast::Exp>($2)); }
        | Exp AND Exp { $$ = make_shared<ast::And>(dynamic_pointer_cast<ast::Exp>($1), dynamic_pointer_cast<ast::Exp>($3)); }
        | Exp OR Exp { $$ = make_shared<ast::Or>(dynamic_pointer_cast<ast::Exp>($1), dynamic_pointer_cast<ast::Exp>($3)); }
        | Exp RELOP Exp {
                $$ = make_shared<ast::RelOp>(dynamic_pointer_cast<ast::Exp>($1), dynamic_pointer_cast<ast::Exp>($3), dynamic_pointer_cast<ast::RelOp>($2)->op);
            }
        | LPAREN Type RPAREN Exp { $$ = make_shared<ast::Cast>(dynamic_pointer_cast<ast::Exp>($4), dynamic_pointer_cast<ast::Type>($2)); }
        | LPAREN Exp RPAREN { $$ = $2; }
    ;



%%

// TODO: Place any additional code here
void yyerror(const char* message) {
    output::errorSyn(yylineno);
    exit(1);
};