// Copyright (C) 2018 Alain Miniussi <alain.miniussi -at- oca.eu>.

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/** @file seq.hpp
 *
 *  Minimal MPI dummy declaration.
 * We need those when we want a sequential (eg one process) MPI version of the code.
 */
#ifndef BOOST_MPI_SEQ_HPP
#define BOOST_MPI_SEQ_HPP

namespace boost { namespace mpi {
int const MPI_SUCCESS    = 0;
int const MPI_ANY_SOURCE = -2;
int const MPI_ANY_TAG    = -1;
} }

#endif // BOOST_MPI_SEQ_HPP
