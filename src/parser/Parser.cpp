#include "Parser.h"
#include "../utils/ErrorHandler.h"

namespace MyCompiler
{

    Parser::Parser(Lexer &lexer) : lexer_(lexer)
    {
        advance(); // 读取第一个 Token
    }

    // ================================================================
    //  核心方法
    // ================================================================

    void Parser::advance()
    {
        // TODO: previousToken_ = currentToken_; currentToken_ = lexer_.nextToken();
        previousToken_ = currentToken_;
        currentToken_ = lexer_.nextToken();
    }

    bool Parser::check(TokenType t) const
    {
        // TODO: 检查当前 Token 类型
        return (currentToken_.type == t); // t是期望的类型，currentToken_.type是当前的类型
    }

    bool Parser::match(TokenType t)
    {
        // TODO: 如果 check(t)，则 advance() 并返回 true
        if (check(t))
        {
            advance();
            return true;
        }
        return false;
    }

    Token Parser::consume(TokenType t, const std::string &errMsg)
    {
        // TODO: 如果 check(t)，保存 Token 后 advance()；否则抛出 ParseError
        if (check(t))
        {
            Token tok = currentToken_;
            advance();
            return tok;
        }
        throw ParseError(errMsg, currentToken_.line, currentToken_.column);
    }

    void Parser::synchronize()
    {
        // TODO: 恐慌模式错误恢复 —— 跳过 Token 直到同步点（; } if while 等）
        advance();
        while (!lexer_.isAtEnd() && !check(TokenType::EOF_TOKEN))
        {
            if (previousToken_.type == TokenType::SEMICOLON)
                return;
            switch (currentToken_.type)
            {
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::RETURN:
                return;
            default:
                break;
            }
            advance();
        }
    }

    // ================================================================
    //  CompUnit -> (Decl | FuncDef)+
    // ================================================================

    std::unique_ptr<Program> Parser::parse()
    {
        return parseProgram();
    }

    std::unique_ptr<Program> Parser::parseProgram()
    {
        // TODO: 循环解析顶层声明或函数定义
        //       ToyC 顶层只有: ConstDecl / VarDecl / FuncDef
        auto program = std::make_unique<Program>();
        while (!check(TokenType::EOF_TOKEN))
        {
            if (check(TokenType::CONST))
            {
                program->statements.push_back(parseConstDecl());
            }
            else if (check(TokenType::INT) || check(TokenType::VOID))
            {
                // 可能是 FuncDef(int/void) 或 VarDecl(int)
                // 策略: 解析返回类型, 然后看 ID 后面是 '(' 还是 '='
                DataType type = parseType();
                Token name = consume(TokenType::IDENTIFIER, MSG("需要标识符", "Expected identifier."));
                if (check(TokenType::LPAREN))
                {
                    // 函数定义或函数原型
                    advance(); // 消耗 '('
                    std::vector<std::pair<std::string, DataType>> params;
                    // 接受 C 风格 (void) 作为空参数列表（兼容旧测试）
                    if (check(TokenType::VOID) && lexer_.peek().type == TokenType::RPAREN)
                    {
                        advance(); // 消耗 'void'
                    }
                    if (!check(TokenType::RPAREN))
                    {
                        params.push_back(parseParam());
                        while (match(TokenType::COMMA))
                        {
                            params.push_back(parseParam());
                        }
                    }
                    consume(TokenType::RPAREN, MSG("需要 ')'", "Expected ')'."));
                    if (match(TokenType::SEMICOLON))
                    {
                        // 函数原型（前向声明）：跳过，依赖语义分析的两遍扫描支持前向引用
                        // 实际定义将在后续解析时加入 program->functions
                    }
                    else
                    {
                        // 函数定义
                        auto body = parseBlock();
                        program->functions.push_back(
                            std::make_unique<FunctionDecl>(type, name.lexeme, std::move(params), std::move(body)));
                    }
                }
                else
                {
                    // 变量声明（初始化可选：全局变量可无初始化，C 语义默认 0）
                    std::unique_ptr<Expr> init = nullptr;
                    if (match(TokenType::ASSIGN))
                    {
                        init = parseExpression();
                    }
                    consume(TokenType::SEMICOLON, MSG("声明后需要 ';'", "Expected ';' after declaration."));
                    program->statements.push_back(
                        std::make_unique<VarDeclStmt>(type, name.lexeme, std::move(init), false));
                }
            }
            else
            {
                throw ParseError(MSG("顶层只允许声明或函数定义",
                                     "Only declarations or function definitions at top level."),
                                 currentToken_.line, currentToken_.column);
            }
        }
        return program;
    }

    // ================================================================
    //  声明
    // ================================================================

    std::unique_ptr<Stmt> Parser::parseDecl()
    {
        if (check(TokenType::CONST))
            return parseConstDecl();
        return parseVarDecl();
    }

    std::unique_ptr<Stmt> Parser::parseConstDecl()
    {
        // ConstDecl -> "const" "int" ID "=" Expr ";"
        consume(TokenType::CONST, MSG("需要 'const'", "Expected 'const'."));
        DataType type = parseType(); // 必须是 int
        Token name = consume(TokenType::IDENTIFIER, MSG("需要常量名", "Expected constant name."));
        consume(TokenType::ASSIGN, MSG("常量声明需要 '='", "Expected '=' in const declaration."));
        auto init = parseExpression();
        consume(TokenType::SEMICOLON, MSG("常量声明后需要 ';'", "Expected ';' after const declaration."));
        return std::make_unique<VarDeclStmt>(type, name.lexeme, std::move(init), true);
    }

    // ================================================================
    //  Param -> "int" ID
    // ================================================================

    std::pair<std::string, DataType> Parser::parseParam()
    {
        DataType type = parseType(); // 必须是 int
        Token name = consume(TokenType::IDENTIFIER, MSG("需要参数名", "Expected parameter name."));
        return {name.lexeme, type};
    }

    // ================================================================
    //  FuncDef -> ("int" | "void") ID "(" (Param ("," Param)*)? ")" Block
    //  (独立方法, 供外部调用; parseProgram 中已内联实现)
    // ================================================================

    std::unique_ptr<FunctionDecl> Parser::parseFuncDef()
    {
        // TODO: 如果需要从其他地方调用函数解析, 实现此方法
        //       当前 parseProgram 中已内联处理 FuncDef
        DataType returnType = parseType();
        Token name = consume(TokenType::IDENTIFIER, MSG("需要函数名", "Expected function name."));
        consume(TokenType::LPAREN, MSG("需要 '('", "Expected '('."));
        std::vector<std::pair<std::string, DataType>> params;
        // 接受 C 风格 (void) 作为空参数列表（兼容旧测试）
        if (check(TokenType::VOID) && lexer_.peek().type == TokenType::RPAREN)
        {
            advance(); // 消耗 'void'
        }
        if (!check(TokenType::RPAREN))
        {
            params.push_back(parseParam());
            while (match(TokenType::COMMA))
                params.push_back(parseParam());
        }
        consume(TokenType::RPAREN, MSG("需要 ')'", "Expected ')'."));
        auto body = parseBlock();
        return std::make_unique<FunctionDecl>(returnType, name.lexeme, std::move(params), std::move(body));
    }

    // ================================================================
    //  statement
    // ================================================================

    std::unique_ptr<Stmt> Parser::parseStatement()
    {
        if (check(TokenType::CONST))
            return parseConstDecl();
        if (check(TokenType::INT))
            return parseVarDecl();

        switch (currentToken_.type)
        {
        case TokenType::IF:
            return parseIfStmt();
        case TokenType::WHILE:
            return parseWhileStmt();
        case TokenType::BREAK:
            return parseBreakStmt();
        case TokenType::CONTINUE:
            return parseContinueStmt();
        case TokenType::RETURN:
            return parseReturnStmt();
        case TokenType::LBRACE:
            return parseBlock();
        case TokenType::SEMICOLON: // 空语句 ;
            advance();
            return std::make_unique<ExprStmt>(nullptr);
        case TokenType::IDENTIFIER:
            if (lexer_.peek().type == TokenType::ASSIGN)
                return parseAssignment();
            [[fallthrough]];
        default:
            auto expr = parseExpression();
            consume(TokenType::SEMICOLON, MSG("表达式后需要 ';'", "Expected ';' after expression."));
            return std::make_unique<ExprStmt>(std::move(expr));
        }
        return nullptr;
    }

    std::unique_ptr<Stmt> Parser::parseVarDecl()
    {
        // VarDecl -> "int" ID ("=" Expr)? ";"
        //   局部变量初始化可选（无初始化时默认 0）
        DataType type = parseType();
        Token name = consume(TokenType::IDENTIFIER, MSG("需要变量名", "Expected variable name."));
        std::unique_ptr<Expr> init = nullptr;
        if (match(TokenType::ASSIGN))
        {
            init = parseExpression();
        }
        consume(TokenType::SEMICOLON, MSG("变量声明后需要 ';'", "Expected ';' after variable declaration."));
        return std::make_unique<VarDeclStmt>(type, name.lexeme, std::move(init), false);
    }

    DataType Parser::parseType()
    {
        // 'int' | 'void'
        if (match(TokenType::INT))
            return DataType::INT;
        if (match(TokenType::VOID))
            return DataType::VOID;
        throw ParseError(MSG("需要类型 'int' 或 'void'", "Expected type 'int' or 'void'."),
                         currentToken_.line, currentToken_.column);
    }

    std::unique_ptr<Stmt> Parser::parseAssignment()
    {
        // TODO: Token name = consume(IDENTIFIER); consume(ASSIGN);
        //       auto value = parseExpression(); consume(SEMICOLON);
        Token name = consume(TokenType::IDENTIFIER, MSG("需要变量名", "Expected variable name."));
        consume(TokenType::ASSIGN, MSG("赋值需要 '='", "Expected '=' after variable name."));
        auto value = parseExpression();
        consume(TokenType::SEMICOLON, MSG("赋值后需要 ';'", "Expected ';' after assignment."));
        auto assign = std::make_unique<AssignExpr>(name.lexeme, std::move(value));
        return std::make_unique<ExprStmt>(std::move(assign));
    }

    std::unique_ptr<Stmt> Parser::parseIfStmt()
    {
        // TODO: consume(IF); consume(LPAREN); auto cond = parseExpression(); consume(RPAREN);
        //       auto thenB = parseStatement();
        //       unique_ptr<Stmt> elseB = nullptr;
        //       if (match(ELSE)) elseB = parseStatement();
        consume(TokenType::IF, MSG("需要 'if'", "Expected 'if'."));
        consume(TokenType::LPAREN, MSG("'if' 后需要 '('", "Expected '(' after 'if'."));
        auto cond = parseExpression();
        consume(TokenType::RPAREN, MSG("条件后需要 ')'", "Expected ')' after condition."));
        auto thenB = parseStatement();         // if 语句的 then 分支
        std::unique_ptr<Stmt> elseB = nullptr; // if 语句的 else 分支，默认为 nullptr
        if (match(TokenType::ELSE))
        {
            elseB = parseStatement();
        }
        return std::make_unique<IfStmt>(std::move(cond), std::move(thenB), std::move(elseB)); // 构造 IfStmt 节点，传入条件、then 分支和 else 分支
    }

    std::unique_ptr<Stmt> Parser::parseWhileStmt()
    {
        // TODO: consume(WHILE); consume(LPAREN); auto cond = parseExpression(); consume(RPAREN);
        //       auto body = parseStatement();
        consume(TokenType::WHILE, MSG("需要 'while'", "Expected 'while'."));
        consume(TokenType::LPAREN, MSG("'while' 后需要 '('", "Expected '(' after 'while'."));
        auto cond = parseExpression();
        consume(TokenType::RPAREN, MSG("条件后需要 ')'", "Expected ')' after condition."));
        auto body = parseStatement();
        return std::make_unique<WhileStmt>(std::move(cond), std::move(body)); // while 语句的条件和主体
    }

    std::unique_ptr<BlockStmt> Parser::parseBlock()
    { // 代码块由一系列语句组成，直到遇到右大括号 '}'
        // TODO: consume(LBRACE);
        //       while (!check(RBRACE) && !check(EOF)) { block->statements.push_back(parseStatement()); }
        //       consume(RBRACE);
        consume(TokenType::LBRACE, MSG("代码块需要 '{'", "Expected '{' to start block."));
        auto block = std::make_unique<BlockStmt>();
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN))
        {
            block->statements.push_back(parseStatement());
        } // 代码块内的语句解析完成后，必须遇到右大括号 '}' 来结束块
        /*{ int x = 1; x = x + 2; }
    BlockStmt
    ├── statements[0] → VarDeclStmt(int, "x", NumberLiteral(1))
    └── statements[1] → ExprStmt
                          └── AssignExpr("x", BinaryExpr(x, +, 2))*/
        consume(TokenType::RBRACE, MSG("代码块需要 '}'", "Expected '}' to end block."));
        return block;
    }

    std::unique_ptr<Stmt> Parser::parseBreakStmt()
    {
        // TODO: consume(BREAK); consume(SEMICOLON);
        consume(TokenType::BREAK, MSG("需要 'break'", "Expected 'break'."));
        consume(TokenType::SEMICOLON, MSG("'break' 后需要 ';'", "Expected ';' after 'break'."));
        return std::make_unique<BreakStmt>();
    }

    std::unique_ptr<Stmt> Parser::parseContinueStmt()
    {
        // TODO: consume(CONTINUE); consume(SEMICOLON);
        consume(TokenType::CONTINUE, MSG("需要 'continue'", "Expected 'continue'."));
        consume(TokenType::SEMICOLON, MSG("'continue' 后需要 ';'", "Expected ';' after 'continue'."));
        return std::make_unique<ContinueStmt>();
    }

    std::unique_ptr<Stmt> Parser::parseReturnStmt()
    {
        // TODO: consume(RETURN);
        //       if (!check(SEMICOLON)) auto val = parseExpression();
        //       consume(SEMICOLON);
        consume(TokenType::RETURN, MSG("需要 'return'", "Expected 'return'."));
        std::unique_ptr<Expr> value = nullptr;
        if (!check(TokenType::SEMICOLON))
        {
            value = parseExpression();
        }
        consume(TokenType::SEMICOLON, MSG("'return' 后需要 ';'", "Expected ';' after return."));
        return std::make_unique<ReturnStmt>(std::move(value));
    }

    // ================================================================
    //  表达式层级（函数调用深度 = 优先级高低）
    // ================================================================

    std::unique_ptr<Expr> Parser::parseExpression()
    {
        return parseAssignmentExpr(); // 最低优先级入口：赋值（右结合）
    }

    // AssignExpr -> LOrExpr ('=' AssignExpr)?
    //   右结合：支持 a = b = c
    //   左值必须是标识符（其它形式报错）
    std::unique_ptr<Expr> Parser::parseAssignmentExpr()
    {
        auto lhs = parseLogicalOr();
        if (check(TokenType::ASSIGN))
        {
            // 左值必须是标识符
            if (!dynamic_cast<IdentifierExpr *>(lhs.get()))
            {
                throw ParseError(MSG("赋值左值必须是变量",
                                     "Assignment target must be a variable."),
                                 currentToken_.line, currentToken_.column);
            }
            std::string name = static_cast<IdentifierExpr *>(lhs.get())->name;
            advance();                        // 消耗 '='
            auto rhs = parseAssignmentExpr(); // 右结合递归
            return std::make_unique<AssignExpr>(name, std::move(rhs));
        }
        return lhs;
    }

    // ====== 逻辑或 ||（优先级最低）======
    std::unique_ptr<Expr> Parser::parseLogicalOr()
    {
        // TODO: auto expr = parseLogicalAnd();
        //       while (check(OR)) { op = currentToken_.type; advance(); right = parseLogicalAnd(); expr = BinaryExpr(...); }
        auto expr = parseLogicalAnd();
        while (check(TokenType::OR))
        {
            TokenType op = currentToken_.type;
            advance();
            auto right = parseLogicalAnd();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    // LAndExpr -> RelExpr ( '&&' RelExpr )*
    std::unique_ptr<Expr> Parser::parseLogicalAnd()
    {
        auto expr = parseRelational();
        while (check(TokenType::AND))
        {
            TokenType op = currentToken_.type;
            advance();
            auto right = parseRelational();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    // RelExpr -> AddExpr ( ('<' | '>' | '<=' | '>=' | '==' | '!=') AddExpr )*
    // 将原来的 equality 和 comparison 合并为一个层级 (它们优先级相同)
    std::unique_ptr<Expr> Parser::parseRelational()
    {
        auto expr = parseAddition();
        while (check(TokenType::LT) || check(TokenType::LE) ||
               check(TokenType::GT) || check(TokenType::GE) ||
               check(TokenType::EQ) || check(TokenType::NEQ))
        {
            TokenType op = currentToken_.type;
            advance();
            auto right = parseAddition();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    std::unique_ptr<Expr> Parser::parseAddition()
    {
        // TODO: auto expr = parseMultiply();
        //       while (check(PLUS) || check(MINUS)) { ... }
        auto expr = parseMultiply();
        while (check(TokenType::PLUS) || check(TokenType::MINUS))
        {
            TokenType op = currentToken_.type;
            advance();
            auto right = parseMultiply();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    std::unique_ptr<Expr> Parser::parseMultiply()
    {
        // TODO: auto expr = parseUnary();
        //       while (check(STAR) || check(SLASH)) { ... }
        auto expr = parseUnary();
        while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::MOD))
        {
            TokenType op = currentToken_.type;
            advance();
            auto right = parseUnary();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    std::unique_ptr<Expr> Parser::parseUnary()
    {
        // TODO: if (check(MINUS) || check(NOT) || check(PLUS)) { op = ...; advance(); operand = parseUnary(); return UnaryExpr; }
        //       return parsePrimary();
        if (check(TokenType::MINUS) || check(TokenType::NOT) || check(TokenType::PLUS))
        {
            TokenType op = currentToken_.type;
            advance();
            auto operand = parseUnary();
            return std::make_unique<UnaryExpr>(op, std::move(operand));
        }
        return parsePrimary();
    }

    // PrimaryExpr -> ID | NUMBER | '(' Expr ')' | ID '(' (Expr (',' Expr)*)? ')'
    std::unique_ptr<Expr> Parser::parsePrimary()
    {
        if (match(TokenType::NUMBER))
        {
            // ⚠️ 使用 Lexer 已计算好的 intValue，而非 stoi(lexeme)
            //    因为 lexeme 可能是 "0xFF" / "0b101" 等非十进制形式
            return std::make_unique<NumberLiteral>(previousToken_.intValue);
        }
        if (match(TokenType::IDENTIFIER))
        {
            std::string idName = previousToken_.lexeme;
            // 检查是否是函数调用: ID "(" ...
            if (check(TokenType::LPAREN))
            {
                advance(); // 消耗 '('
                std::vector<std::unique_ptr<Expr>> args;
                if (!check(TokenType::RPAREN))
                {
                    args.push_back(parseExpression());
                    while (match(TokenType::COMMA))
                    {
                        args.push_back(parseExpression());
                    }
                }
                consume(TokenType::RPAREN, MSG("函数调用需要 ')'", "Expected ')' after arguments."));
                return std::make_unique<CallExpr>(idName, std::move(args));
            }
            return std::make_unique<IdentifierExpr>(idName);
        }
        if (match(TokenType::LPAREN))
        {
            auto expr = parseExpression();
            consume(TokenType::RPAREN, MSG("表达式后需要 ')'", "Expected ')' after expression."));
            return expr;
        }
        throw ParseError(MSG("需要表达式", "Expected expression."), currentToken_.line, currentToken_.column);
    }

} // namespace MyCompiler
