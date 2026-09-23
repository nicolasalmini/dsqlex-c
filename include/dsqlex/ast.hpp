#pragma once
#include <memory>
#include <string>
#include <vector>

namespace dsqlex {

enum class NodeKind {
    Select,
    NumberLit,
    StringLit,
    BoolLit,
    NullLit,
    Identifier,
    BinaryOp,
    CaseExpr,
    FunctionCall,
    InExpr,
    NotInExpr,
    LikeExpr,
    NotLikeExpr,
    UnaryOp,
};

enum class BinOp {
    Plus, Minus, Multiply, Divide,
    Eq, Neq, Lt, Gt, Lte, Gte,
    And, Or,
};

struct ASTNode;
using ASTPtr = std::unique_ptr<ASTNode>;

struct WhenClause {
    ASTPtr condition;
    ASTPtr result;
};

struct ASTNode {
    NodeKind kind;

    // Payload — only the relevant fields are set per kind.
    std::string text;             // NumberLit, StringLit, Identifier
    bool bool_val = false;        // BoolLit
    BinOp op{};                   // BinaryOp
    ASTPtr left;                  // BinaryOp left
    ASTPtr right;                 // BinaryOp right / LikeExpr pattern
    ASTPtr expr;                  // InExpr/NotInExpr/LikeExpr/NotLikeExpr subject / Select inner
    std::vector<ASTPtr> items;    // InExpr/NotInExpr list, FunctionCall args
    std::vector<WhenClause> whens;// CaseExpr
    ASTPtr else_clause;           // CaseExpr
    std::string func_name;        // FunctionCall

    // Convenience factories
    static ASTPtr make_select(ASTPtr inner);
    static ASTPtr make_number(const std::string& val);
    static ASTPtr make_string(const std::string& val);
    static ASTPtr make_bool(bool val);
    static ASTPtr make_null();
    static ASTPtr make_identifier(const std::string& name);
    static ASTPtr make_binary_op(BinOp op, ASTPtr left, ASTPtr right);
    static ASTPtr make_case(std::vector<WhenClause> whens, ASTPtr else_clause);
    static ASTPtr make_function(const std::string& name, std::vector<ASTPtr> args);
    static ASTPtr make_in(ASTPtr expr, std::vector<ASTPtr> items);
    static ASTPtr make_not_in(ASTPtr expr, std::vector<ASTPtr> items);
    static ASTPtr make_like(ASTPtr expr, ASTPtr pattern);
    static ASTPtr make_not_like(ASTPtr expr, ASTPtr pattern);
    static ASTPtr make_unary_op(BinOp op, ASTPtr operand);
};

} // namespace dsqlex
