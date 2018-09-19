/*
 * EdgeSet.cpp
 *
 *  Created on: 12 Nov 2013
 *      Author: s0965328
 */

#include "EdgeSet.h"
#include "Edge.h"
#include <sstream>

using namespace std;
namespace AutoDiff {

EdgeSet::EdgeSet() {
	// TODO Auto-generated constructor stub

}

EdgeSet::~EdgeSet() {
	edges.clear();
}

bool EdgeSet::containsEdge(Edge& e)
{
	list<Edge>::iterator it = edges.begin();
	for(;it!= edges.end();it++){
		Edge eit = *it;
		if(eit.isEqual(e))
		{
			return true;
		}
	}
	return false;
}

void EdgeSet::insertEdge(Edge& e) {
	if(!containsEdge(e)){
		edges.push_front(e);
	}
}

void EdgeSet::clear() {
	edges.clear();
}

unsigned int EdgeSet::size(){
	return edges.size();
}

unsigned int EdgeSet::numSelfEdges(){
	unsigned int diag = 0;
	list<Edge>::iterator it = edges.begin();
	for(;it!=edges.end();it++)
	{
		Edge eit = *it;
		if(eit.a == eit.b)
		{
			diag++;
		}
	}
	return diag;
}

string EdgeSet::toString()
{
	ostringstream oss;
	list<Edge>::iterator it = edges.begin();
	for(;it!=edges.end();it++)
	{
		oss<<(*it).toString()<<endl;
	}
	return oss.str();
}

} /* namespace AutoDiff */
