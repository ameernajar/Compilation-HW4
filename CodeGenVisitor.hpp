#include "visitor.hpp"
#include "output.hpp"
#include <memory>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

using std::cout;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
extern std::shared_ptr<ast::Node> program;

class GenCodeScope {

public:
    struct FuncInfo {
        ast::BuiltInType ret;
        vector<ast::BuiltInType> params;
    };

    struct VarInfo {
        ast::BuiltInType type;
        int offset;
    };
    struct Info {
        FuncInfo *func;
        VarInfo *var;
    };
    struct resultFound {
        ast::BuiltInType type;
        bool found;
        IdType idType;
        Info info;
    };

    // ------------------fields declaration------------------------
    std::unordered_map<std::string, FuncInfo> funcsMap;
    std::unordered_map<std::string, VarInfo> varsMap;
    std::unordered_map<std::string, string> varToReg;
    shared_ptr<GenCodeScope> parentScope;
    bool isLoopScope = false;
    int offset = 0; // for variables' offsets
    GenCodeScope(shared_ptr<GenCodeScope> parentScope) : funcsMap(), varsMap(),
                                           parentScope(parentScope),
                                           isLoopScope(parentScope ? parentScope->isLoopScope : false) {}

    // ---------------------------methods---------------------------------
    resultFound find(const string &id) {
        if (varsMap.find(id) != varsMap.end())
            return resultFound{varsMap[id].type, true, IdType::VAR, Info{nullptr, &varsMap[id]}};
        if (funcsMap.find(id) != funcsMap.end())
            return resultFound{funcsMap[id].ret, true, IdType::FUNC, Info{&funcsMap[id], nullptr}};
        if (parentScope)
            return parentScope->find(id);
        return resultFound{ast::BuiltInType::VOID, false, IdType::VAR, Info{nullptr, nullptr}};
    }
};

class CodeGenVisitor : public Visitor {
    output::CodeBuffer buffer;
    shared_ptr<GenCodeScope> currentScope = make_shared<GenCodeScope>(nullptr);
    ast::BuiltInType lastType;
    string lastReg;
    int lastValue;

    void getFuncs();

    void checkForMain();

public:
    CodeGenVisitor() = default;

    void analyze() {
        getFuncs();
        checkForMain();
    }
    void visitStatement(ast::Statement &node);

    void visit(ast::Num &node) override;

    void visit(ast::NumB &node) override;

    void visit(ast::String &node) override;

    void visit(ast::Bool &node) override;

    void visit(ast::ID &node) override;

    void visit(ast::BinOp &node) override;

    void visit(ast::RelOp &node) override;

    void visit(ast::Not &node) override;

    void visit(ast::And &node) override;

    void visit(ast::Or &node) override;

    void visit(ast::Type &node) override;

    void visit(ast::Cast &node) override;

    void visit(ast::ExpList &node) override;

    void visit(ast::Call &node) override;

    void visit(ast::Statements &node) override;

    void visit(ast::Break &node) override;

    void visit(ast::Continue &node) override;

    void visit(ast::Return &node) override;

    void visit(ast::If &node) override;

    void visit(ast::While &node) override;

    void visit(ast::VarDecl &node) override;

    void visit(ast::Assign &node) override;

    void visit(ast::Formal &node) override;

    void visit(ast::Formals &node) override;

    void visit(ast::FuncDecl &node) override;

    void visit(ast::Funcs &node) override;

    void divByZeroCheck(const std::string &rhsI32);

    bool blockTerminated = false;

    // loop stack for break/continue: (condLabel, endLabel)
    std::vector<std::pair<std::string, std::string>> loopStack;

    // helpers
    void emitLabel(const std::string& labelName);

    void emitBr(const std::string& labelName);

    void emitCondBr(const std::string& condI1, const std::string& trueLabel, const std::string& falseLabel);

    bool canEmit() const { return !blockTerminated; }

    void emitHelperFunctions();

    //void collectFuncDecls(ast::Funcs& root);
};