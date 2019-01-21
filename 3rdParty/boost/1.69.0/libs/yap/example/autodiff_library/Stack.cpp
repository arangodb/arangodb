/*
 * Stack.cpp
 *
 *  Created on: 15 Apr 2013
 *      Author: s0965328
 */


#include <cstddef>
#include <math.h>
#include <cassert>

#include "Stack.h"

namespace AutoDiff {


Stack* Stack::vals = NULL;
Stack* Stack::diff = NULL;

Stack::Stack()
{
}

Stack::~Stack() {
	this->clear();
}


double Stack::pop_back()
{
	assert(this->lifo.size()!=0);
	double v = this->lifo.top();
	lifo.pop();
	return v;
}
void Stack::push_back(double& v)
{
	assert(!isnan(v));
	this->lifo.push(v);
}
double& Stack::peek()
{
	return this->lifo.top();
}
unsigned int Stack::size()
{
	return this->lifo.size();
}

void Stack::clear()
{
	while(!this->lifo.empty())
	{
		this->lifo.pop();
	}
}
}
