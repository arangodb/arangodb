/*
 * BinaryOPNode.cpp
 *
 *  Created on: 6 Nov 2013
 *      Author: s0965328
 */

#include "auto_diff_types.h"
#include "BinaryOPNode.h"
#include "PNode.h"
#include "Stack.h"
#include "Tape.h"
#include "EdgeSet.h"
#include "Node.h"
#include "VNode.h"
#include "OPNode.h"
#include "ActNode.h"
#include "EdgeSet.h"

namespace AutoDiff {

BinaryOPNode::BinaryOPNode(OPCODE op_, Node* left_, Node* right_):OPNode(op_,left_),right(right_)
{
}

OPNode* BinaryOPNode::createBinaryOpNode(OPCODE op, Node* left, Node* right)
{
	assert(left!=NULL && right!=NULL);
	OPNode* node = NULL;
	node = new BinaryOPNode(op,left,right);
	return node;
}

BinaryOPNode::~BinaryOPNode() {
	if(right->getType()!=VNode_Type)
	{
		delete right;
		right = NULL;
	}
}

void BinaryOPNode::inorder_visit(int level,ostream& oss){
	if(left!=NULL){
		left->inorder_visit(level+1,oss);
	}
	oss<<this->toString(level)<<endl;
	if(right!=NULL){
		right->inorder_visit(level+1,oss);
	}
}

void BinaryOPNode::collect_vnodes(boost::unordered_set<Node*>& nodes,unsigned int& total){
	total++;
	if (left != NULL) {
		left->collect_vnodes(nodes,total);
	}
	if (right != NULL) {
		right->collect_vnodes(nodes,total);
	}

}

void BinaryOPNode::eval_function()
{
	assert(left!=NULL && right!=NULL);
	left->eval_function();
	right->eval_function();
	this->calc_eval_function();
}

void BinaryOPNode::calc_eval_function()
{
	double x = NaN_Double;
	double rx = SV->pop_back();
	double lx = SV->pop_back();
	switch (op)
	{
	case OP_PLUS:
		x = lx + rx;
		break;
	case OP_MINUS:
		x = lx - rx;
		break;
	case OP_TIMES:
		x = lx * rx;
		break;
	case OP_DIVID:
		x = lx / rx;
		break;
	case OP_POW:
		x = pow(lx,rx);
		break;
	default:
		cerr<<"op["<<op<<"] not yet implemented!!"<<endl;
		assert(false);
		break;
	}
	SV->push_back(x);
}


//1. visiting left if not NULL
//2. then, visiting right if not NULL
//3. calculating the immediate derivative hu and hv
void BinaryOPNode::grad_reverse_0()
{
	assert(left!=NULL && right != NULL);
	this->adj = 0;
	left->grad_reverse_0();
	right->grad_reverse_0();
	this->calc_grad_reverse_0();
}

//right left - right most traversal
void BinaryOPNode::grad_reverse_1()
{
	assert(right!=NULL && left!=NULL);
	double r_adj = SD->pop_back()*this->adj;
	right->update_adj(r_adj);
	double l_adj = SD->pop_back()*this->adj;
	left->update_adj(l_adj);

	right->grad_reverse_1();
	left->grad_reverse_1();
}

void BinaryOPNode::calc_grad_reverse_0()
{
	assert(left!=NULL && right != NULL);
	double l_dh = NaN_Double;
	double r_dh = NaN_Double;
	double rx = SV->pop_back();
	double lx = SV->pop_back();
	double x = NaN_Double;
	switch (op)
	{
	case OP_PLUS:
		x = lx + rx;
		l_dh = 1;
		r_dh = 1;
		break;
	case OP_MINUS:
		x = lx - rx;
		l_dh = 1;
		r_dh = -1;
		break;
	case OP_TIMES:
		x = lx * rx;
		l_dh = rx;
		r_dh = lx;
		break;
	case OP_DIVID:
		x = lx / rx;
		l_dh = 1 / rx;
		r_dh = -(lx) / pow(rx, 2);
		break;
	case OP_POW:
		if(right->getType()==PNode_Type){
			x = pow(lx,rx);
			l_dh = rx*pow(lx,(rx-1));
			r_dh = 0;
		}
		else{
			assert(lx>0.0); //otherwise log(lx) is not defined in read number
			x = pow(lx,rx);
			l_dh = rx*pow(lx,(rx-1));
			r_dh = pow(lx,rx)*log(lx);  //this is for x1^x2 when x1=0 cause r_dh become +inf, however d(0^x2)/d(x2) = 0
		}
		break;
	default:
		cerr<<"error op not impl"<<endl;
		break;
	}
	SV->push_back(x);
	SD->push_back(l_dh);
	SD->push_back(r_dh);
}

void BinaryOPNode::hess_reverse_0_init_n_in_arcs()
{
	this->left->hess_reverse_0_init_n_in_arcs();
	this->right->hess_reverse_0_init_n_in_arcs();
	this->Node::hess_reverse_0_init_n_in_arcs();
}

void BinaryOPNode::hess_reverse_1_clear_index()
{
	this->left->hess_reverse_1_clear_index();
	this->right->hess_reverse_1_clear_index();
	this->Node::hess_reverse_1_clear_index();
}

unsigned int BinaryOPNode::hess_reverse_0()
{
	assert(this->left!=NULL && right!=NULL);
	if(index==0)
	{
		unsigned int lindex=0, rindex=0;
		lindex = left->hess_reverse_0();
		rindex = right->hess_reverse_0();
		assert(lindex!=0 && rindex !=0);
		II->set(lindex);
		II->set(rindex);
		double rx,rx_bar,rw,rw_bar;
		double lx,lx_bar,lw,lw_bar;
		double x,x_bar,w,w_bar;
		double r_dh, l_dh;
		right->hess_reverse_0_get_values(rindex,rx,rx_bar,rw,rw_bar);
		left->hess_reverse_0_get_values(lindex,lx,lx_bar,lw,lw_bar);
		switch(op)
		{
		case OP_PLUS:
	//		cout<<"lindex="<<lindex<<"\trindex="<<rindex<<"\tI="<<I<<endl;
			x = lx + rx;
	//		cout<<lx<<"\t+"<<rx<<"\t="<<x<<"\t\t"<<toString(0)<<endl;
			x_bar = 0;
			l_dh = 1;
			r_dh = 1;
			w = lw * l_dh  + rw * r_dh;
	//		cout<<lw<<"\t+"<<rw<<"\t="<<w<<"\t\t"<<toString(0)<<endl;
			w_bar = 0;
			break;
		case OP_MINUS:
			x = lx - rx;
			x_bar = 0;
			l_dh = 1;
			r_dh = -1;
			w = lw * l_dh  + rw * r_dh;
			w_bar = 0;
			break;
		case OP_TIMES:
			x = lx * rx;
			x_bar = 0;
			l_dh = rx;
			r_dh = lx;
			w = lw * l_dh  + rw * r_dh;
			w_bar = 0;
			break;
		case OP_DIVID:
			x = lx / rx;
			x_bar = 0;
			l_dh = 1/rx;
			r_dh = -lx/pow(rx,2);
			w = lw * l_dh  + rw * r_dh;
			w_bar = 0;
			break;
		case OP_POW:
			if(right->getType()==PNode_Type)
			{
				x = pow(lx,rx);
				x_bar = 0;
				l_dh = rx*pow(lx,(rx-1));
				r_dh = 0;
				w = lw * l_dh + rw * r_dh;
				w_bar = 0;
			}
			else
			{
				assert(lx>0.0); //otherwise log(lx) undefined in real number
				x = pow(lx,rx);
				x_bar = 0;
				l_dh = rx*pow(lx,(rx-1));
				r_dh = pow(lx,rx)*log(lx);   //log(lx) cause -inf when lx=0;
				w = lw * l_dh  + rw * r_dh;
				w_bar = 0;
			}
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
		TT->set(r_dh);
		assert(TT->index == TT->index);
		index = TT->index;
	}
	return index;
}

void BinaryOPNode::hess_reverse_0_get_values(unsigned int i,double& x, double& x_bar, double& w, double& w_bar)
{
	--i; // skip the r_dh (ie, dh/du)
	--i; // skip the l_dh (ie. dh/dv)
	w_bar = TT->get(--i);
	w = TT->get(--i);
	x_bar = TT->get(--i);
	x = TT->get(--i);
}

void BinaryOPNode::hess_reverse_1(unsigned int i)
{
	n_in_arcs--;
	if(n_in_arcs==0)
	{
		assert(right!=NULL && left!=NULL);
		unsigned int rindex = II->get(--(II->index));
		unsigned int lindex = II->get(--(II->index));
	//	cout<<"ri["<<rindex<<"]\tli["<<lindex<<"]\t"<<this->toString(0)<<endl;
		double r_dh = TT->get(--i);
		double l_dh = TT->get(--i);
		double w_bar = TT->get(--i);
		--i; //skip w
		double x_bar = TT->get(--i);
		--i; //skip x

		double lw_bar=0,rw_bar=0;
		double lw=0,lx=0; left->hess_reverse_1_get_xw(lindex,lw,lx);
		double rw=0,rx=0; right->hess_reverse_1_get_xw(rindex,rw,rx);
		switch(op)
		{
		case OP_PLUS:
			assert(l_dh==1);
			assert(r_dh==1);
			lw_bar += w_bar*l_dh;
			rw_bar += w_bar*r_dh;
			break;
		case OP_MINUS:
			assert(l_dh==1);
			assert(r_dh==-1);
			lw_bar += w_bar*l_dh;
			rw_bar += w_bar*r_dh;
			break;
		case OP_TIMES:
			assert(rx == l_dh);
			assert(lx == r_dh);
			lw_bar += w_bar*rx;
			lw_bar += x_bar*lw*0 + x_bar*rw*1;
			rw_bar += w_bar*lx;
			rw_bar += x_bar*lw*1 + x_bar*rw*0;
			break;
		case OP_DIVID:
			lw_bar += w_bar*l_dh;
			lw_bar += x_bar*lw*0 + x_bar*rw*-1/(pow(rx,2));
			rw_bar += w_bar*r_dh;
			rw_bar += x_bar*lw*-1/pow(rx,2) + x_bar*rw*2*lx/pow(rx,3);
			break;
		case OP_POW:
			if(right->getType()==PNode_Type){
				lw_bar += w_bar*l_dh;
				lw_bar += x_bar*lw*pow(lx,rx-2)*rx*(rx-1) + 0;
				rw_bar += w_bar*r_dh; assert(r_dh==0.0);
				rw_bar += 0;
			}
			else{
				assert(lx>0.0); //otherwise log(lx) is not define in Real
				lw_bar += w_bar*l_dh;
				lw_bar += x_bar*lw*pow(lx,rx-2)*rx*(rx-1) + x_bar*rw*pow(lx,rx-1)*(rx*log(lx)+1);  //cause log(lx)=-inf when
				rw_bar += w_bar*r_dh;
				rw_bar += x_bar*lw*pow(lx,rx-1)*(rx*log(lx)+1) + x_bar*rw*pow(lx,rx)*pow(log(lx),2);
			}
			break;
		default:
			cerr<<"op["<<op<<"] not yet implemented !"<<endl;
			assert(false);
			break;
		}
		double rx_bar = x_bar*r_dh;
		double lx_bar = x_bar*l_dh;
		right->update_x_bar(rindex,rx_bar);
		left->update_x_bar(lindex,lx_bar);
		right->update_w_bar(rindex,rw_bar);
		left->update_w_bar(lindex,lw_bar);


		this->right->hess_reverse_1(rindex);
		this->left->hess_reverse_1(lindex);
	}
}
void BinaryOPNode::hess_reverse_1_init_x_bar(unsigned int i)
{
	TT->at(i-5) = 1;
}
void BinaryOPNode::update_x_bar(unsigned int i ,double v)
{
	TT->at(i-5) += v;
}
void BinaryOPNode::update_w_bar(unsigned int i ,double v)
{
	TT->at(i-3) += v;
}
void BinaryOPNode::hess_reverse_1_get_xw(unsigned int i,double& w,double& x)
{
	w = TT->get(i-4);
	x = TT->get(i-6);
}
void BinaryOPNode::hess_reverse_get_x(unsigned int i,double& x)
{
	x = TT->get(i-6);
}


void BinaryOPNode::nonlinearEdges(EdgeSet& edges)
{
	for(list<Edge>::iterator it=edges.edges.begin();it!=edges.edges.end();)
	{
		Edge e = *it;
		if(e.a==this || e.b == this){
			if(e.a == this && e.b == this)
			{
				Edge e1(left,left);
				Edge e2(right,right);
				Edge e3(left,right);
				edges.insertEdge(e1);
				edges.insertEdge(e2);
				edges.insertEdge(e3);
			}
			else
			{
				Node* o = e.a==this? e.b: e.a;
				Edge e1(left,o);
				Edge e2(right,o);
				edges.insertEdge(e1);
				edges.insertEdge(e2);
			}
			it = edges.edges.erase(it);
		}
		else
		{
			it++;
		}
	}

	Edge e1(left,right);
	Edge e2(left,left);
	Edge e3(right,right);
	switch(op)
	{
	case OP_PLUS:
	case OP_MINUS:
		//do nothing for linear operator
		break;
	case OP_TIMES:
		edges.insertEdge(e1);
		break;
	case OP_DIVID:
		edges.insertEdge(e1);
		edges.insertEdge(e3);
		break;
	case OP_POW:
		edges.insertEdge(e1);
		edges.insertEdge(e2);
		edges.insertEdge(e3);
		break;
	default:
		cerr<<"op["<<op<<"] not yet implmented !"<<endl;
		assert(false);
		break;
	}
	left->nonlinearEdges(edges);
	right->nonlinearEdges(edges);
}

#if FORWARD_ENABLED

void BinaryOPNode::hess_forward(unsigned int len, double** ret_vec)
{
	double* lvec = NULL;
	double* rvec = NULL;
	if(left!=NULL){
		left->hess_forward(len,&lvec);
	}
	if(right!=NULL){
		right->hess_forward(len,&rvec);
	}

	*ret_vec = new double[len];
	hess_forward_calc0(len,lvec,rvec,*ret_vec);
	//delete lvec, rvec
	delete[] lvec;
	delete[] rvec;
}

void BinaryOPNode::hess_forward_calc0(unsigned int& len, double* lvec, double* rvec, double* ret_vec)
{
	double hu = NaN_Double, hv= NaN_Double;
	double lval = NaN_Double, rval = NaN_Double;
	double val = NaN_Double;
	unsigned int index = 0;
	switch (op)
	{
	case OP_PLUS:
		rval = SV->pop_back();
		lval = SV->pop_back();
		val = lval + rval;
		SV->push_back(val);
		//calculate the first order derivatives
		for(unsigned int i=0;i<AutoDiff::num_var;++i)
		{
			ret_vec[i] = lvec[i]+rvec[i];
		}
		//calculate the second order
		index = AutoDiff::num_var;
		for(unsigned int i=0;i<AutoDiff::num_var;++i)
		{
			for(unsigned int j=i;j<AutoDiff::num_var;++j){
				ret_vec[index] = lvec[index] + 0 + rvec[index] + 0;
				++index;
			}
		}
		assert(index==len);
		break;
	case OP_MINUS:
		rval = SV->pop_back();
		lval = SV->pop_back();
		val = lval + rval;
		SV->push_back(val);
		//calculate the first order derivatives
		for(unsigned int i=0;i<AutoDiff::num_var;++i)
		{
			ret_vec[i] = lvec[i] - rvec[i];
		}
		//calculate the second order
		index = AutoDiff::num_var;
		for(unsigned int i=0;i<AutoDiff::num_var;++i)
		{
			for(unsigned int j=i;j<AutoDiff::num_var;++j){
				ret_vec[index] = lvec[index] + 0 - rvec[index] + 0;
				++index;
			}
		}
		assert(index==len);
		break;
	case OP_TIMES:
		rval = SV->pop_back();
		lval = SV->pop_back();
		val = lval * rval;
		SV->push_back(val);
		hu = rval;
		hv = lval;
		//calculate the first order derivatives
		for(unsigned int i =0;i<AutoDiff::num_var;++i)
		{
			ret_vec[i] = hu*lvec[i] + hv*rvec[i];
		}
		//calculate the second order
		index = AutoDiff::num_var;
		for(unsigned int i=0;i<AutoDiff::num_var;++i)
		{
			for(unsigned int j=i;j<AutoDiff::num_var;++j)
			{
				ret_vec[index] = hu * lvec[index] + lvec[i] * rvec[j]+hv * rvec[index] + rvec[i] * lvec[j];
				++index;
			}
		}
		assert(index==len);
		break;
	case OP_POW:
		rval = SV->pop_back();
		lval = SV->pop_back();
		val = pow(lval,rval);
		SV->push_back(val);
		if(left->getType()==PNode_Type && right->getType()==PNode_Type)
		{
			std::fill_n(ret_vec,len,0);
		}
		else
		{
			hu = rval*pow(lval,(rval-1));
			hv = pow(lval,rval)*log(lval);
			if(left->getType()==PNode_Type)
			{
				double coeff = pow(log(lval),2)*pow(lval,rval);
				//calculate the first order derivatives
				for(unsigned int i =0;i<AutoDiff::num_var;++i)
				{
					ret_vec[i] = hu*lvec[i] + hv*rvec[i];
				}
				//calculate the second order
				index = AutoDiff::num_var;
				for(unsigned int i=0;i<AutoDiff::num_var;++i)
				{
					for(unsigned int j=i;j<AutoDiff::num_var;++j)
					{
						ret_vec[index] = 0 + 0 + hv * rvec[index] + rvec[i] * coeff * rvec[j];
						++index;
					}
				}
			}
			else if(right->getType()==PNode_Type)
			{
				double coeff = rval*(rval-1)*pow(lval,rval-2);
				//calculate the first order derivatives
				for(unsigned int i =0;i<AutoDiff::num_var;++i)
				{
					ret_vec[i] = hu*lvec[i] + hv*rvec[i];
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
			}
			else
			{
				assert(false);
			}
		}
		assert(index==len);
		break;
	case OP_SIN: //TODO should move to UnaryOPNode.cpp?
		assert(left!=NULL&&right==NULL);
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


string BinaryOPNode::toString(int level){
	ostringstream oss;
	string s(level,'\t');
	oss<<s<<"[BinaryOPNode]("<<op<<")";
	return oss.str();
}

} /* namespace AutoDiff */
