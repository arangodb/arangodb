/*
 * Node.h
 *
 *  Created on: 8 Apr 2013
 *      Author: s0965328
 */

#ifndef NODE_H_
#define NODE_H_

#include <boost/unordered_set.hpp>
#include "auto_diff_types.h"

using namespace std;

namespace AutoDiff {

class EdgeSet;

class Node {
public:
	Node();
	virtual ~Node();

	virtual void eval_function() = 0;
	virtual void grad_reverse_0() = 0;
	virtual void grad_reverse_1_init_adj() = 0;
	virtual void grad_reverse_1() = 0;
	virtual void update_adj(double& v) = 0;
	virtual unsigned int hess_reverse_0() = 0;
	virtual void hess_reverse_0_init_n_in_arcs();
	virtual void hess_reverse_0_get_values(unsigned int,double&, double&,double&, double&) = 0;
	virtual void hess_reverse_1(unsigned int i) = 0;
	virtual void hess_reverse_1_init_x_bar(unsigned int) = 0;
	virtual void update_x_bar(unsigned int,double) = 0;
	virtual void update_w_bar(unsigned int,double) = 0;
	virtual void hess_reverse_1_get_xw(unsigned int, double&,double&) = 0;
	virtual void hess_reverse_get_x(unsigned int,double& x)=0;
	virtual void hess_reverse_1_clear_index();
	//routing for checking non-zero structures
	virtual void collect_vnodes(boost::unordered_set<Node*>& nodes,unsigned int& total) = 0;
	virtual void nonlinearEdges(EdgeSet&) = 0;
#if FORWARD_ENABLED
	virtual void hess_forward(unsigned int len, double** ret_vec) = 0;
#endif

	//other utility methods
	virtual void inorder_visit( int level,ostream& oss) = 0;
	virtual string toString(int levl) = 0;
	virtual TYPE getType() = 0;


	//! index on the tape
	unsigned int index;
	//! number of incoming arcs
	//! n_in_arcs in root node equals 1 before evaluation and 0 after evaluation
	unsigned int n_in_arcs;

	static unsigned int DEFAULT_INDEX;

};

} // end namespace foo

#endif /* NODE_H_ */
