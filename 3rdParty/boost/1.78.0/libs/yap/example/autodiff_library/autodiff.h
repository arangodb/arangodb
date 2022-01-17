/*
 * autodiff.h
 *
 *  Created on: 16 Apr 2013
 *      Author: s0965328
 */

#ifndef AUTODIFF_H_
#define AUTODIFF_H_
#include <boost/unordered_set.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_sparse.hpp>
#include <boost/numeric/ublas/io.hpp>
#include "auto_diff_types.h"
#include "Node.h"
#include "VNode.h"
#include "OPNode.h"
#include "PNode.h"
#include "ActNode.h"
#include "EdgeSet.h"


/*
 * + Function and Gradient Evaluation
 * The tapeless implementation for function and derivative evaluation
 * Advantage for tapeless:
 * 			Low memory usage
 * 			Function evaluation use one stack
 * 			Gradient evaluation use two stack.
 * Disadvantage for tapeless:
 * 			Inefficient if the expression tree have repeated nodes.
 * 			for example:
 * 						 root
 * 						 /  \
 * 						*    *
 * 					   / \	/ \
 * 				      x1 x1 x1 x1
 * 				      Tapeless implementation will go through all the edges.
 * 				      ie. adjoint of x will be updated 4 times for the correct
 * 				      gradient of x1.While the tape implemenation can discovery this
 * 				      dependence and update adjoint of x1 just twice. The computational
 * 				      graph (DAG) for a taped implemenation will be look like bellow.
 * 				       root
 * 				        /\
 * 				        *
 * 				        /\
 * 				        x1
 *
 * + Forward Hessian Evaluation:
 * This is an inefficient implementation of the forward Hessian method. It will evaluate the diagonal
 * and upper triangular of the Hessian. The gradient is also evaluation in the same routine. The result
 * will be returned in an array.
 * To use this method, one have to provide a len parameter. len = (nvar+3)*nvar/2 where nvar is the number
 * of independent variables. ie. x_1 x_2 ... x_nvar. And the varaible id need to be a consequent integer start
 * with 0.
 * ret_vec will contains len number of doubles. Where the first nvar elements is the gradient vector,
 * and the rest of (nvar+1)*nvar/2 elements are the upper/lower plus the diagonal part of the Hessian matrix
 * in row format.
 * This algorithm is inefficient, because at each nodes, it didn't check the dependency of the independent
 * variables up to the current node. (or it is hard to do so for this setup). Therefore, it computes a full
 * for loops over each independent variable (ie. assume they are all dependent), for those independent
 * variables that are not dependent at the current node, zero will be produced by computation.
 * By default the forward mode hessian routing is disabled. To enable the forward hessian interface, the
 * compiler marco FORWARD_ENABLED need to be set equal to 1 in auto_diff_types.h
 *
 * + Reverse Hessian*Vector Evaluation:
 * Simple, building a tape in the forward pass, and a reverse pass will evaluate the Hessian*vector. The implemenation
 * also discovery the repeated subexpression and use one piece of memory on the tape for the same subexpression. This
 * allow efficient evaluation, because the repeated subexpression only evaluate once in the forward and reverse pass.
 * This algorithm can be called n times to compute a full Hessian, where n equals the number of independent
 * variables.
 * */

typedef boost::numeric::ublas::compressed_matrix<double,boost::numeric::ublas::column_major,0,std::vector<std::size_t>,std::vector<double> >  col_compress_matrix;
typedef boost::numeric::ublas::matrix_row<col_compress_matrix > col_compress_matrix_row;
typedef boost::numeric::ublas::matrix_column<col_compress_matrix  > col_compress_matrix_col;

namespace AutoDiff{

	//node creation methods
	extern PNode* create_param_node(double value);
	extern VNode* create_var_node(double v=NaN_Double);
	extern OPNode* create_uary_op_node(OPCODE code, Node* left);
	extern OPNode* create_binary_op_node(OPCODE code, Node* left,Node* right);

	//single constraint version
	extern double eval_function(Node* root);
	extern unsigned int nzGrad(Node* root);
	extern double grad_reverse(Node* root,vector<Node*>& nodes, vector<double>& grad);
	extern unsigned int nzHess(EdgeSet&);
	extern double hess_reverse(Node* root, vector<Node*>& nodes, vector<double>& dhess);

	//multiple constraints version
	extern unsigned int nzGrad(Node* root, boost::unordered_set<Node*>& vnodes);
	extern double grad_reverse(Node* root, vector<Node*>& nodes, col_compress_matrix_row& rgrad);
	extern unsigned int nzHess(EdgeSet&,boost::unordered_set<Node*>& set1, boost::unordered_set<Node*>& set2);
	extern double hess_reverse(Node* root, vector<Node*>& nodes, col_compress_matrix_col& chess);

#if FORWARD_ENDABLED
	//forward methods
	extern void hess_forward(Node* root, unsigned int nvar, double** hess_mat);
#endif

	//utiliy methods
	extern void nonlinearEdges(Node* root, EdgeSet& edges);
	extern unsigned int numTotalNodes(Node*);
	extern string tree_expr(Node* root);
	extern void print_tree(Node* root);
	extern void autodiff_setup();
	extern void autodiff_cleanup();
};

#endif /* AUTODIFF_H_ */
