#ifndef FST_LIB_STRING_WEIGHT_DECL_H__
#define FST_LIB_STRING_WEIGHT_DECL_H__

namespace fst {

// Determines whether to use left or right string semiring.  Includes a
// 'restricted' version that signals an error if proper prefixes/suffixes
// would otherwise be returned by Plus, useful with various
// algorithms that require functional transducer input with the
// string semirings.
enum StringType { STRING_LEFT = 0, STRING_RIGHT = 1, STRING_RESTRICT = 2 };

#define REVERSE_STRING_TYPE(S)                                  \
   ((S) == STRING_LEFT ? STRING_RIGHT :                         \
   ((S) == STRING_RIGHT ? STRING_LEFT :                        \
     STRING_RESTRICT))

template <typename L, StringType S = STRING_LEFT>
class StringWeight;

template <typename L, StringType S = STRING_LEFT>
class StringWeightIterator;

template <typename L, StringType S = STRING_LEFT>
class StringWeightReverseIterator;

}

#endif
