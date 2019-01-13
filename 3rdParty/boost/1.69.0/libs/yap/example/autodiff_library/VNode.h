/*
 * VNode.h
 *
 *  Created on: 8 Apr 2013
 *      Author: s0965328
 */

#ifndef VNODE_H_
#define VNODE_H_

#include "ActNode.h"

namespace AutoDiff {
class VNode: public ActNode {
public:
	VNode(double v=NaN_Double);
	virtual ~VNode();
	void collect_vnodes(boost::unordered_set<Node*>& nodes,unsigned int& total);
	void eval_function();
	void grad_reverse_0();
	void grad_reverse_1();
	unsigned int hess_reverse_0();
	void hess_reverse_0_get_values(unsigned int i,double& x, double& x_bar, double& w, double& w_bar);
	void hess_reverse_1(unsigned int i);
	void hess_reverse_1_init_x_bar(unsigned int);
	void update_x_bar(unsigned int,double);
	void update_w_bar(unsigned int,double);
	void hess_reverse_1_get_xw(unsigned int,double&,double&);
	void hess_reverse_get_x(unsigned int,double& x);

	void nonlinearEdges(EdgeSet&);

	void inorder_visit(int level,ostream& oss);
	string toString(int level);
	TYPE getType();

#if FORWARD_ENABLED
	void hess_forward(unsigned int nvar, double** ret_vec);
	//! used for only forward hessian
	//! the id has to be assigned starting from 0
	int id;
	static int DEFAULT_ID;
#endif
	double val;
	double u;


};

} // end namespace foo

#endif /* VNODE_H_ */
