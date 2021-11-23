/*
 * PNode.h
 *
 *  Created on: 8 Apr 2013
 *      Author: s0965328
 */

#ifndef PNODE_H_
#define PNODE_H_

#include "Node.h"
namespace AutoDiff {

using namespace std;
class PNode: public Node {
public:
	PNode(double value);
	virtual ~PNode();
	void collect_vnodes(boost::unordered_set<Node*>& nodes,unsigned int& total);
	void eval_function();
	void grad_reverse_0();
	void grad_reverse_1_init_adj();
	void grad_reverse_1();
	void update_adj(double& v);
	void hess_forward(unsigned int len,double** ret_vec);
	unsigned int hess_reverse_0();
	void hess_reverse_0_get_values(unsigned int i,double& x,double& x_bar,double& w,double& w_bar);
	void hess_reverse_1(unsigned int i);
	void hess_reverse_1_init_x_bar(unsigned int);
	void update_x_bar(unsigned int, double);
	void update_w_bar(unsigned int, double);
	void hess_reverse_1_get_xw(unsigned int, double&,double&);
	void hess_reverse_get_x(unsigned int,double& x);
	
	void nonlinearEdges(EdgeSet&);

	void inorder_visit(int level,ostream& oss);
	string toString(int level);
	TYPE getType();

	double pval;

};

} // end namespace foo

#endif /* PNODE_H_ */
