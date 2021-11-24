//=======================================================================
// Copyright 2002 Indiana University.
// Authors: Andrew Lumsdaine, Lie-Quan Lee, Jeremy G. Siek
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================

//
// The following test permutations are extracted from the old adj_list_test.cpp
// which generated code on the fly, but was never run as part of the regular
// tests.
//
#if TEST == 1
#define TEST_TYPE vecS
#define DIRECTED_TYPE bidirectionalS
#elif TEST == 2
#define TEST_TYPE vecS
#define DIRECTED_TYPE directedS
#elif TEST == 3
#define TEST_TYPE vecS
#define DIRECTED_TYPE undirectedS
#elif TEST == 4
#define TEST_TYPE listS
#define DIRECTED_TYPE bidirectionalS
#elif TEST == 5
#define TEST_TYPE listS
#define DIRECTED_TYPE directedS
#elif TEST == 6
#define TEST_TYPE listS
#define DIRECTED_TYPE undirectedS
#elif TEST == 7
#define TEST_TYPE setS
#define DIRECTED_TYPE bidirectionalS
#elif TEST == 8
#define TEST_TYPE setS
#define DIRECTED_TYPE directedS
#elif TEST == 9
#define TEST_TYPE setS
#define DIRECTED_TYPE undirectedS
#else
#error "No test combination specified - define macro TEST to the value 1 - 9."
#endif

#include <boost/graph/adjacency_list.hpp>
typedef boost::adjacency_list< boost::TEST_TYPE, boost::TEST_TYPE,
    boost::DIRECTED_TYPE, boost::property< vertex_id_t, std::size_t >,
    boost::property< edge_id_t, std::size_t > >
    Graph;
typedef boost::property< vertex_id_t, std::size_t > VertexId;
typedef boost::property< edge_id_t, std::size_t > EdgeID;
