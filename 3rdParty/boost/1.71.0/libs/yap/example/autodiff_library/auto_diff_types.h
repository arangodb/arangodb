/*
 * auto_diff_types.h
 *
 *  Created on: 18 Apr 2013
 *      Author: s0965328
 */

#ifndef AUTO_DIFF_TYPES_H_
#define AUTO_DIFF_TYPES_H_

namespace AutoDiff{

#define FORWARD_ENABLED 0
#if FORWARD_ENABLED
extern unsigned int num_var;
#endif

#define NaN_Double std::numeric_limits<double>::quiet_NaN()

typedef enum { OPNode_Type=0, VNode_Type, PNode_Type} TYPE;


typedef enum {OP_PLUS=0, OP_MINUS, OP_TIMES, OP_DIVID, OP_SIN, OP_COS, OP_SQRT, OP_POW, OP_NEG} OPCODE;

}
#endif /* AUTO_DIFF_TYPES_H_ */
