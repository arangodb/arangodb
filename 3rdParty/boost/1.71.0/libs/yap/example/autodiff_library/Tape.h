/*
 * Tape.h
 *
 *  Created on: 15 Apr 2013
 *      Author: s0965328
 */

#ifndef TAPE_H_
#define TAPE_H_

#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <iostream>


namespace AutoDiff {

using namespace std;
#define TT 	(Tape<double>::valueTape)
#define II 	(Tape<unsigned int>::indexTape)

template<typename T> class Tape {
public:
	Tape<T> () : index(0){};
	T& at(const unsigned int index);
	const T& get(const unsigned int index);
	void set(T& v);
	unsigned int size();
	void clear();
	bool empty();
	string toString();
	virtual ~Tape();

	vector<T> vals;
	unsigned int index;

	static Tape<double>* valueTape;
	static Tape<unsigned int>* indexTape;
};


template<typename T> Tape<T>::~Tape<T>()
{
	index = 0;
	vals.clear();
}

template<typename T> T& Tape<T>::at(const unsigned int i)
{
	assert(this->vals.size()>i);
	return vals[i];
}
template<typename T> const T& Tape<T>::get(const unsigned int i)
{
	assert(this->vals.size()>i);
	return vals[i];
}
template <typename T> void Tape<T>::set(T& v)
{
	vals.push_back(v);
	index++;
}

template<typename T> unsigned int Tape<T>::size()
{
	return this->vals.size();
}

template<typename T> bool Tape<T>::empty()
{
	return vals.empty();
}

template<typename T> void Tape<T>::clear()
{
	this->vals.clear();
	this->index = 0;
	assert(this->vals.size()==0);
	assert(this->vals.empty());
}

template<typename T>  string Tape<T>::toString()
{
	assert(vals.size()>=index);
	ostringstream oss;
	oss<<"Tape size["<<vals.size()<<"]";
	for(unsigned int i=0;i<vals.size();i++){
		if(i%10==0) oss<<endl;
		oss<<vals[i]<<",";
	}
	oss<<endl;
	return oss.str();
}
}
#endif /* TAPE_H_ */
