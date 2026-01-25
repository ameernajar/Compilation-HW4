#include "CodeGenVisitor.hpp"
//
// Created by amirn on 12/19/2025.
//

void CodeGenVisitor::emitLabel(const std::string& labelName) {
    buffer.emitLabel(labelName);
    blockTerminated = false;
}

void CodeGenVisitor::emitBr(const std::string& labelName) {
    buffer << "br label " << labelName << "\n";
    blockTerminated = true;
}

void CodeGenVisitor::emitCondBr(const std::string& condI1,
                                const std::string& trueLabel,
                                const std::string& falseLabel) {
    buffer << "br i1 " << condI1 << ", label " << trueLabel << ", label " << falseLabel << "\n";
    blockTerminated = true;
}


/*bool isNumericType(ast::BuiltInType type) {
    return type == ast::BuiltInType::INT || type == ast::BuiltInType::BYTE;
}*/

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
    /*result ensured to be found from semnatic analysis*/


    lastType = result.type;
    lastReg = currentScope->varToReg[node.value];

    std::string p = buffer.freshVar();
    buffer << p << " = getelementptr [50 x i32], [50 x i32]* " << lastReg
           << ", i32 0, i32 " << lastReg << "\n";
    string loadVar = buffer.freshVar();
    buffer << loadVar << " = load i32, i32* " << p << "\n";
    lastReg = loadVar;
    // lastValue = value of the ID if its a variable,
    //    which we can get via storing it in an additional field in VarInfo class
}

void verifyFuncNotUsedAsVar(ast::Node &node, shared_ptr<GenCodeScope> &currentScope) {
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

    string trueLabel = buffer.freshLabel();
    string falseLabel = buffer.freshLabel();
    string endLabel = buffer.freshLabel();

    emitCondBr(leftReg, trueLabel, falseLabel);
    emitLabel(falseLabel);
    emitBr(endLabel);

    emitLabel(trueLabel);
    node.right->accept(*this);
    const ast::BuiltInType rightType = lastType;
    const string rightReg = lastReg;
    emitBr(endLabel);

    emitLabel(endLabel);
    string phiVar = buffer.freshVar();
    buffer << phiVar << " = phi i1 [0, " << falseLabel << "], [" << rightReg << ", " << trueLabel << "]\n";    
    string phi =  buffer.freshVar();
    buffer << phi << " = zext i1 " << phiVar << " to i32\n";
    lastReg = phi;
    lastType = ast::BuiltInType::BOOL;
}


    /*this doesn't implement short-circuit evaluation*/
    /*string var = buffer.freshVar();
    buffer << var << " = and i32 " << leftReg << ", " << rightReg << std::endl;
    lastReg = var;*/

    

void CodeGenVisitor::visit(ast::Or &node) {
    node.left->accept(*this);
    const ast::BuiltInType leftType = lastType;
    const string leftReg = lastReg;
    string trueLabel = buffer.freshLabel();
    string falseLabel = buffer.freshLabel();
    string endLabel = buffer.freshLabel();
    emitCondBr(leftReg, trueLabel, falseLabel);
    emitLabel(trueLabel);
    emitBr(endLabel);

    emitLabel(falseLabel);
    node.right->accept(*this);
    const ast::BuiltInType rightType = lastType;
    const string rightReg = lastReg;
    emitBr(endLabel);

    emitLabel(endLabel);
    string phiVar = buffer.freshVar();
    buffer << phiVar << " = phi i1 [1, " << trueLabel << "], [" << rightReg << ", " << falseLabel << "]\n";    
    string phi =  buffer.freshVar();    
    buffer << phi << " = zext i1 " << phiVar << " to i32\n";
    lastReg = phi;  
    lastType = ast::BuiltInType::BOOL;

    /*this doesn't implement short-circuit evaluation*/
    /*string var = buffer.freshVar();
    buffer << var << " = or i32 " << leftReg << ", " << rightReg << std::endl;
    lastReg = var;*/
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

    /*didn't know what to do here*/
    for (const auto &statement : node.statements) {
        if(!canEmit()) {
            break;
        }
        visitStatement(*statement);
    }
}

void CodeGenVisitor::visit(ast::Break &node) {
    if(!canEmit()) {
        return;
    }
    emitBr(loopStack.back().second);
}

void CodeGenVisitor::visit(ast::Continue &node) {
    if(!canEmit()) {
        return;
    }
    emitBr(loopStack.back().first);
}

void CodeGenVisitor::visit(ast::Return &node) {
    if(!canEmit()) {
        return;
    }
    if(!node.exp) {
        buffer << "ret void\n";
        blockTerminated = true;
        return;
    } 
    node.exp->accept(*this);
    buffer << "ret i32 " << lastReg << "\n";
    blockTerminated = true;
}

void CodeGenVisitor::visitStatement(ast::Statement &node) {
    try {
        ast::Statements &stmtNode = dynamic_cast<ast::Statements &>(node);
        currentScope = make_shared<GenCodeScope>(currentScope);

        stmtNode.accept(*this);

        currentScope = currentScope->parentScope;
    } catch (const std::bad_cast &) {
        node.accept(*this);
    }
}

void CodeGenVisitor::visit(ast::If &node) {
    if (!canEmit()) {
        return;
    }

    node.condition->accept(*this);
    //const ast::BuiltInType condType = lastType;

    std::string condI1 = buffer.freshVar();
    buffer << condI1 << " = icmp ne i32 " << lastReg << ", 0\n";
    std::string thenLabel = buffer.freshLabel();
    std::string elseLabel = buffer.freshLabel();
    std::string endLabel = buffer.freshLabel();
    if(node.otherwise) {
        emitCondBr(condI1, thenLabel, elseLabel);
    } else {
        emitCondBr(condI1, thenLabel, endLabel);
    }


    emitLabel(thenLabel);
    currentScope = make_shared<GenCodeScope>(currentScope);
    visitStatement(*node.then);
    currentScope = currentScope->parentScope;
    if(canEmit()) {
        emitBr(endLabel);
    }

    if (node.otherwise) {
        emitLabel(elseLabel);
        currentScope = make_shared<GenCodeScope>(currentScope);
        visitStatement(*node.otherwise);        
        currentScope = currentScope->parentScope;
        if(canEmit()) {
            emitBr(endLabel);
        }
    } else {
        emitLabel(endLabel);
    }

}

void CodeGenVisitor::visit(ast::While &node) {
    if (!canEmit()) {
        return;
    }
    std::string condLabel = buffer.freshLabel();
    std::string bodyLabel = buffer.freshLabel();
    std::string endLabel = buffer.freshLabel();
    emitBr(condLabel);
    emitLabel(condLabel);

    node.condition->accept(*this);
    string condI1 = buffer.freshVar();
    buffer << condI1 << " = icmp ne i32 " << lastReg << ", 0\n";
    emitCondBr(condI1, bodyLabel, endLabel);
    
    emitLabel(bodyLabel);
    loopStack.push_back({condLabel, endLabel});
    currentScope = make_shared<GenCodeScope>(currentScope);
    node.body->accept(*this);
    currentScope = currentScope->parentScope;
    loopStack.pop_back();
    if (canEmit()) {
        emitBr(condLabel);
    }
    emitLabel(endLabel);
    
}

/*bool isAssignable(ast::BuiltInType dst, ast::BuiltInType src) {
    if (dst == src)
        return true;
    if (dst == ast::BuiltInType::INT && src == ast::BuiltInType::BYTE)
        return true;
    return false;
}*/

void CodeGenVisitor::visit(ast::VarDecl &node) {
    if(!canEmit()) {
        return;
    }
    GenCodeScope::VarInfo varInfo;
    varInfo.type = node.type->type;
    varInfo.offset = int(currentScope->offset);
    currentScope->varsMap[node.id->value] = varInfo;
    currentScope->offset += 1;

    string varReg = buffer.freshVar();
    buffer << varReg << " = getelementptr [50 x i32], [50 x i32]* %locals, i32 0, i32 "
           << varInfo.offset << "\n";

    string initVal = "0";
    if (node.init_exp) {
        node.init_exp->accept(*this);
        initVal = lastReg;
        if(varInfo.type == ast::BuiltInType::BYTE) {
            initVal = maskByte(initVal, buffer);
        }
    }

    buffer << "store i32 " << initVal << ", i32* " << varReg << "\n";

    /*what is this??*/
    currentScope->varToReg[node.id->value] = varReg;
}




void CodeGenVisitor::visit(ast::Assign &node) {
    if(!canEmit()) {
        return;
    }


    const string &id = node.id->value;
    auto result = currentScope->find(id);
    /*result ensured to be found from semnatic analysis*/
    /*idType ensured to be var from semantic analysis*/
    /*no need to check ifAssignable as well*/
    
    node.exp->accept(*this);
    string expReg = lastReg;
    if(result.type == ast::BuiltInType::BYTE) {
        expReg = maskByte(expReg, buffer);
    }
    string varReg = currentScope->varToReg[id];
    buffer << "store i32 " << expReg << ", i32* " << varReg << "\n";
    
}

void CodeGenVisitor::visit(ast::Formal &node) {
    /*auto paramType = node.type->type;
    const string &paramid = node.id->value;

    // TODO: get param info add to scopes and emit
    currentScope->varsMap[paramid] = Scope::VarInfo{paramType, int(currentScope->offset) * -1};
    currentScope->offset++;*/
}

void CodeGenVisitor::visit(ast::Formals &node) {
    /*currentScope->offset = 1; // first param offset
    for (const auto &formal : node.formals) {
        if (currentScope->find(formal->id->value).found) {
            output::errorDef(formal->line, formal->id->value);
        }
        formal->accept(*this);
    }

    // reset offset for local variables
    currentScope->offset = 0;*/
}

void CodeGenVisitor::visit(ast::FuncDecl &node) {
    currentScope = make_shared<GenCodeScope>(currentScope->parentScope);
    blockTerminated = false;
    loopStack.clear();
    lastReg = "";
    lastType = ast::BuiltInType::VOID;
    const string &func_name = node.id->value;
    auto func = currentScope->funcsMap[func_name];
    if(func.ret != ast::BuiltInType::VOID) {
        buffer << "define i32 @" << func_name << "(";
    } else {
        buffer << "define void @" << func_name << "(";
    }
    std::vector<std::pair<string, ast::BuiltInType>> paramRegs;
    if(node.formals) {
        for(size_t i = 0; i < node.formals->formals.size(); i++) {
            if(i > 0) {
                buffer << ", ";
            }
            ast::BuiltInType paramType = node.formals->formals[i]->type->type;
            string paramReg = buffer.freshVar();
            paramRegs.push_back({paramReg, paramType});
            if(paramType == ast::BuiltInType::INT) {
                buffer << "i32 " << paramReg;
            } else if(paramType == ast::BuiltInType::STRING) {
                buffer << "i8* " << paramReg;
            }
        }
    }
    buffer << ") {\n";
    buffer << "%locals = alloca [50 x i32]\n";
    node.body->accept(*this);
    if(!blockTerminated) {
        if(func.ret == ast::BuiltInType::VOID) {
            buffer << "ret void\n";
        } else {
            buffer << "ret i32 0\n";
        }  
        blockTerminated = true;
    }
    buffer << "}\n\n";

    currentScope = currentScope->parentScope;
}

void CodeGenVisitor::emitHelperFunctions() {
    buffer << "declare void @print(i8*)\n";
    buffer << "declare void @printi(i32)\n";
    buffer << "declare void @exit(i32)\n\n";
    buffer << "declare void @printf(i8*, ...)\n\n";
}

// redundant if we are running semantic analysis before codegen
/*
void CodeGenVisitor::collectFuncDecls(ast::Funcs &root) {
    auto &funcsMap = currentScope->funcsMap;

    // add built-in functions
    funcsMap["print"] = {ast::BuiltInType::VOID, {ast::BuiltInType::STRING}};
    funcsMap["printi"] = {ast::BuiltInType::VOID, {ast::BuiltInType::INT}};

    for (const auto &func : root.funcs) {
        const string &name = func->id->value;
        vector<ast::BuiltInType> params = {};
        if (func->formals) {
            for (const auto &formal : func->formals->formals) {
                params.push_back(formal->type->type);
            }
        }

        funcsMap[name] = {func->return_type->type, params};
    }
}*/

void CodeGenVisitor::visit(ast::Funcs &node) {
    emitHelperFunctions();
    //collectFuncDecls(node);
    for (const auto &func : node.funcs) {
        func->accept(*this);
    }
    std::cout << buffer;
}