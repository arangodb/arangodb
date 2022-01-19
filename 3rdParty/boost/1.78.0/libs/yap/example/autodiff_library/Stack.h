/*
 * Stack.h
 *
 *  Created on: 15 Apr 2013
 *      Author: s0965328
 */

#ifndef STACK_H_
#define STACK_H_

#include <stack>

namespace AutoDiff {

using namespace std;
#define SV (Stack::vals)
#define SD (Stack::diff)

class Stack {
public:
	Stack();
	double pop_back();
	void push_back(double& v);
	double& peek();
	unsigned int size();
	void clear();
	virtual ~Stack();

	stack<double> lifo;

	static Stack* diff;
	static Stack* vals;


};

}
#endif /* STACK_H_ */
