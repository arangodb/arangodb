/*
 * PNode.cpp
 *
 *  Created on: 8 Apr 2013
 *      Author: s0965328
 */

#include "PNode.h"
#include "Stack.h"
#include "Tape.h"
#include "EdgeSet.h"

namespace AutoDiff {

PNode::PNode(double value):pval(value) {
	assert(!isnan(value));
}

PNode::~PNode() {
}

void PNode::inorder_visit(int level,ostream& oss){
	oss<<this->toString(level)<<endl;
}

void PNode::collect_vnodes(boost::unordered_set<Node*>& nodes,unsigned int& total)
{
	//do not fill this to nodes, as this is a parameter node
	total++;
}

void PNode::eval_function()
{
	SV->push_back(pval);
}

string PNode::toString(int level)
{
	ostringstream oss;
	string s(level,'\t');
	oss<<s<<"[PNode]("<<pval<<")";
	return oss.str();
}

void PNode::grad_reverse_0()
{
	SV->push_back(pval);
}
void PNode::grad_reverse_1_init_adj()
{
	//do nothing as PNode does not have adjoint
}

void PNode::grad_reverse_1()
{
	//do nothing
	//this is a parameter
}

void PNode::update_adj(double& v)
{
	//do nothing
	//no adj for PNode
}

unsigned int PNode::hess_reverse_0()
{
	if(index==0)
	{
		TT->set(pval);
		assert(TT->index == TT->index);
		index = TT->index;
	}
	return index;
}

void PNode::hess_reverse_0_get_values(unsigned int i,double& x,double& x_bar,double& w,double& w_bar)
{
	x = TT->get(--i);
	x_bar = 0;
	w = 0;
	w_bar = 0;
}

void PNode::hess_reverse_1(unsigned int i)
{
	n_in_arcs--;
	//leaf node do nothing
}

void PNode::hess_reverse_1_init_x_bar(unsigned int)
{
	//do nothing as Parameter does not have x_bar
}

void PNode::update_x_bar(unsigned int i ,double v)
{
	//do nothing as Parameter does not have x_bar
}
void PNode::update_w_bar(unsigned int i ,double v)
{
	//do nothing as Parameter does not have w_bar
}
void PNode::hess_reverse_1_get_xw(unsigned int i, double& w,double& x)
{
	//do nothing as Parameter does not have w
	x = TT->get(i-1);
	w = 0;
}
void PNode::hess_reverse_get_x(unsigned int i, double& x)
{
	x = TT->get(i-1);
}

void PNode::nonlinearEdges(EdgeSet& edges)
{
	for(std::list<Edge>::iterator it=edges.edges.begin();it!=edges.edges.end();)
	{
		Edge e = *it;
		if(e.a == this || e.b == this)
		{
			it = edges.edges.erase(it); //erase invalidate the iterator
		}
		else
		{
			it++;
		}
	}
}

#if FORWARD_ENABLED
void PNode::hess_forward(unsigned int len, double** ret_vec)
{
	//it's a scalar
	(*ret_vec) = new double[len];
	std::fill_n(*ret_vec,len,0);
	SV->push_back(this->pval);
	assert(SV->size()==1);
}
#endif


TYPE PNode::getType()
{
	return PNode_Type;
}
}
