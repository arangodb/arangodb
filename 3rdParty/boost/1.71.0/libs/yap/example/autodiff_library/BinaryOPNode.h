/*
 * BinaryOPNode.h
 *
 *  Created on: 6 Nov 2013
 *      Author: s0965328
 */

#ifndef BINARYOPNODE_H_
#define BINARYOPNODE_H_

#include "OPNode.h"

namespace AutoDiff {

class EdgeSet;

class BinaryOPNode: public OPNode {
public:

	static OPNode* createBinaryOpNode(OPCODE op, Node* left, Node* right);
	virtual ~BinaryOPNode();

	void collect_vnodes(boost::unordered_set<Node*>& nodes,unsigned int& total);
	void eval_function();

	void grad_reverse_0();
	void grad_reverse_1();

	void hess_forward(unsigned int len, double** ret_vec);

	unsigned int hess_reverse_0();
	void hess_reverse_0_init_n_in_arcs();
	void hess_reverse_0_get_values(unsigned int,double&, double&, double&, double&);
	void hess_reverse_1(unsigned int i);
	void hess_reverse_1_init_x_bar(unsigned int);
	void update_x_bar(unsigned int,double);
	void update_w_bar(unsigned int,double);
	void hess_reverse_1_get_xw(unsigned int, double&,double&);
	void hess_reverse_get_x(unsigned int,double& x);
	void hess_reverse_1_clear_index();

	void nonlinearEdges(EdgeSet& a);

	void inorder_visit(int level,ostream& oss);
	string toString(int level);

	Node* right;

private:
	BinaryOPNode(OPCODE op, Node* left, Node* right);
	void calc_eval_function();
	void calc_grad_reverse_0();
	void hess_forward_calc0(unsigned int& len, double* lvec, double* rvec,double* ret_vec);
};

} /* namespace AutoDiff */
#endif /* BINARYOPNODE_H_ */
