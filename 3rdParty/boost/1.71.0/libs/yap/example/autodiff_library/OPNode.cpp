/*
 * OpNode.cpp
 *
 *  Created on: 8 Apr 2013
 *      Author: s0965328
 */

#include "OPNode.h"
#include "Stack.h"
#include "Tape.h"
#include "PNode.h"
/***********************************************************
	     h
	    / \
	   u   v  ----- hu  hv represent dh/du dh/dv resepectively.
	   - - -
	  x1....xn
***********************************************************/

namespace AutoDiff{

OPNode::OPNode(OPCODE op, Node* left) : ActNode(), op(op), left(left),val(NaN_Double) {
}

TYPE OPNode::getType()
{
	return OPNode_Type;
}

OPNode::~OPNode() {
	if(left->getType()!=VNode_Type)
	{
		delete left;
		this->left = NULL;
	}
}
}
