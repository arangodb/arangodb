// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include "autodiff.h"

#include <iostream>

#include <boost/yap/algorithm.hpp>
#include <boost/polymorphic_cast.hpp>
#include <boost/hana/for_each.hpp>

#define BOOST_TEST_MODULE autodiff_test
#include <boost/test/included/unit_test.hpp>


double const Epsilon = 10.0e-6;
#define CHECK_CLOSE(A,B) do { BOOST_CHECK_CLOSE(A,B,Epsilon); } while(0)

using namespace AutoDiff;

//[ autodiff_expr_template_decl
template <boost::yap::expr_kind Kind, typename Tuple>
struct autodiff_expr
{
    static boost::yap::expr_kind const kind = Kind;

    Tuple elements;
};

BOOST_YAP_USER_UNARY_OPERATOR(negate, autodiff_expr, autodiff_expr)

BOOST_YAP_USER_BINARY_OPERATOR(plus, autodiff_expr, autodiff_expr)
BOOST_YAP_USER_BINARY_OPERATOR(minus, autodiff_expr, autodiff_expr)
BOOST_YAP_USER_BINARY_OPERATOR(multiplies, autodiff_expr, autodiff_expr)
BOOST_YAP_USER_BINARY_OPERATOR(divides, autodiff_expr, autodiff_expr)
//]

//[ autodiff_expr_literals_decl
namespace autodiff_placeholders {

    // This defines a placeholder literal operator that creates autodiff_expr
    // placeholders.
    BOOST_YAP_USER_LITERAL_PLACEHOLDER_OPERATOR(autodiff_expr)

}
//]

//[ autodiff_function_terminals
template <OPCODE Opcode>
struct autodiff_fn_expr :
    autodiff_expr<boost::yap::expr_kind::terminal, boost::hana::tuple<OPCODE>>
{
    autodiff_fn_expr () :
        autodiff_expr {boost::hana::tuple<OPCODE>{Opcode}}
    {}

    BOOST_YAP_USER_CALL_OPERATOR_N(::autodiff_expr, 1);
};

// Someone included <math.h>, so we have to add trailing underscores.
autodiff_fn_expr<OP_SIN> const sin_;
autodiff_fn_expr<OP_COS> const cos_;
autodiff_fn_expr<OP_SQRT> const sqrt_;
//]

//[ autodiff_xform
struct xform
{
    // Create a var-node for each placeholder when we see it for the first
    // time.
    template <long long I>
    Node * operator() (boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
                       boost::yap::placeholder<I>)
    {
        if (list_.size() < I)
            list_.resize(I);
        auto & retval = list_[I - 1];
        if (retval == nullptr)
            retval = create_var_node();
        return retval;
    }

    // Create a param-node for every numeric terminal in the expression.
    Node * operator() (boost::yap::expr_tag<boost::yap::expr_kind::terminal>, double x)
    { return create_param_node(x); }

    // Create a "uary" node for each call expression, using its OPCODE.
    template <typename Expr>
    Node * operator() (boost::yap::expr_tag<boost::yap::expr_kind::call>,
                       OPCODE opcode, Expr const & expr)
    {
        return create_uary_op_node(
            opcode,
            boost::yap::transform(boost::yap::as_expr<autodiff_expr>(expr), *this)
        );
    }

    template <typename Expr>
    Node * operator() (boost::yap::expr_tag<boost::yap::expr_kind::negate>,
                       Expr const & expr)
    {
        return create_uary_op_node(
            OP_NEG,
            boost::yap::transform(boost::yap::as_expr<autodiff_expr>(expr), *this)
        );
    }

    // Define a mapping from binary arithmetic expr_kind to OPCODE...
    static OPCODE op_for_kind (boost::yap::expr_kind kind)
    {
        switch (kind) {
        case boost::yap::expr_kind::plus: return OP_PLUS;
        case boost::yap::expr_kind::minus: return OP_MINUS;
        case boost::yap::expr_kind::multiplies: return OP_TIMES;
        case boost::yap::expr_kind::divides: return OP_DIVID;
        default: assert(!"This should never execute"); return OPCODE{};
        }
        assert(!"This should never execute");
        return OPCODE{};
    }

    // ... and use it to handle all the binary arithmetic operators.
    template <boost::yap::expr_kind Kind, typename Expr1, typename Expr2>
    Node * operator() (boost::yap::expr_tag<Kind>, Expr1 const & expr1, Expr2 const & expr2)
    {
        return create_binary_op_node(
            op_for_kind(Kind),
            boost::yap::transform(boost::yap::as_expr<autodiff_expr>(expr1), *this),
            boost::yap::transform(boost::yap::as_expr<autodiff_expr>(expr2), *this)
        );
    }

    vector<Node *> & list_;
};
//]

//[ autodiff_to_node
template <typename Expr, typename ...T>
Node * to_auto_diff_node (Expr const & expr, vector<Node *> & list, T ... args)
{
    Node * retval = nullptr;

    // This fills in list as a side effect.
    retval = boost::yap::transform(expr, xform{list});

    assert(list.size() == sizeof...(args));

    // Fill in the values of the value-nodes in list with the "args"
    // parameter pack.
    auto it = list.begin();
    boost::hana::for_each(
        boost::hana::make_tuple(args ...),
        [&it](auto x) {
            Node * n = *it;
            VNode * v = boost::polymorphic_downcast<VNode *>(n);
            v->val = x;
            ++it;
        }
    );

    return retval;
}
//]

struct F{
	F() {	AutoDiff::autodiff_setup();	}
	~F(){	AutoDiff::autodiff_cleanup();	}
};


BOOST_FIXTURE_TEST_SUITE(all, F)

//[ autodiff_original_node_builder
Node* build_linear_fun1_manually(vector<Node*>& list)
{
	//f(x1,x2,x3) = -5*x1+sin(10)*x1+10*x2-x3/6
	PNode* v5 = create_param_node(-5);
	PNode* v10 = create_param_node(10);
	PNode* v6 = create_param_node(6);
	VNode*	x1 = create_var_node();
	VNode*	x2 = create_var_node();
	VNode*	x3 = create_var_node();

	OPNode* op1 = create_binary_op_node(OP_TIMES,v5,x1);   //op1 = v5*x1
	OPNode* op2 = create_uary_op_node(OP_SIN,v10);         //op2 = sin(v10)
	OPNode* op3 = create_binary_op_node(OP_TIMES,op2,x1);  //op3 = op2*x1
	OPNode* op4 = create_binary_op_node(OP_PLUS,op1,op3);  //op4 = op1 + op3
	OPNode*	op5 = create_binary_op_node(OP_TIMES,v10,x2);  //op5 = v10*x2
	OPNode* op6 = create_binary_op_node(OP_PLUS,op4,op5);  //op6 = op4+op5
	OPNode* op7 = create_binary_op_node(OP_DIVID,x3,v6);   //op7 = x3/v6
	OPNode* op8 = create_binary_op_node(OP_MINUS,op6,op7); //op8 = op6 - op7
	x1->val = -1.9;
	x2->val = 2;
	x3->val = 5./6.;
	list.push_back(x1);
	list.push_back(x2);
	list.push_back(x3);
	return op8;
}
//]

//[ autodiff_yap_node_builder
Node* build_linear_fun1(vector<Node*>& list)
{
    //f(x1,x2,x3) = -5*x1+sin(10)*x1+10*x2-x3/6
    using namespace autodiff_placeholders;
    return to_auto_diff_node(
        -5 * 1_p + sin_(10) * 1_p + 10 * 2_p - 3_p / 6,
        list,
        -1.9,
        2,
        5./6.
    );
}
//]

Node* build_linear_function2_manually(vector<Node*>& list)
{
	//f(x1,x2,x3) = -5*x1+-10*x1+10*x2-x3/6
	PNode* v5 = create_param_node(-5);
	PNode* v10 = create_param_node(10);
	PNode* v6 = create_param_node(6);
	VNode*	x1 = create_var_node();
	VNode*	x2 = create_var_node();
	VNode*	x3 = create_var_node();
	list.push_back(x1);
	list.push_back(x2);
	list.push_back(x3);
	OPNode* op1 = create_binary_op_node(OP_TIMES,v5,x1); //op1 = v5*x1
	OPNode* op2 = create_uary_op_node(OP_NEG,v10);       //op2 = -v10
	OPNode* op3 = create_binary_op_node(OP_TIMES,op2,x1);//op3 = op2*x1
	OPNode* op4 = create_binary_op_node(OP_PLUS,op1,op3);//op4 = op1 + op3
	OPNode*	op5 = create_binary_op_node(OP_TIMES,v10,x2);//op5 = v10*x2
	OPNode* op6 = create_binary_op_node(OP_PLUS,op4,op5);//op6 = op4+op5
	OPNode* op7 = create_binary_op_node(OP_DIVID,x3,v6); //op7 = x3/v6
	OPNode* op8 = create_binary_op_node(OP_MINUS,op6,op7);//op8 = op6 - op7
	x1->val = -1.9;
	x2->val = 2;
	x3->val = 5./6.;
	return op8;
}

Node* build_linear_function2(vector<Node*>& list)
{
    //f(x1,x2,x3) = -5*x1+-10*x1+10*x2-x3/6
    using namespace autodiff_placeholders;
    auto ten = boost::yap::make_terminal<autodiff_expr>(10);
    return to_auto_diff_node(
        -5 * 1_p + -ten * 1_p + 10 * 2_p - 3_p / 6,
        list,
        -1.9,
        2,
        5./6.
    );
}

Node* build_nl_function1_manually(vector<Node*>& list)
{
//	(x1*x2 * sin(x1))/x3 + x2*x4 - x1/x2
	VNode* x1 = create_var_node();
	VNode* x2 = create_var_node();
	VNode* x3 = create_var_node();
	VNode* x4 = create_var_node();
	x1->val = -1.23;
	x2->val = 7.1231;
	x3->val = 2;
	x4->val = -10;
	list.push_back(x1);
	list.push_back(x2);
	list.push_back(x3);
	list.push_back(x4);

	OPNode* op1 = create_binary_op_node(OP_TIMES,x2,x1);
	OPNode* op2 = create_uary_op_node(OP_SIN,x1);
	OPNode* op3 = create_binary_op_node(OP_TIMES,op1,op2);
	OPNode* op4 = create_binary_op_node(OP_DIVID,op3,x3);
	OPNode* op5 = create_binary_op_node(OP_TIMES,x2,x4);
	OPNode* op6 = create_binary_op_node(OP_PLUS,op4,op5);
	OPNode* op7 = create_binary_op_node(OP_DIVID,x1,x2);
	OPNode* op8 = create_binary_op_node(OP_MINUS,op6,op7);
	return op8;
}

Node* build_nl_function1(vector<Node*>& list)
{
    // (x1*x2 * sin(x1))/x3 + x2*x4 - x1/x2
    using namespace autodiff_placeholders;
    return to_auto_diff_node(
        (1_p * 2_p * sin_(1_p)) / 3_p + 2_p * 4_p - 1_p / 2_p,
        list,
	-1.23,
	7.1231,
	2,
	-10
    );
}

BOOST_AUTO_TEST_CASE( test_linear_fun1 )
{
	BOOST_TEST_MESSAGE("test_linear_fun1");
	vector<Node*> list;
	Node* root = build_linear_fun1(list);
	vector<double> grad;
	double val1 = grad_reverse(root,list,grad);
	double val2 = eval_function(root);
	double x1g[] = {-5.5440211108893697744548489936278,10.0,-0.16666666666666666666666666666667};

	for(unsigned int i=0;i<3;i++){
		CHECK_CLOSE(grad[i],x1g[i]);
	}

	double eval = 30.394751221800913;

	CHECK_CLOSE(val1,eval);
	CHECK_CLOSE(val2,eval);


	EdgeSet s;
	nonlinearEdges(root,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,0);
}

BOOST_AUTO_TEST_CASE( test_grad_sin )
{
	BOOST_TEST_MESSAGE("test_grad_sin");
	VNode* x1 = create_var_node();
	x1->val = 10;
	OPNode* root = create_uary_op_node(OP_SIN,x1);
	vector<Node*> nodes;
	nodes.push_back(x1);
	vector<double> grad;
	grad_reverse(root,nodes,grad);
	double x1g =  -0.83907152907645244;
	//the matlab give cos(10) = -0.839071529076452

	CHECK_CLOSE(grad[0],x1g);
	BOOST_CHECK_EQUAL(nodes.size(),1);

	EdgeSet s;
	nonlinearEdges(root,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,1);
}

BOOST_AUTO_TEST_CASE(test_grad_single_node)
{
	VNode* x1 = create_var_node();
	x1->val = -2;
	vector<Node*> nodes;
	nodes.push_back(x1);
	vector<double> grad;
	double val = grad_reverse(x1,nodes,grad);
	CHECK_CLOSE(grad[0],1);
	CHECK_CLOSE(val,-2);

	EdgeSet s;
	unsigned int n = 0;
	nonlinearEdges(x1,s);
	n = nzHess(s);
	BOOST_CHECK_EQUAL(n,0);

	grad.clear();
	nodes.clear();
	PNode* p = create_param_node(-10);
	//OPNode*	op = create_binary_op_node(TIMES,p,create_param_node(2));
	val = grad_reverse(p,nodes,grad);
	BOOST_CHECK_EQUAL(grad.size(),0);
	CHECK_CLOSE(val,-10);

	s.clear();
	nonlinearEdges(p,s);
	n = nzHess(s);
	BOOST_CHECK_EQUAL(n,0);
}

BOOST_AUTO_TEST_CASE(test_grad_neg)
{
	VNode* x1 = create_var_node();
	x1->val = 10;
	PNode* p2 = create_param_node(-1);
	vector<Node*> nodes;
	vector<double> grad;
	nodes.push_back(x1);
	Node* root = create_binary_op_node(OP_TIMES,x1,p2);
	grad_reverse(root,nodes,grad);
	CHECK_CLOSE(grad[0],-1);
	BOOST_CHECK_EQUAL(nodes.size(),1);
	nodes.clear();
	grad.clear();
	nodes.push_back(x1);
	root = create_uary_op_node(OP_NEG,x1);
	grad_reverse(root,nodes,grad);
	CHECK_CLOSE(grad[0],-1);

	EdgeSet s;
	unsigned int n = 0;
	nonlinearEdges(root,s);
	n = nzHess(s);
	BOOST_CHECK_EQUAL(n,0);
}

BOOST_AUTO_TEST_CASE( test_nl_function)
{
	vector<Node*> list;
	Node* root = build_nl_function1(list);
	double val = eval_function(root);
	vector<double> grad;
	grad_reverse(root,list,grad);
	double eval =-66.929555552886214;
	double gx[] = {-4.961306690356109,-9.444611307649055,-2.064383410399700,7.123100000000000};
	CHECK_CLOSE(val,eval);

	for(unsigned int i=0;i<4;i++)
	{
		CHECK_CLOSE(grad[i],gx[i]);
	}
	unsigned int nzgrad = nzGrad(root);
	unsigned int tol = numTotalNodes(root);
	BOOST_CHECK_EQUAL(nzgrad,4);
	BOOST_CHECK_EQUAL(tol,16);

	EdgeSet s;
	nonlinearEdges(root,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,11);
}

BOOST_AUTO_TEST_CASE( test_hess_reverse_1)
{
	vector<Node*> nodes;
	Node* root = build_linear_fun1(nodes);
	vector<double> grad;
	double val = grad_reverse(root,nodes,grad);
	double eval = eval_function(root);
//	cout<<eval<<"\t"<<grad[0]<<"\t"<<grad[1]<<"\t"<<grad[2]<<"\t"<<endl;

	CHECK_CLOSE(val,eval);

	for(unsigned int i=0;i<nodes.size();i++)
	{
		static_cast<VNode*>(nodes[i])->u = 0;
	}

	static_cast<VNode*>(nodes[0])->u = 1;
	double hval = 0;
	vector<double> dhess;
	hval = hess_reverse(root,nodes,dhess);
	CHECK_CLOSE(hval,eval);
	for(unsigned int i=0;i<dhess.size();i++)
	{
		CHECK_CLOSE(dhess[i],0);
	}
}

BOOST_AUTO_TEST_CASE( test_hess_reverse_2)
{
	vector<Node*> nodes;
	Node* root = build_linear_function2(nodes);
	vector<double> grad;
	double val = grad_reverse(root,nodes,grad);
	double eval = eval_function(root);

	CHECK_CLOSE(val,eval);
	for(unsigned int i=0;i<nodes.size();i++)
	{
		static_cast<VNode*>(nodes[i])->u = 0;
	}

	static_cast<VNode*>(nodes[0])->u = 1;
	double hval = 0;
	vector<double> dhess;
	hval = hess_reverse(root,nodes,dhess);
	CHECK_CLOSE(hval,eval);

	for(unsigned int i=0;i<dhess.size();i++)
	{
		CHECK_CLOSE(dhess[i],0);
	}

	EdgeSet s;
	nonlinearEdges(root,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,0);
}

BOOST_AUTO_TEST_CASE( test_hess_reverse_4)
{
	vector<Node*> nodes;
//	Node* root = build_nl_function1(nodes);

	VNode* x1 = create_var_node();
	nodes.push_back(x1);
	x1->val = 1;
	x1->u =1;
	Node* op = create_uary_op_node(OP_SIN,x1);
	Node* root = create_uary_op_node(OP_SIN,op);
	vector<double> grad;
	double eval = eval_function(root);
	vector<double> dhess;
	double hval = hess_reverse(root,nodes,dhess);
	CHECK_CLOSE(hval,eval);
	BOOST_CHECK_EQUAL(dhess.size(),1);
	CHECK_CLOSE(dhess[0], -0.778395788418109);

	EdgeSet s;
	nonlinearEdges(root,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,1);
}

BOOST_AUTO_TEST_CASE( test_hess_reverse_3)
{
	vector<Node*> nodes;
	VNode* x1 = create_var_node();
	VNode* x2 = create_var_node();
	nodes.push_back(x1);
	nodes.push_back(x2);
	x1->val = 2.5;
	x2->val = -9;
	Node* op1 = create_binary_op_node(OP_TIMES,x1,x2);
	Node* root = create_binary_op_node(OP_TIMES,x1,op1);
	double eval = eval_function(root);
	for(unsigned int i=0;i<nodes.size();i++)
	{
		static_cast<VNode*>(nodes[i])->u = 0;
	}
	static_cast<VNode*>(nodes[0])->u = 1;

	vector<double> dhess;
	double hval = hess_reverse(root,nodes,dhess);
	BOOST_CHECK_EQUAL(dhess.size(),2);
	CHECK_CLOSE(hval,eval);
	double hx[]={-18,5};
	for(unsigned int i=0;i<dhess.size();i++)
	{
		//Print("\t["<<i<<"]="<<dhess[i]);
		CHECK_CLOSE(dhess[i],hx[i]);
	}

	EdgeSet s;
	nonlinearEdges(root,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,3);
}

BOOST_AUTO_TEST_CASE( test_hess_reverse_5)
{
	vector<Node*> nodes;
	VNode* x1 = create_var_node();
	VNode* x2 = create_var_node();
	nodes.push_back(x1);
	nodes.push_back(x2);
	x1->val = 2.5;
	x2->val = -9;
	Node* op1 = create_binary_op_node(OP_TIMES,x1,x1);
	Node* op2 = create_binary_op_node(OP_TIMES,x2,x2);
	Node* op3 = create_binary_op_node(OP_MINUS,op1,op2);
	Node* op4 = create_binary_op_node(OP_PLUS,op1,op2);
	Node* root = create_binary_op_node(OP_TIMES,op3,op4);

	double eval = eval_function(root);

	for(unsigned int i=0;i<nodes.size();i++)
	{
		static_cast<VNode*>(nodes[i])->u = 0;
	}
	static_cast<VNode*>(nodes[0])->u = 1;

	vector<double> dhess;
	double hval = hess_reverse(root,nodes,dhess);
	CHECK_CLOSE(hval,eval);
	double hx[] ={75,0};
	for(unsigned int i=0;i<dhess.size();i++)
	{
		CHECK_CLOSE(dhess[i],hx[i]);
	}

	for(unsigned int i=0;i<nodes.size();i++)
	{
		static_cast<VNode*>(nodes[i])->u = 0;
	}
	static_cast<VNode*>(nodes[1])->u = 1;

	double hx2[] = {0, -972};
	hval = hess_reverse(root,nodes,dhess);
	for(unsigned int i=0;i<dhess.size();i++)
	{
		CHECK_CLOSE(dhess[i],hx2[i]);
	}

	EdgeSet s;
	nonlinearEdges(root,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,4);
}
BOOST_AUTO_TEST_CASE( test_hess_reverse_6)
{
	vector<Node*> nodes;
//	Node* root = build_nl_function1(nodes);

	VNode* x1 = create_var_node();
	VNode* x2 = create_var_node();
	nodes.push_back(x1);
	nodes.push_back(x2);
	x1->val = 2.5;
	x2->val = -9;
	Node* root = create_binary_op_node(OP_POW,x1,x2);

	double eval = eval_function(root);

	static_cast<VNode*>(nodes[0])->u=1;static_cast<VNode*>(nodes[1])->u=0;
	vector<double> dhess;
	double hval = hess_reverse(root,nodes,dhess);
	CHECK_CLOSE(hval,eval);
	double hx1[] ={0.003774873600000 , -0.000759862823419};
	double hx2[] ={-0.000759862823419, 0.000220093141567};
	for(unsigned int i=0;i<dhess.size();i++)
	{
		CHECK_CLOSE(dhess[i],hx1[i]);
	}
	static_cast<VNode*>(nodes[0])->u=0;static_cast<VNode*>(nodes[1])->u=1;
	hess_reverse(root,nodes,dhess);
	for(unsigned int i=0;i<dhess.size();i++)
	{
		CHECK_CLOSE(dhess[i],hx2[i]);
	}

	EdgeSet s;
	nonlinearEdges(root,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,4);
}

BOOST_AUTO_TEST_CASE( test_hess_reverse_7)
{
	vector<Node*> nodes;
	Node* root = build_nl_function1(nodes);

	double eval = eval_function(root);


	vector<double> dhess;
	double hx0[] ={-1.747958066718855,
			  -0.657091724418110,
			   2.410459188139686,
			                   0};
	double hx1[] ={ -0.657091724418110,
			   0.006806564792590,
			  -0.289815306593997,
			   1.000000000000000};
	double hx2[] ={  2.410459188139686,
			  -0.289815306593997,
			   2.064383410399700,
			                   0};
	double hx3[] ={0,1,0,0};
	for(unsigned int i=0;i<nodes.size();i++)
	{
		static_cast<VNode*>(nodes[i])->u = 0;
	}
	static_cast<VNode*>(nodes[0])->u = 1;
	double hval = hess_reverse(root,nodes,dhess);
	CHECK_CLOSE(hval,eval);
	for(unsigned int i=0;i<dhess.size();i++)
	{
		CHECK_CLOSE(dhess[i],hx0[i]);
	}

	for (unsigned int i = 0; i < nodes.size(); i++) {
		static_cast<VNode*>(nodes[i])->u = 0;
	}
	static_cast<VNode*>(nodes[1])->u = 1;
	hess_reverse(root, nodes, dhess);
	for (unsigned int i = 0; i < dhess.size(); i++) {
		CHECK_CLOSE(dhess[i], hx1[i]);
	}

	for (unsigned int i = 0; i < nodes.size(); i++) {
		static_cast<VNode*>(nodes[i])->u = 0;
	}
	static_cast<VNode*>(nodes[2])->u = 1;
	hess_reverse(root, nodes, dhess);
	for (unsigned int i = 0; i < dhess.size(); i++) {
		CHECK_CLOSE(dhess[i], hx2[i]);
	}

	for (unsigned int i = 0; i < nodes.size(); i++) {
		static_cast<VNode*>(nodes[i])->u = 0;
	}
	static_cast<VNode*>(nodes[3])->u = 1;
	hess_reverse(root, nodes, dhess);
	for (unsigned i = 0; i < dhess.size(); i++) {
		CHECK_CLOSE(dhess[i], hx3[i]);
	}
}

#if FORWARD_ENABLED
void test_hess_forward(Node* root, unsigned int& nvar)
{
	AutoDiff::num_var = nvar;
	unsigned int len = (nvar+3)*nvar/2;
	double* hess = new double[len];
	hess_forward(root,nvar,&hess);
	for(unsigned int i=0;i<len;i++){
		cout<<"hess["<<i<<"]="<<hess[i]<<endl;
	}
	delete[] hess;
}
#endif

BOOST_AUTO_TEST_CASE( test_hess_reverse_8)
{
	vector<Node*> list;
	vector<double> dhess;

	VNode* x1 = create_var_node();
	list.push_back(x1);
	static_cast<VNode*>(list[0])->val = -10.5;
	static_cast<VNode*>(list[0])->u = 1;
	double deval = hess_reverse(x1,list,dhess);
	CHECK_CLOSE(deval,-10.5);
	BOOST_CHECK_EQUAL(dhess.size(),1);
	BOOST_CHECK(isnan(dhess[0]));

	EdgeSet s;
	nonlinearEdges(x1,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,0);

	PNode* p1 = create_param_node(-1.5);
	list.clear();
	deval = hess_reverse(p1,list,dhess);
	CHECK_CLOSE(deval,-1.5);
	BOOST_CHECK_EQUAL(dhess.size(),0);

	s.clear();
	nonlinearEdges(p1,s);
	n = nzHess(s);
	BOOST_CHECK_EQUAL(n,0);
}

BOOST_AUTO_TEST_CASE( test_hess_revers9)
{
	vector<Node*> list;
	vector<double> dhess;
	VNode* x1 = create_var_node();
	list.push_back(x1);
	static_cast<VNode*>(list[0])->val = 2.5;
	static_cast<VNode*>(list[0])->u =1;
	Node* op1 = create_binary_op_node(OP_TIMES,x1,x1);
	Node* root = create_binary_op_node(OP_TIMES,op1,op1);
	double deval = hess_reverse(root,list,dhess);
	double eval = eval_function(root);
	CHECK_CLOSE(eval,deval);
	BOOST_CHECK_EQUAL(dhess.size(),1);
	CHECK_CLOSE(dhess[0],75);

	EdgeSet s;
	nonlinearEdges(root,s);
	unsigned int n = nzHess(s);
	BOOST_CHECK_EQUAL(n,1);

}

BOOST_AUTO_TEST_CASE( test_hess_revers10)
{
	vector<Node*> list;
	vector<double> dhess;
	VNode* x1 = create_var_node();
	VNode* x2 = create_var_node();
	list.push_back(x1);
	list.push_back(x2);
	Node* op1 = create_binary_op_node(OP_TIMES, x1,x2);
	Node* op2 = create_uary_op_node(OP_SIN,op1);
	Node* op3 = create_uary_op_node(OP_COS,op1);
	Node* root = create_binary_op_node(OP_TIMES, op2, op3);
	static_cast<VNode*>(list[0])->val = 2.1;
	static_cast<VNode*>(list[1])->val = 1.8;
	double eval = eval_function(root);

	//second column
	static_cast<VNode*>(list[0])->u = 0;
	static_cast<VNode*>(list[1])->u = 1;
	double deval = hess_reverse(root,list,dhess);
	CHECK_CLOSE(eval,deval);
	BOOST_CHECK_EQUAL(dhess.size(),2);
	CHECK_CLOSE(dhess[0], -6.945893481707861);
	CHECK_CLOSE(dhess[1],  -8.441601940854081);

	//first column
	static_cast<VNode*>(list[0])->u = 1;
	static_cast<VNode*>(list[1])->u = 0;
	deval = hess_reverse(root,list,dhess);
	CHECK_CLOSE(eval,deval);
	BOOST_CHECK_EQUAL(dhess.size(),2);
	CHECK_CLOSE(dhess[0], -6.201993262668304);
	CHECK_CLOSE(dhess[1], -6.945893481707861);
}

BOOST_AUTO_TEST_CASE( test_grad_reverse11)
{
	vector<Node*> list;
	VNode* x1 = create_var_node();
	Node* p2 = create_param_node(2);
	list.push_back(x1);
	Node* op1 = create_binary_op_node(OP_POW,x1,p2);
	static_cast<VNode*>(x1)->val = 0;
	vector<double> grad;
	grad_reverse(op1,list,grad);
	BOOST_CHECK_EQUAL(grad.size(),1);
	CHECK_CLOSE(grad[0],0);
}

BOOST_AUTO_TEST_CASE( test_hess_reverse12)
{
	vector<Node*> list;
	VNode* x1 = create_var_node();
	Node* p2 = create_param_node(2);
	list.push_back(x1);
	Node* op1 = create_binary_op_node(OP_POW,x1,p2);
	x1->val = 0;
	x1->u = 1;
	vector<double> hess;
	hess_reverse(op1,list,hess);
	BOOST_CHECK_EQUAL(hess.size(),1);
	CHECK_CLOSE(hess[0],2);
}

BOOST_AUTO_TEST_CASE( test_grad_reverse13)
{
	vector<Node*> list;
	VNode* x1 = create_var_node();
	PNode* p1 = create_param_node(0.090901);
	VNode* x2 = create_var_node();
	PNode* p2 = create_param_node(0.090901);
	list.push_back(x1);
	list.push_back(x2);
	Node* op1 = create_binary_op_node(OP_TIMES,x1,p1);
	Node* op2 = create_binary_op_node(OP_TIMES,x2,p2);
	Node* root = create_binary_op_node(OP_PLUS,op1,op2);
	x1->val = 1;
	x2->val = 1;
	vector<double> grad;
	grad_reverse(root,list,grad);
	BOOST_CHECK_EQUAL(grad.size(),2);
	CHECK_CLOSE(grad[0],0.090901);
	CHECK_CLOSE(grad[1],0.090901);
}

BOOST_AUTO_TEST_SUITE_END()
