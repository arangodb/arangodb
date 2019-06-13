/*
 * UaryOPNode.h
 *
 *  Created on: 6 Nov 2013
 *      Author: s0965328
 */

#ifndef UARYOPNODE_H_
#define UARYOPNODE_H_

#include "OPNode.h"

namespace AutoDiff {

class UaryOPNode: public OPNode {
public:
	static OPNode* createUnaryOpNode(OPCODE op, Node* left);
	virtual ~UaryOPNode();

	void inorder_visit(int level,ostream& oss);
	void collect_vnodes(boost::unordered_set<Node*> & nodes,unsigned int& total);
	void eval_function();

	void grad_reverse_0();
	void grad_reverse_1();
#if FORWARD_ENABLED
	void hess_forward(unsigned int len, double** ret_vec);
#endif
	unsigned int hess_reverse_0();
	void hess_reverse_0_init_n_in_arcs();
	void hess_reverse_0_get_values(unsigned int i,double& x, double& x_bar, double& w, double& w_bar);
	void hess_reverse_1(unsigned int i);
	void hess_reverse_1_init_x_bar(unsigned int);
	void update_x_bar(unsigned int, double v);
	void update_w_bar(unsigned int, double v);
	void hess_reverse_1_get_xw(unsigned int, double&,double&);
	void hess_reverse_get_x(unsigned int,double& x);
	void hess_reverse_1_clear_index();

	void nonlinearEdges(EdgeSet&);

	string toString(int level);

private:
	UaryOPNode(OPCODE op, Node* left);
	void calc_eval_function();
	void calc_grad_reverse_0();
	void hess_forward_calc0(unsigned int& len, double* lvec,double* ret_vec);
};

} /* namespace AutoDiff */
#endif /* UARYOPNODE_H_ */
