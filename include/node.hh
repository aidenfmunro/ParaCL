#ifndef NODE_HH
#define NODE_HH

#include "context.hh"
#include "log.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace AST
{

using VarIterator = std::unordered_map<std::string, int>::iterator;

enum class BinaryOp
{
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    GR,
    LS,
    EQ,
    GR_EQ,
    LS_EQ,
    NOT_EQ,
    AND,
    OR,
};

enum class UnaryOp
{
    NEG,
    NOT,
};

class INode
{
  public:
    virtual void eval(detail::Context &ctx) const = 0;

    virtual ~INode() = default;
};

class StatementNode : public INode
{
};

class ExpressionNode : public StatementNode
{
  public:
    void eval(detail::Context &ctx) const override { eval_value(ctx); }

    virtual int eval_value(detail::Context &ctx) const = 0;
};

class ConditionalStatementNode : public StatementNode
{
};

using ExprPtr = ExpressionNode *;

using StmtPtr = StatementNode *;

using CondStmtPtr = ConditionalStatementNode *;

class ScopeNode final : public StatementNode
{
  private:
    std::vector<StmtPtr> children_;

  public:
    ScopeNode(std::vector<StmtPtr> &&stms)
        : children_(std::move(stms))
    {}

    void eval(detail::Context &ctx) const override
    {
        if (children_.empty())
            return;

        MSG("Evaluating scope\n");

        ++ctx.curScope_;

        ctx.varTables_.push_back(detail::Context::VarTable());

        LOG("ctx.curScope_ = {}\n", ctx.curScope_);
        LOG("ctx.varTables_ size = {}\n", ctx.varTables_.size());

        MSG("Scopes children:\n");
        for ([[maybe_unused]] const auto &child : children_)
        {
            LOG("{}\n", static_cast<const void *>(child));
        }

        for (const auto &child : children_)
        {
            LOG("Evaluating {}\n", static_cast<const void *>(child));
            child->eval(ctx);
        }

        ctx.varTables_.pop_back();

        --ctx.curScope_;

        LOG("ctx.curScope_ = {}\n", ctx.curScope_);
        LOG("ctx.varTables_ size = {}\n", ctx.varTables_.size());
    }

    void pushChild(StmtPtr stmt) { children_.push_back(stmt); }

    size_t nstms() const { return children_.size(); }
};

using ScopePtr = ScopeNode *;

class ConstantNode final : public ExpressionNode
{
  private:
    const int val_;

  public:
    ConstantNode(int val)
        : val_(val)
    {}

    int eval_value([[maybe_unused]] detail::Context &ctx) const override
    {
        LOG("Evaluating constant: {}\n", val_);

        return val_;
    }

    int getVal() const { return val_; }
};

class VariableNode final : public ExpressionNode
{
  private:
    std::string_view name_;

  public:
    VariableNode(std::string_view name)
        : name_(name)
    {}

    std::string_view getName() const { return name_; }

    int eval_value(detail::Context &ctx) const override
    {
        LOG("Evaluating variable: {}\n", name_);

        return ctx.getVarValue(name_);
    }
};

using VariablePtr = VariableNode *;

class BinaryOpNode final : public ExpressionNode
{
  private:
    ExprPtr left_{};
    ExprPtr right_{};
    BinaryOp op_{};

  public:
    BinaryOpNode(ExprPtr left, BinaryOp op, ExprPtr right)
        : left_(left)
        , right_(right)
        , op_(op)
    {}

    int eval_value(detail::Context &ctx) const override
    {
        MSG("Evaluating Binary Operation\n");

        int leftVal = left_->eval_value(ctx);
        int rightVal = right_->eval_value(ctx);

        int result = 0;

        switch (op_)
        {
            case BinaryOp::ADD:
                result = leftVal + rightVal;
                break;

            case BinaryOp::SUB:
                result = leftVal - rightVal;
                break;

            case BinaryOp::MUL:
                result = leftVal * rightVal;
                break;

            case BinaryOp::DIV:
                if (rightVal == 0)
                {
                    throw std::runtime_error("Divide by zero");
                }
                result = leftVal / rightVal;
                break;

            case BinaryOp::MOD:
                result = leftVal % rightVal;
                break;

            case BinaryOp::LS:
                result = leftVal < rightVal;
                break;

            case BinaryOp::GR:
                result = leftVal > rightVal;
                break;

            case BinaryOp::LS_EQ:
                result = leftVal <= rightVal;
                break;

            case BinaryOp::GR_EQ:
                result = leftVal >= rightVal;
                break;

            case BinaryOp::EQ:
                result = leftVal == rightVal;
                break;

            case BinaryOp::NOT_EQ:
                result = leftVal != rightVal;
                break;

            case BinaryOp::AND:
                result = leftVal && rightVal;
                break;

            case BinaryOp::OR:
                result = leftVal || rightVal;
                break;

            default:
                throw std::runtime_error("Unknown binary operation");
        }

        LOG("It's {}\n", result);

        return result;
    }
};

class UnaryOpNode final : public ExpressionNode
{
  private:
    ExprPtr operand_{};
    UnaryOp op_{};

  public:
    UnaryOpNode(ExprPtr operand, UnaryOp op)
        : operand_(operand)
        , op_(op)
    {}

    int eval_value(detail::Context &ctx) const override
    {
        int operandVal = operand_->eval_value(ctx);

        switch (op_)
        {
            case UnaryOp::NEG:
                return -operandVal;

            case UnaryOp::NOT:
                return !operandVal;

            default:
                throw std::runtime_error("Unknown unary operation");
        }
    }
};

class AssignNode final : public ExpressionNode
{
  private:
    VariablePtr dest_{};
    ExprPtr expr_{};

  public:
    AssignNode(VariablePtr dest, ExprPtr expr)
        : dest_(dest)
        , expr_(expr)
    {}

    int eval_value(detail::Context &ctx) const override
    {
        MSG("Evaluating assignment\n");

        MSG("Getting assigned value\n");
        int value = expr_->eval_value(ctx);
        LOG("Assigned value is {}\n", value);

        std::string_view destName = dest_->getName();

        ctx.get_variable(destName) = value;

        return value;
    }
};

class WhileNode final : public ConditionalStatementNode
{
  private:
    ExprPtr cond_{};
    StmtPtr scope_{};

  public:
    WhileNode(ExprPtr cond, StmtPtr scope)
        : cond_(cond)
        , scope_(scope)
    {}

    void eval(detail::Context &ctx) const override
    {
        while (cond_->eval_value(ctx))
        {
            scope_->eval(ctx);
        }
    }
};

class IfElseNode final : public StatementNode
{
  private:
    ExprPtr cond_{};
    StmtPtr action_{};
    StmtPtr alt_action_{};

  public:
    IfElseNode(ExprPtr cond, StmtPtr action)
        : cond_(cond)
        , action_(action)
    {}

    IfElseNode(ExprPtr cond, StmtPtr action, StmtPtr alt_action)
        : cond_(cond)
        , action_(action)
        , alt_action_(alt_action)
    {}

    IfElseNode(StmtPtr action)
        : action_(action)
    {}

    void eval(detail::Context &ctx) const override
    {
        if (!cond_)
        {
            // if there is no condition do it
            action_->eval(ctx);
            return;
        }

        if (cond_->eval_value(ctx))
        {
            action_->eval(ctx);
        }
        else
        {
            if (alt_action_)
                alt_action_->eval(ctx);
        }
    }
};

class PrintNode final : public StatementNode
{
  private:
    ExprPtr expr_{};

  public:
    PrintNode(ExprPtr expr)
        : expr_(expr)
    {}

    void eval(detail::Context &ctx) const override
    {
        MSG("Evaluation print\n");

        int value = expr_->eval_value(ctx);

        ctx.out << value << '\n';
    }
};

class InNode final : public ExpressionNode
{
  public:
    int eval_value([[maybe_unused]] detail::Context &ctx) const override
    {
        int value = 0;

        std::cin >> value;

        if (!std::cin.good())
        {
            throw std::runtime_error("Incorrect input");
        }

        return value;
    }
};

} // namespace AST

#endif // ! NODE_HH
