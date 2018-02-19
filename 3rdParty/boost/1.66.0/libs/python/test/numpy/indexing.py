#!/usr/bin/env python

# Copyright Jim Bosch & Ankit Daftery 2010-2012.
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          http://www.boost.org/LICENSE_1_0.txt)

import unittest
import numpy
import indexing_ext

class TestIndexing(unittest.TestCase):

    def testSingle(self):
        x = numpy.arange(0,10)
        for i in range(0,10):
            numpy.testing.assert_equal(indexing_ext.single(x,i), i)
        for i in range(-10,0):
            numpy.testing.assert_equal(indexing_ext.single(x,i),10+i)

    def testSlice(self):
        x = numpy.arange(0,10)
        sl = slice(3,8)
        b = [3,4,5,6,7]
        numpy.testing.assert_equal(indexing_ext.slice(x,sl), b)

    def testStepSlice(self):
        x = numpy.arange(0,10)
        sl = slice(3,8,2)
        b = [3,5,7]
        numpy.testing.assert_equal(indexing_ext.slice(x,sl), b)

    def testIndex(self):
        x = numpy.arange(0,10)
        chk = numpy.array([3,4,5,6])
        numpy.testing.assert_equal(indexing_ext.indexarray(x,chk),chk)
        chk = numpy.array([[0,1],[2,3]])
        numpy.testing.assert_equal(indexing_ext.indexarray(x,chk),chk)
        x = numpy.arange(9).reshape(3,3)
        y = numpy.array([0,1])
        z = numpy.array([0,2])
        chk = numpy.array([0,5])
        numpy.testing.assert_equal(indexing_ext.indexarray(x,y,z),chk)
        x = numpy.arange(0,10)
        b = x>4
        chk = numpy.array([5,6,7,8,9])
        numpy.testing.assert_equal(indexing_ext.indexarray(x,b),chk)
        x = numpy.arange(9).reshape(3,3)
        b = numpy.array([0,2])
        sl = slice(0,3)
        chk = numpy.array([[0,1,2],[6,7,8]])
        numpy.testing.assert_equal(indexing_ext.indexslice(x,b,sl),chk)

if __name__=="__main__":
    unittest.main()
