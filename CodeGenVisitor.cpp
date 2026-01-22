#include "CodeGenVisitor.hpp"
//
// Created by amirn on 12/19/2025.
//

bool isNumericType(ast::BuiltInType type) {
    return type == ast::BuiltInType::INT || type == ast::BuiltInType::BYTE;
}

// NOTE: duplicated from previous HW
// void CodeGenVisitor::getFuncs() {

//     auto &funcsMap = currentScope->funcsMap;

//     // add built-in functions
//     funcsMap["print"] = {ast::BuiltInType::VOID, {ast::BuiltInType::STRING}};
//     funcsMap["printi"] = {ast::BuiltInType::VOID, {ast::BuiltInType::INT}};

//     for (const auto &func : dynamic_cast<ast::Funcs *>(program.get())->funcs) {
//         const string &name = func->id->value;
//         // given that a function must not be defined more than once (even with different parameters)
//         if (funcsMap.find(name) != funcsMap.end()) {
//             output::errorDef(func->id->line, name);
//         }
//         vector<ast::BuiltInType> params = {};
//         if (func->formals) {
//             for (const auto &formal : func->formals->formals) {
//                 params.push_back(formal->type->type);
//             }
//         }

//         funcsMap[name] = {func->return_type->type, params};
//     }
// }

// void CodeGenVisitor::checkForMain() {
//     const auto &result = currentScope->find("main");
//     if (!result.found || result.idType != IdType::FUNC ||
//         result.type != ast::BuiltInType::VOID || !result.info.func->params.empty()) {
//         output::errorMainMissing();
//     }
// }
// x = 5+4+3
void CodeGenVisitor::visit(ast::Num &node) {
    lastType = ast::BuiltInType::INT;
    // string var = buffer.freshVar();
    // buffer << var << " = add i32 0, " << node.value << std::endl;
    lastReg = std::to_string(node.value);
    lastValue = node.value;
    return;
}

void CodeGenVisitor::visit(ast::NumB &node) {
    lastType = ast::BuiltInType::BYTE;
    lastReg = std::to_string(node.value);
    lastValue = node.value;
    return;
}

void CodeGenVisitor::visit(ast::String &node) {
    lastType = ast::BuiltInType::STRING;
    const size_t len = node.value.length() + 1;
    string globalName = buffer.emitString(node.value);
    string ptr = buffer.freshVar();
    buffer << ptr << " = getelementptr inbounds [" << len << " x i8], [" << len << " x i8]* "
           << globalName << ", i32 0, i32 0" << std::endl;
    lastReg = ptr;
}

void CodeGenVisitor::visit(ast::Bool &node) {
    lastType = ast::BuiltInType::BOOL;
    lastReg = node.value ? "1" : "0";
}

void CodeGenVisitor::visit(ast::ID &node) {
    auto result = currentScope->find(node.value);
    if (!result.found)
        output::errorUndef(node.line, node.value);

    lastType = result.type;
    lastReg = currentScope->varToReg[node.value];
    // lastValue = value of the ID if its a variable,
    //    which we can get via storing it in an additional field in the VarInfo class
}

void verifyFuncNotUsedAsVar(ast::Node &node, shared_ptr<Scope> &currentScope) {
    if (auto func = dynamic_cast<ast::ID *>(&node)) {
        const auto findResult = currentScope->find(func->value);
        if (findResult.found && findResult.idType == IdType::FUNC) {
            output::errorDefAsFunc(node.line, func->value);
        }
    }
}

void CodeGenVisitor::divByZeroCheck(const std::string &rhs) {
    std::string isZero = buffer.freshVar();
    buffer << isZero << " = icmp eq i32 " << rhs << ", 0\n";

    std::string errLbl = buffer.freshLabel();
    std::string okLbl = buffer.freshLabel();
    buffer << "br i1 " << isZero << ", label " << errLbl << ", label " << okLbl << "\n";

    buffer.emitLabel(errLbl);
    const std::string msg = "Error division by zero";
    std::string g = buffer.emitString(msg);
    std::string p = buffer.freshVar();
    int n = (int)msg.size() + 1;
    buffer << p << " = getelementptr [" << n << " x i8], [" << n << " x i8]* "
           << g << ", i32 0, i32 0\n";
    buffer << "call void @print(i8* " << p << ")\n";
    buffer << "call void @exit(i32 0)\n";

    buffer.emitLabel(okLbl);
}
std::string maskByte(const std::string &i32reg, output::CodeBuffer &buffer) {
    std::string r = buffer.freshVar();
    buffer << r << " = and i32 " << i32reg << ", 255\n";
    return r;
}
void CodeGenVisitor::visit(ast::BinOp &node) {
    node.left->accept(*this);
    const ast::BuiltInType leftType = lastType;
    string leftReg = lastReg;
    int leftValue = lastValue;

    node.right->accept(*this);
    const ast::BuiltInType rightType = lastType;
    string rightReg = lastReg;
    int rightValue = lastValue;

    bool isInt = leftType == ast::BuiltInType::INT || rightType == ast::BuiltInType::INT;
    // LLVM IR stuff
    string var = buffer.freshVar();
    switch (node.op) {
    case ast::BinOpType::ADD:
        buffer << var << " = add ";
        lastValue = leftValue + rightValue;
        break;
    case ast::BinOpType::SUB:
        buffer << var << " = sub ";
        lastValue = leftValue - rightValue;
        break;
    case ast::BinOpType::MUL:
        buffer << var << " = mul ";
        lastValue = leftValue * rightValue;
        break;
    case ast::BinOpType::DIV:
        divByZeroCheck(rightReg);
        if (rightValue == 0) {
            cout << "Error division by zero" << std::endl;
            exit(0);
        }
        if (isInt)
            buffer << var << " = sdiv ";
        else
            buffer << var << " = udiv ";
        lastValue = leftValue / rightValue;
        break;
    }
    if (isInt) {
        buffer << "i32 " << leftReg << ", " << rightReg << std::endl;
        lastType = ast::BuiltInType::INT;
    } else {
        buffer << "i8 " << leftReg << ", " << rightReg << std::endl;
        string maskedVar = maskByte(var, buffer);
        lastType = ast::BuiltInType::BYTE;
    }
    lastReg = var;
}

void CodeGenVisitor::visit(ast::RelOp &node) {
    node.left->accept(*this);
    const ast::BuiltInType leftType = lastType;
    string leftReg = lastReg;

    node.right->accept(*this);
    const ast::BuiltInType rightType = lastType;
    string rightReg = lastReg;

    bool isInt = leftType == ast::BuiltInType::INT || rightType == ast::BuiltInType::INT;

    string var = buffer.freshVar();
    buffer << var << " = icmp ";
    switch (node.op) {
    case ast::RelOpType::EQ:
        buffer << "eq ";
        break;
    case ast::RelOpType::NE:
        buffer << "ne ";
        break;
    case ast::RelOpType::LT:
        if (isInt)
            buffer << "slt ";
        else
            buffer << "ult ";
        break;
    case ast::RelOpType::GT:
        if (isInt)
            buffer << "sgt ";
        else
            buffer << "ugt ";
        break;
    case ast::RelOpType::LE:
        if (isInt)
            buffer << "sle ";
        else
            buffer << "ule ";
        break;
    case ast::RelOpType::GE:
        if (isInt)
            buffer << "sge ";
        else
            buffer << "uge ";
        break;
    }
    buffer << "i32 " << leftReg << ", " << rightReg << std::endl;
    lastReg = var;
    lastType = ast::BuiltInType::BOOL;
}

void CodeGenVisitor::visit(ast::Not &node) {
    node.exp->accept(*this);
    const ast::BuiltInType expType = lastType;

    // verified as bool from semantic visitor
    string var = buffer.freshVar();
    buffer << var << " = xor i32 " << lastReg << ", 1" << std::endl;
    lastReg = var;

    lastType = ast::BuiltInType::BOOL;
}

void CodeGenVisitor::visit(ast::And &node) {
    node.left->accept(*this);
    const ast::BuiltInType leftType = lastType;
    const string leftReg = lastReg;

    node.right->accept(*this);
    const ast::BuiltInType rightType = lastType;
    const string rightReg = lastReg;

    string var = buffer.freshVar();
    buffer << var << " = and i32 " << leftReg << ", " << rightReg << std::endl;
    lastReg = var;

    lastType = ast::BuiltInType::BOOL;
}

void CodeGenVisitor::visit(ast::Or &node) {
    node.left->accept(*this);
    const ast::BuiltInType leftType = lastType;
    const string leftReg = lastReg;

    node.right->accept(*this);
    const ast::BuiltInType rightType = lastType;
    const string rightReg = lastReg;

    string var = buffer.freshVar();
    buffer << var << " = or i32 " << leftReg << ", " << rightReg << std::endl;
    lastReg = var;

    lastType = ast::BuiltInType::BOOL;
}

void CodeGenVisitor::visit(ast::Type &node) {
    lastType = node.type;
}

void CodeGenVisitor::visit(ast::Cast &node) {
    node.exp->accept(*this);
    const ast::BuiltInType expType = lastType;
    if (expType == ast::BuiltInType::INT && node.target_type->type == ast::BuiltInType::BYTE) {
        string lastReg = maskByte(lastReg, buffer);
        lastType = ast::BuiltInType::BYTE;
        return;
    }
    lastType = node.target_type->type;
    /*













    TODO: we stopped here










    */
}

void CodeGenVisitor::visit(ast::ExpList &node) {
    size_t argIndex = 0;
    for (const auto &exp : node.exps) {
        exp->accept(*this);
        currentScope->varToReg["__arg" + std::to_string(argIndex)] = lastReg;
        argIndex++;
    }
}

void CodeGenVisitor::visit(ast::Call &node) {
    const auto findResult = currentScope->find(node.func_id->value);
    // garaunteed to be a function from semantic analysis
    const auto &funcInfo = *findResult.info.func;
    if (node.args) {
        node.args->accept(*this);
    }

    // emit call
    string callReg = buffer.freshVar();
    buffer << callReg << " = call ";
    ;
    switch (funcInfo.ret) {
    case ast::BuiltInType::INT:
        buffer << "i32 ";
        break;
    // case ast::BuiltInType::BYTE:
    //     buffer << "i8 ";
    //     break;
    // case ast::BuiltInType::BOOL:
    //     buffer << "i1 ";
    //     break;
    case ast::BuiltInType::STRING:
        buffer << "i8* ";
        break;
    case ast::BuiltInType::VOID:
        buffer << "void ";
        break;
    }
    buffer << "@" << node.func_id->value << "(";
    for (size_t i = 0; i < funcInfo.params.size(); i++) {
        if (i > 0)
            buffer << ", ";
        switch (funcInfo.params[i]) {
        case ast::BuiltInType::INT:
            buffer << "i32 ";
            break;
        // case ast::BuiltInType::BYTE:
        //     buffer << "i8 ";
        //     break;
        // case ast::BuiltInType::BOOL:
        //     buffer << "i1 ";
        //     break;
        case ast::BuiltInType::STRING:
            buffer << "i8* ";
            break;
        case ast::BuiltInType::VOID:
            // should not happen
            break;
        }
        buffer << currentScope->varToReg["__arg" + std::to_string(i)];
    }
    buffer << ")" << std::endl;
    lastReg = callReg;

    lastType = funcInfo.ret;
}

void CodeGenVisitor::visit(ast::Statements &node) {
    // TODO: check scopes
    for (const auto &statement : node.statements) {
        visitStatement(*statement);
    }
}

void CodeGenVisitor::visit(ast::Break &node) {
    if (!currentScope->isLoopScope)
        output::errorUnexpectedBreak(node.line);
}

void CodeGenVisitor::visit(ast::Continue &node) {
    if (!currentScope->isLoopScope)
        output::errorUnexpectedContinue(node.line);
}

void CodeGenVisitor::visit(ast::Return &node) {
    if (node.exp) {
        node.exp->accept(*this);
    }
}

void CodeGenVisitor::visitStatement(ast::Statement &node) {
    try {
        ast::Statements &stmtNode = dynamic_cast<ast::Statements &>(node);
        currentScope = make_shared<Scope>(currentScope);

        stmtNode.accept(*this);

        currentScope = currentScope->parentScope;
    } catch (const std::bad_cast &) {
        node.accept(*this);
    }
}

void CodeGenVisitor::visit(ast::If &node) {
    node.condition->accept(*this);
    const ast::BuiltInType condType = lastType;

    if (condType != ast::BuiltInType::BOOL)
        output::errorMismatch(node.line);

    currentScope = make_shared<Scope>(currentScope);
    visitStatement(*node.then);
    currentScope = currentScope->parentScope;
    if (node.otherwise) {
        currentScope = make_shared<Scope>(currentScope);
        visitStatement(*node.otherwise);
        currentScope = currentScope->parentScope;
    }
}

void CodeGenVisitor::visit(ast::While &node) {
    node.condition->accept(*this);
    const ast::BuiltInType condType = lastType;

    if (condType != ast::BuiltInType::BOOL)
        output::errorMismatch(node.line);

    currentScope = make_shared<Scope>(currentScope);
    currentScope->isLoopScope = true;
    visitStatement(*node.body);
    currentScope = currentScope->parentScope;
}

bool isAssignable(ast::BuiltInType dst, ast::BuiltInType src) {
    if (dst == src)
        return true;
    if (dst == ast::BuiltInType::INT && src == ast::BuiltInType::BYTE)
        return true;
    return false;
}

void CodeGenVisitor::visit(ast::VarDecl &node) {
    if (currentScope->find(node.id->value).found)
        output::errorDef(node.line, node.id->value);

    ast::BuiltInType initType = ast::BuiltInType::VOID;
    if (node.init_exp) {
        node.init_exp->accept(*this);
        initType = lastType;
    }

    node.type->accept(*this);
    auto declType = lastType;

    if (node.init_exp) {
        if (!isAssignable(declType, initType)) {
            output::errorMismatch(node.line);
        }
    }

    currentScope->varsMap[node.id->value] = Scope::VarInfo{declType, int(currentScope->offset)};
    currentScope->offset += 1;
}

void CodeGenVisitor::visit(ast::Assign &node) {
    const string &id = node.id->value;
    auto result = currentScope->find(id);
    if (!result.found) {
        output::errorUndef(node.line, id);
    }
    if (result.idType == IdType::FUNC) {
        output::errorDefAsFunc(node.line, id);
    }
    auto dstType = result.type;
    node.exp->accept(*this);
    auto srcType = lastType;
    if (!isAssignable(dstType, srcType)) {
        output::errorMismatch(node.line);
    }
}

void CodeGenVisitor::visit(ast::Formal &node) {
    auto paramType = node.type->type;
    const string &paramid = node.id->value;

    // TODO: get param info add to scopes and emit
    currentScope->varsMap[paramid] = Scope::VarInfo{paramType, int(currentScope->offset) * -1};
    currentScope->offset++;
}

void CodeGenVisitor::visit(ast::Formals &node) {
    currentScope->offset = 1; // first param offset
    for (const auto &formal : node.formals) {
        if (currentScope->find(formal->id->value).found) {
            output::errorDef(formal->line, formal->id->value);
        }
        formal->accept(*this);
    }

    // reset offset for local variables
    currentScope->offset = 0;
}

void CodeGenVisitor::visit(ast::FuncDecl &node) {
    // a function is already added in getFuncs
    // we made sure in getFuncs that no function is defined more than once
    const string &func_name = node.id->value;
    auto func = currentScope->funcsMap[func_name];
    auto retType = func.ret;

    currentScope = make_shared<Scope>(currentScope);

    // visit formals
    if (node.formals) {
        node.formals->accept(*this);
    }

    node.body->accept(*this);

    currentScope = currentScope->parentScope;
}

void CodeGenVisitor::visit(ast::Funcs &node) {
    analyze();
    for (const auto &func : node.funcs) {
        func->accept(*this);
    }
    std::cout << buffer;
}