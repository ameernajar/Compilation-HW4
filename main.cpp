#include "output.hpp"
#include "nodes.hpp"
#include "myVisitor.hpp"
#include "CodeGenVisitor.hpp"

// Extern from the bison-generated parser
extern int yyparse();

extern std::shared_ptr<ast::Node> program;

int main() {
    // Parse the input. The result is stored in the global variable `program`
    yyparse();

    MyVisitor visitorSemantic;
    program->accept(visitorSemantic);

    CodeGenVisitor codeGenVisitor;
    program->accept(codeGenVisitor);
}
