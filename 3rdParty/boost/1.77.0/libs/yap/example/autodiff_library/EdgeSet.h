/*
 * EdgeSet.h
 *
 *  Created on: 12 Nov 2013
 *      Author: s0965328
 */

#ifndef EDGESET_H_
#define EDGESET_H_

#include "Edge.h"
#include <list>

namespace AutoDiff {

class EdgeSet {
public:
	EdgeSet();
	virtual ~EdgeSet();

	void insertEdge(Edge& e);
	bool containsEdge(Edge& e);
	unsigned int numSelfEdges();
	void clear();
	unsigned int size();
	std::string toString();


	std::list<Edge> edges;
};

} /* namespace AutoDiff */
#endif /* EDGESET_H_ */
