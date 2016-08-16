.. Copyright David Abrahams 2006. Distributed under the Boost
.. Software License, Version 1.0. (See accompanying
.. file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

The zip iterator provides the ability to parallel-iterate
over several controlled sequences simultaneously. A zip 
iterator is constructed from a tuple of iterators. Moving
the zip iterator moves all the iterators in parallel.
Dereferencing the zip iterator returns a tuple that contains
the results of dereferencing the individual iterators. 

The tuple of iterators is now implemented in terms of a Boost fusion sequence. 
Because of this the 'tuple' may be any Boost fusion sequence and, for backwards
compatibility through a Boost fusion sequence adapter, a Boost tuple. Because the 
'tuple' may be any boost::fusion sequence the 'tuple' may also be any type for which a 
Boost fusion adapter exists. This includes, among others, a std::tuple and a std::pair.
Just remember to include the appropriate Boost fusion adapter header files for these
other Boost fusion adapters. The zip_iterator header file already includes the
Boost fusion adapter header file for Boost tuple, so you need not include it yourself
to use a Boost tuple as your 'tuple'.
