/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_CONJURE_COMPILER_HPP)
#define BOOST_SPIRIT_CONJURE_COMPILER_HPP

#include "ast.hpp"
#include "error_handler.hpp"
#include "vm.hpp"
#include <map>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/function.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <llvm/DerivedTypes.h>
#include <llvm/Constants.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/IRBuilder.h>

namespace client { namespace code_gen
{
    unsigned const int_size = 32;
    struct compiler;
    struct llvm_compiler;

    ///////////////////////////////////////////////////////////////////////////
    //  The Value (light abstraction of an LLVM::Value)
    ///////////////////////////////////////////////////////////////////////////
    struct value
    {
        value();
        value(value const& rhs);

        value& operator=(value const& rhs);
        bool is_lvalue() const;
        bool is_valid() const;
        operator bool() const;

        value& assign(value const& rhs);

        void name(char const* id);
        void name(std::string const& id);

        friend value operator-(value a);
        friend value operator!(value a);
        friend value operator+(value a, value b);
        friend value operator-(value a, value b);
        friend value operator*(value a, value b);
        friend value operator/(value a, value b);
        friend value operator%(value a, value b);

        friend value operator&(value a, value b);
        friend value operator|(value a, value b);
        friend value operator^(value a, value b);
        friend value operator<<(value a, value b);
        friend value operator>>(value a, value b);

        friend value operator==(value a, value b);
        friend value operator!=(value a, value b);
        friend value operator<(value a, value b);
        friend value operator<=(value a, value b);
        friend value operator>(value a, value b);
        friend value operator>=(value a, value b);

    private:

        struct to_llvm_value;
        friend struct to_llvm_value;
        friend struct llvm_compiler;

        value(
            llvm::Value* v,
            bool is_lvalue_,
            llvm::IRBuilder<>* builder);

        llvm::LLVMContext& context() const
        { return llvm::getGlobalContext(); }

        operator llvm::Value*() const;

        llvm::Value* v;
        bool is_lvalue_;
        llvm::IRBuilder<>* builder;
    };

    ///////////////////////////////////////////////////////////////////////////
    //  The Basic Block (light abstraction of an LLVM::BasicBlock)
    ///////////////////////////////////////////////////////////////////////////
    struct function;

    struct basic_block
    {
        basic_block()
          : b(0) {}

        bool has_terminator() const;
        bool is_valid() const;

    private:

        basic_block(llvm::BasicBlock* b)
          : b(b) {}

        operator llvm::BasicBlock*() const
        { return b; }

        friend struct llvm_compiler;
        friend struct function;
        llvm::BasicBlock* b;
    };

    ///////////////////////////////////////////////////////////////////////////
    //  The Function (light abstraction of an LLVM::Function)
    ///////////////////////////////////////////////////////////////////////////
    struct llvm_compiler;

    struct function
    {
    private:

        struct to_value;
        typedef llvm::Function::arg_iterator arg_iterator;
        typedef boost::transform_iterator<
            to_value, arg_iterator>
        arg_val_iterator;

    public:

        typedef boost::iterator_range<arg_val_iterator> arg_range;

        function()
          : f(0), c(c) {}

        std::string name() const;

        std::size_t arg_size() const;
        arg_range args() const;

        void add(basic_block const& b);
        void erase_from_parent();
        basic_block last_block();
        bool empty() const;

        bool is_valid() const;
        void verify() const;

    private:

        function(llvm::Function* f, llvm_compiler* c)
          : f(f), c(c) {}

        operator llvm::Function*() const;

        friend struct llvm_compiler;
        llvm::Function* f;
        llvm_compiler* c;
    };

    ///////////////////////////////////////////////////////////////////////////
    //  The LLVM Compiler. Lower level compiler (does not deal with ASTs)
    ///////////////////////////////////////////////////////////////////////////
    struct llvm_compiler
    {
        llvm_compiler(vmachine& vm)
          : llvm_builder(context())
          , vm(vm)
          , fpm(vm.module())
        { init_fpm(); }

        value val() { return value(); }
        value val(unsigned int x);
        value val(int x);
        value val(bool x);

        value var(char const* name);
        value var(std::string const& name);

        template <typename Container>
        value call(function callee, Container const& args);

        function get_function(char const* name);
        function get_function(std::string const& name);
        function get_current_function();

        function declare_function(
            bool void_return
          , std::string const& name
          , std::size_t nargs);

        basic_block make_basic_block(
            char const* name
          , function parent = function()
          , basic_block before = basic_block());

        basic_block get_insert_block();
        void set_insert_point(basic_block b);

        void conditional_branch(
            value cond, basic_block true_br, basic_block false_br);
        void branch(basic_block b);

        void return_();
        void return_(value v);

        void optimize_function(function f);

    protected:

        llvm::LLVMContext& context() const
        { return llvm::getGlobalContext(); }

        llvm::IRBuilder<>& builder()
        { return llvm_builder; }

    private:

        friend struct function::to_value;

        value val(llvm::Value* v);

        template <typename C>
        llvm::Value* call_impl(
            function callee,
            C const& args);

        void init_fpm();
        llvm::IRBuilder<> llvm_builder;
        vmachine& vm;
        llvm::FunctionPassManager fpm;
    };

    ///////////////////////////////////////////////////////////////////////////
    //  The main compiler. Generates code from our AST.
    ///////////////////////////////////////////////////////////////////////////
    struct compiler : llvm_compiler
    {
        typedef value result_type;

        template <typename ErrorHandler>
        compiler(vmachine& vm, ErrorHandler& error_handler_)
          : llvm_compiler(vm)
        {
            using namespace boost::phoenix::arg_names;
            namespace phx = boost::phoenix;
            using boost::phoenix::function;

            error_handler = function<ErrorHandler>(error_handler_)(
                "Error! ", _2, phx::cref(error_handler_.iters)[_1]);
        }

        value operator()(ast::nil) { BOOST_ASSERT(0); return val(); }
        value operator()(unsigned int x);
        value operator()(bool x);
        value operator()(ast::primary_expr const& x);
        value operator()(ast::identifier const& x);
        value operator()(ast::unary_expr const& x);
        value operator()(ast::function_call const& x);
        value operator()(ast::expression const& x);
        value operator()(ast::assignment const& x);

        bool operator()(ast::variable_declaration const& x);
        bool operator()(ast::statement_list const& x);
        bool operator()(ast::statement const& x);
        bool operator()(ast::if_statement const& x);
        bool operator()(ast::while_statement const& x);
        bool operator()(ast::return_statement const& x);
        bool operator()(ast::function const& x);
        bool operator()(ast::function_list const& x);

    private:

        value compile_binary_expression(
            value lhs, value rhs, token_ids::type op);

        value compile_expression(
            int min_precedence,
            value lhs,
            std::list<ast::operation>::const_iterator& rest_begin,
            std::list<ast::operation>::const_iterator rest_end);

        struct statement_compiler;
        statement_compiler& as_statement();

        function function_decl(ast::function const& x);
        void function_allocas(ast::function const& x, function function);

        boost::function<
            void(int tag, std::string const& what)>
        error_handler;

        bool void_return;
        std::string current_function_name;
        std::map<std::string, value> locals;
        basic_block return_block;
        value return_var;
    };
}}

#endif
