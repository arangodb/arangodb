/*
 * ActNode.h
 *
 *  Created on: 13 Apr 2013
 *      Author: s0965328
 */

#ifndef ACTNODE_H_
#define ACTNODE_H_

#include "Node.h"


namespace AutoDiff {

class ActNode : public Node{
public:
	ActNode();
	virtual ~ActNode();

	void update_adj(double& v);
	void grad_reverse_1_init_adj();

	double adj;
};

} // end namespace foo

#endif /* ACTNODE_H_ */
