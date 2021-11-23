/*
 * Edge.cpp
 *
 *  Created on: 12 Nov 2013
 *      Author: s0965328
 */

#include "Edge.h"
#include <iostream>
#include <sstream>

namespace AutoDiff {

Edge::Edge(Node* a_,Node* b_):a(a_),b(b_) {

}

Edge::~Edge() {
	a = NULL;
	b = NULL;
}

Edge::Edge(const Edge& e)
{
	a = e.a;
	b = e.b;
}

bool Edge::isEqual(Edge* e)
{
	if(e->a == a && e->b == b)
	{
		return true;
	}
	else if(e->b == a && e->a == b)
	{
		return true;
	}
	return false;
}

bool Edge::isEqual(Edge& e)
{
	return isEqual(&e);
}

string Edge::toString()
{
	ostringstream oss;
	oss<<""<<a->toString(0)<<"|"<<a<<" ----- "<<b->toString(0)<<"|"<<b<<""<<endl;
	return oss.str();
}

} /* namespace AutoDiff */
