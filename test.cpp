#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <memory>
#include <algorithm>

// --- Tokens & AST Definitions ---
// Note: This implementation focuses on simplicity by using C++ smart pointers 
// (std::unique_ptr) and classes, fulfilling the core compiler requirements.
enum class TokenType{INC,PRINT,ASSIGN,SEMICOLON,LPAREN,RPAREN,NUMBER,IDENTIFIER,END_OF_FILE,UNKNOWN};
struct Token{TokenType type;std::string lexeme;int line;};
struct ASTNode{virtual ~ASTNode()=default;};
struct Expr:public ASTNode{};
struct NumberExpr:public Expr{int value;NumberExpr(int val):value(val){}};
struct IdentifierExpr:public Expr{std::string name;IdentifierExpr(const std::string& n):name(n){}};
struct IncCallExpr:public Expr{std::unique_ptr<Expr> argument;IncCallExpr(std::unique_ptr<Expr> arg):argument(std::move(arg)){}};
struct Stmt:public ASTNode{};
struct VarDeclStmt:public Stmt{std::string var_name;std::unique_ptr<NumberExpr> initial_value;VarDeclStmt(const std::string& name,std::unique_ptr<NumberExpr> value):var_name(name),initial_value(std::move(value)){}};
struct PrintStmt:public Stmt{std::unique_ptr<Expr> expression;PrintStmt(std::unique_ptr<Expr> expr):expression(std::move(expr)){}};
struct Program:public ASTNode{std::vector<std::unique_ptr<Stmt>> statements;};

// --- Lexer (Scanner) ---
class Lexer{
private:
    std::string source;int current_pos=0;int line_num=1;
    std::map<std::string,TokenType> keywords={{"inc",TokenType::INC},{"print",TokenType::PRINT}};
    char advance(){return(current_pos<source.length())?source[current_pos++]:'\0';}
    char peek()const{return(current_pos<source.length())?source[current_pos]:'\0';}
    void skipWhitespace(){while(current_pos<source.length()){char c=peek();if(c==' '||c=='\t'||c=='\r'){advance();}else if(c=='\n'){line_num++;advance();}else{break;}}}
    Token scanIdentifier(){std::string lexeme;while(std::isalpha(peek())||std::isdigit(peek())||peek()=='_'){lexeme+=advance();}if(keywords.count(lexeme)){return{keywords[lexeme],lexeme,line_num};}return{TokenType::IDENTIFIER,lexeme,line_num};}
    Token scanNumber(){std::string lexeme;while(std::isdigit(peek())){lexeme+=advance();}return{TokenType::NUMBER,lexeme,line_num};}
public:
    Lexer(const std::string& src):source(src){}
    Token nextToken(){
        skipWhitespace();if(current_pos>=source.length()){return{TokenType::END_OF_FILE,"",line_num};}char c=advance();
        if(std::isalpha(c)){current_pos--;return scanIdentifier();}if(std::isdigit(c)){current_pos--;return scanNumber();}
        switch(c){case'=':return{TokenType::ASSIGN,"=",line_num};case';':return{TokenType::SEMICOLON,";",line_num};case'(':return{TokenType::LPAREN,"(",line_num};case')':return{TokenType::RPAREN,")",line_num};default:return{TokenType::UNKNOWN,std::string(1,c),line_num};}
    }
};

// --- Parser (Syntax Analysis) ---
class Parser{
private:
    Lexer& lexer;Token current_token;
    void advance(){current_token=lexer.nextToken();}bool check(TokenType type)const{return current_token.type==type;}
    Token consume(TokenType expected_type,const std::string& msg){if(check(expected_type)){Token t=current_token;advance();return t;}throw std::runtime_error("Syntax Error: "+msg+" (Found '"+current_token.lexeme+"') at line "+std::to_string(current_token.line));}
    std::unique_ptr<IncCallExpr> parseIncCall(){consume(TokenType::INC,"Expected 'inc'");consume(TokenType::LPAREN,"Expected '('");std::unique_ptr<Expr> arg=parseExpr();consume(TokenType::RPAREN,"Expected ')'");return std::make_unique<IncCallExpr>(std::move(arg));}
    std::unique_ptr<Expr> parseExpr(){
        if(check(TokenType::NUMBER)){Token t=consume(TokenType::NUMBER,"Expected number");return std::make_unique<NumberExpr>(std::stoi(t.lexeme));}
        if(check(TokenType::IDENTIFIER)){return std::make_unique<IdentifierExpr>(consume(TokenType::IDENTIFIER,"Expected identifier").lexeme);}
        if(check(TokenType::INC)){return parseIncCall();}
        throw std::runtime_error("Syntax Error: Expected expression");
    }
    std::unique_ptr<VarDeclStmt> parseVarDecl(){Token name=consume(TokenType::IDENTIFIER,"Expected name");consume(TokenType::ASSIGN,"Expected '='");Token val=consume(TokenType::NUMBER,"Expected value");consume(TokenType::SEMICOLON,"Expected ';'");return std::make_unique<VarDeclStmt>(name.lexeme,std::make_unique<NumberExpr>(std::stoi(val.lexeme)));}
    std::unique_ptr<PrintStmt> parsePrintStmt(){consume(TokenType::PRINT,"Expected 'print'");consume(TokenType::LPAREN,"Expected '('");std::unique_ptr<Expr> expr=parseExpr();consume(TokenType::RPAREN,"Expected ')'");consume(TokenType::SEMICOLON,"Expected ';'");return std::make_unique<PrintStmt>(std::move(expr));}
    std::unique_ptr<Stmt> parseStatement(){if(check(TokenType::IDENTIFIER)){return parseVarDecl();}if(check(TokenType::PRINT)){return parsePrintStmt();}throw std::runtime_error("Syntax Error: Expected statement");}
public:
    Parser(Lexer& lex):lexer(lex){advance();}
    std::unique_ptr<Program> parse(){auto p=std::make_unique<Program>();while(!check(TokenType::END_OF_FILE)){p->statements.push_back(parseStatement());}return p;}
};

// --- Semantic Analyzer (Type & Declaration Check) ---
class SemanticAnalyzer{
private:
    std::map<std::string,bool> symbol_table; 
    void analyzeExpr(Expr* expr){
        if(!expr)return;
        // Note on Optimization (O1): Constant folding is not implemented here. 
        // Optimization is set to O0 (No optimization - Base Requirement).
        if(IdentifierExpr* id=dynamic_cast<IdentifierExpr*>(expr)){if(symbol_table.find(id->name)==symbol_table.end()){throw std::runtime_error("Semantic Error: Variable '"+id->name+"' is undeclared.");}}
        else if(IncCallExpr* inc=dynamic_cast<IncCallExpr*>(expr)){analyzeExpr(inc->argument.get());}
    }
    void analyzeStmt(Stmt* stmt){
        if(!stmt)return;
        if(VarDeclStmt* decl=dynamic_cast<VarDeclStmt*>(stmt)){symbol_table[decl->var_name]=true;}
        else if(PrintStmt* print=dynamic_cast<PrintStmt*>(stmt)){analyzeExpr(print->expression.get());}
    }
public:
    void analyze(Program* program){std::cout<<"\n--- Starting Semantic Analysis (O0) ---\n";if(!program)return;for(const auto& stmt:program->statements){analyzeStmt(stmt.get());}std::cout<<"Semantic analysis passed successfully.\n";}
};

// --- Interpreter (Execution) ---
class Interpreter{
private:
    std::map<std::string,int> memory; 
    int evaluateExpr(Expr* expr){
        if(!expr)throw std::runtime_error("Runtime Error: Null expression.");
        if(NumberExpr* num=dynamic_cast<NumberExpr*>(expr)){return num->value;}
        if(IdentifierExpr* id=dynamic_cast<IdentifierExpr*>(expr)){if(memory.find(id->name)==memory.end()){throw std::runtime_error("Runtime Error: Variable '"+id->name+"' used before assignment.");}return memory.at(id->name);}
        if(IncCallExpr* inc=dynamic_cast<IncCallExpr*>(expr)){return evaluateExpr(inc->argument.get())+1;}
        throw std::runtime_error("Runtime Error: Unknown expression type.");
    }
    void executeStmt(Stmt* stmt){
        if(!stmt)return;
        if(VarDeclStmt* decl=dynamic_cast<VarDeclStmt*>(stmt)){memory[decl->var_name]=decl->initial_value->value;} 
        else if(PrintStmt* print=dynamic_cast<PrintStmt*>(stmt)){std::cout<<"Output: "<<evaluateExpr(print->expression.get())<<"\n";}
    }
public:
    void interpret(Program* program){
        // Note on Intermediate Representation (IR): 
        // This interpreter uses Direct AST Interpretation, skipping the optional 
        // Three-Address Code (TAC) generation for simplicity.
        std::cout<<"\n--- Starting Code Execution (Direct AST Interpretation) ---\n";
        if(!program)return;for(const auto& stmt:program->statements){executeStmt(stmt.get());}std::cout<<"Execution finished successfully.\n";
    }
};

// --- Main Execution and Tests ---
void run_test(const std::string& name,const std::string& code){
    std::cout<<"\n==========================================\nTEST: "<<name<<"\n==========================================\nSource Code:\n"<<code<<"\n";
    try{
        Lexer lexer(code);Parser parser(lexer);std::unique_ptr<Program> ast=parser.parse();
        SemanticAnalyzer analyzer;analyzer.analyze(ast.get());
        Interpreter interpreter;interpreter.interpret(ast.get());
    }catch(const std::exception& e){std::cerr<<"\n[Caught Expected Error] "<<e.what()<<std::endl;}
}

int main(){
    std::string valid_code=R"(x=10;print(inc(x));print(inc(15));)";
    run_test("VALID Program (Expected: 11, 16)", valid_code);

    std::string invalid_semantic_code=R"(a=1;print(inc(y));)";
    run_test("INVALID Program (Undeclared Var)", invalid_semantic_code);

    std::string invalid_syntax_code=R"(print(inc());)";
    run_test("INVALID Program (Syntax Error)", invalid_syntax_code);
    
    return 0;
}