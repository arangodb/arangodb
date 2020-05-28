/*
 * ActNode.cpp
 *
 *  Created on: 13 Apr 2013
 *      Author: s0965328
 */

#include "ActNode.h"

namespace AutoDiff {

ActNode::ActNode() : AutoDiff::Node(),adj(NaN_Double){

}

ActNode::~ActNode() {
}


void ActNode::update_adj(double& v)
{
	assert(!isnan(adj));
	assert(!isnan(v));
	adj+=v;
}

void ActNode::grad_reverse_1_init_adj()
{
	adj = 1;
}

}


