/*
 * Tape.cpp
 *
 *  Created on: 6 Nov 2013
 *      Author: s0965328
 */



#include "Tape.h"

namespace AutoDiff
{
	template<> Tape<unsigned int>* Tape<unsigned int>::indexTape = NULL;
	template<> Tape<double>* Tape<double>::valueTape = NULL;
}
