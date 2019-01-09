/*
 * UaryOPNode.cpp
 *
 *  Created on: 6 Nov 2013
 *      Author: s0965328
 */

#include "UaryOPNode.h"
#include "BinaryOPNode.h"
#include "PNode.h"
#include "Stack.h"
#include "Tape.h"
#include "Edge.h"
#include "EdgeSet.h"
#include "auto_diff_types.h"

#include <list>

using namespace std;

namespace AutoDiff {

UaryOPNode::UaryOPNode(OPCODE op_, Node* left): OPNode(op_,left) {
}

OPNode* UaryOPNode::createUnaryOpNode(OPCODE op, Node* left)
{
	assert(left!=NULL);
	OPNode* node = NULL;
	if(op == OP_SQRT)
	{
		double param = 0.5;
		node = BinaryOPNode::createBinaryOpNode(OP_POW,left,new PNode(param));
	}
	else if(op == OP_NEG)
	{
		double param = -1;
		node = BinaryOPNode::createBinaryOpNode(OP_TIMES,left,new PNode(param));
	}
	else
	{
		node = new UaryOPNode(op,left);
	}
	return node;
}

UaryOPNode::~UaryOPNode() {

}


void UaryOPNode::inorder_visit(int level,ostream& oss){
	if(left!=NULL){
		left->inorder_visit(level+1,oss);
	}
	oss<<this->toString(level)<<endl;
}

void UaryOPNode::collect_vnodes(boost::unordered_set<Node*>& nodes,unsigned int& total)
{
	total++;
	if(left!=NULL){
		left->collect_vnodes(nodes,total);
	}
}

void UaryOPNode::eval_function()
{
	if(left!=NULL){
		left->eval_function();
	}
	this->calc_eval_function();
}

//1. visiting left if not NULL
//2. then, visiting right if not NULL
//3. calculating the immediate derivative hu and hv
void UaryOPNode::grad_reverse_0(){
	assert(left!=NULL);
	this->adj = 0;
	left->grad_reverse_0();
	this->calc_grad_reverse_0();
}

//right left - right most traversal
void UaryOPNode::grad_reverse_1()
{
	assert(left!=NULL);
	double l_adj = SD->pop_back()*this->adj;
	left->update_adj(l_adj);
	left->grad_reverse_1();
}

void UaryOPNode::calc_grad_reverse_0()
{
	assert(left!=NULL);
	double hu = NaN_Double;
	double lval = SV->pop_back();
	double val = NaN_Double;
	switch (op)
	{
	case OP_SIN:
		val = sin(lval);
		hu = cos(lval);
		break;
	case OP_COS:
		val = cos(lval);
		hu = -sin(lval);
		break;
	default:
		cerr<<"error op not impl"<<endl;
		break;
	}
	SV->push_back(val);
	SD->push_back(hu);
}

void UaryOPNode::calc_eval_function()
{
	double lval = SV->pop_back();
	double val = NaN_Double;
	switch (op)
	{
	case OP_SIN:
		assert(left!=NULL);
		val = sin(lval);
		break;
	case OP_COS:
		assert(left!=NULL);
		val = cos(lval);
		break;
	default:
		cerr<<"op["<<op<<"] not yet implemented!!"<<endl;
		assert(false);
		break;
	}
	SV->push_back(val);
}

void UaryOPNode::hess_reverse_0_init_n_in_arcs()
{
	this->left->hess_reverse_0_init_n_in_arcs();
	this->Node::hess_reverse_0_init_n_in_arcs();
}

void UaryOPNode::hess_reverse_1_clear_index()
{
	this->left->hess_reverse_1_clear_index();
	this->Node::hess_reverse_1_clear_index();
}

unsigned int UaryOPNode::hess_reverse_0()
{
	assert(left!=NULL);
	if(index==0)
	{
		unsigned int lindex=0;
		lindex = left->hess_reverse_0();
		assert(lindex!=0);
		II->set(lindex);
		double lx,lx_bar,lw,lw_bar;
		double x,x_bar,w,w_bar;
		double l_dh;
		switch(op)
		{
		case OP_SIN:
			assert(left != NULL);
			left->hess_reverse_0_get_values(lindex,lx,lx_bar,lw,lw_bar);
			x = sin(lx);
			x_bar = 0;
			l_dh = cos(lx);
			w = lw*l_dh;
			w_bar = 0;
			break;
		case OP_COS:
			assert(left!=NULL);
			left->hess_reverse_0_get_values(lindex,lx,lx_bar,lw,lw_bar);
			x = cos(lx);
			x_bar = 0;
			l_dh = -sin(lx);
			w = lw*l_dh;
			w_bar = 0;
			break;
		default:
			cerr<<"op["<<op<<"] not yet implemented!"<<endl;
			assert(false);
			break;
		}
		TT->set(x);
		TT->set(x_bar);
		TT->set(w);
		TT->set(w_bar);
		TT->set(l_dh);
		assert(TT->index == TT->index);
		index = TT->index;
	}
	return index;
}

void UaryOPNode::hess_reverse_0_get_values(unsigned int i,double& x, double& x_bar, double& w, double& w_bar)
{
	--i; // skip the l_dh (ie, dh/du)
	w_bar = TT->get(--i);
	w = TT->get(--i);
	x_bar = TT->get(--i);
	x = TT->get(--i);
}

void UaryOPNode::hess_reverse_1(unsigned int i)
{
	n_in_arcs--;
	if(n_in_arcs==0)
	{
		double lindex = II->get(--(II->index));
	//	cout<<"li["<<lindex<<"]\t"<<this->toString(0)<<endl;
		double l_dh = TT->get(--i);
		double w_bar = TT->get(--i);
		--i; //skip w
		double x_bar = TT->get(--i);
		--i; //skip x
	//	cout<<"i["<<i<<"]"<<endl;

		assert(left!=NULL);
		left->update_x_bar(lindex,x_bar*l_dh);
		double lw_bar = 0;
		double lw = 0,lx = 0;
		left->hess_reverse_1_get_xw(lindex,lw,lx);
		switch(op)
		{
		case OP_SIN:
			assert(l_dh == cos(lx));
			lw_bar += w_bar*l_dh;
			lw_bar += x_bar*lw*(-sin(lx));
			break;
		case OP_COS:
			assert(l_dh == -sin(lx));
			lw_bar += w_bar*l_dh;
			lw_bar += x_bar*lw*(-cos(lx));
			break;
		default:
			cerr<<"op["<<op<<"] not yet implemented!"<<endl;
			break;
		}
		left->update_w_bar(lindex,lw_bar);
		left->hess_reverse_1(lindex);
	}
}

void UaryOPNode::hess_reverse_1_init_x_bar(unsigned int i)
{
	TT->at(i-4) = 1;
}

void UaryOPNode::update_x_bar(unsigned int i ,double v)
{
	TT->at(i-4) += v;
}
void UaryOPNode::update_w_bar(unsigned int i ,double v)
{
	TT->at(i-2) += v;
}
void UaryOPNode::hess_reverse_1_get_xw(unsigned int i,double& w,double& x)
{
	w = TT->get(i-3);
	x = TT->get(i-5);
}
void UaryOPNode::hess_reverse_get_x(unsigned int i, double& x)
{
	x = TT->get(i-5);
}

void UaryOPNode::nonlinearEdges(EdgeSet& edges)
{
	for(list<Edge>::iterator it=edges.edges.begin();it!=edges.edges.end();)
	{
		Edge& e = *it;
		if(e.a == this || e.b == this){
			if(e.a == this && e.b == this)
			{
				Edge e1(left,left);
				edges.insertEdge(e1);
			}
			else{
				Node* o = e.a==this?e.b:e.a;
				Edge e1(left,o);
				edges.insertEdge(e1);
			}
			it = edges.edges.erase(it);
		}
		else
		{
			it++;
		}
	}

	Edge e1(left,left);
	switch(op)
	{
	case OP_SIN:
		edges.insertEdge(e1);
		break;
	case OP_COS:
		edges.insertEdge(e1);
		break;
	default:
		cerr<<"op["<<op<<"] is not yet implemented !"<<endl;
		assert(false);
		break;
	}
	left->nonlinearEdges(edges);
}

#if FORWARD_ENABLED
void UaryOPNode::hess_forward(unsigned int len, double** ret_vec)
{
	double* lvec = NULL;
	if(left!=NULL){
		left->hess_forward(len,&lvec);
	}

	*ret_vec = new double[len];
	this->hess_forward_calc0(len,lvec,*ret_vec);
	delete[] lvec;
}

void UaryOPNode::hess_forward_calc0(unsigned int& len, double* lvec, double* ret_vec)
{
	double hu = NaN_Double;
	double lval = NaN_Double;
	double val = NaN_Double;
	unsigned int index = 0;
	switch (op)
	{
	case OP_SIN:
		assert(left!=NULL);
		lval = SV->pop_back();
		val = sin(lval);
		SV->push_back(val);
		hu = cos(lval);

		double coeff;
		coeff = -val; //=sin(left->val); -- and avoid cross initialisation
		//calculate the first order derivatives
		for(unsigned int i =0;i<AutoDiff::num_var;++i)
		{
			ret_vec[i] = hu*lvec[i] + 0;
		}
		//calculate the second order
		index = AutoDiff::num_var;
		for(unsigned int i=0;i<AutoDiff::num_var;++i)
		{
			for(unsigned int j=i;j<AutoDiff::num_var;++j)
			{
				ret_vec[index] = hu*lvec[index] + lvec[i] * coeff * lvec[j] + 0 + 0;
				++index;
			}
		}
		assert(index==len);
		break;
	default:
		cerr<<"op["<<op<<"] not yet implemented!";
		break;
	}
}
#endif

string UaryOPNode::toString(int level)
{
	ostringstream oss;
	string s(level,'\t');
	oss<<s<<"[UaryOPNode]("<<op<<")";
	return oss.str();
}

} /* namespace AutoDiff */
