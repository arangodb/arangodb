#!/usr/bin/env python

# Copyright Jim Bosch & Ankit Daftery 2010-2012.
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          http://www.boost.org/LICENSE_1_0.txt)

import ndarray_ext
import unittest
import numpy

class TestNdarray(unittest.TestCase):
    
    def testNdzeros(self):    
        for dtp in (numpy.int16, numpy.int32, numpy.float32, numpy.complex128):
            v = numpy.zeros(60, dtype=dtp)
            dt = numpy.dtype(dtp)
            for shape in ((60,),(6,10),(4,3,5),(2,2,3,5)):
                a1 = ndarray_ext.zeros(shape,dt)
                a2 = v.reshape(a1.shape)
                self.assertEqual(shape,a1.shape)
                self.assert_((a1 == a2).all())

    def testNdzeros_matrix(self):    
        for dtp in (numpy.int16, numpy.int32, numpy.float32, numpy.complex128):
            dt = numpy.dtype(dtp)
            shape = (6, 10)
            a1 = ndarray_ext.zeros_matrix(shape, dt)
            a2 = numpy.matrix(numpy.zeros(shape, dtype=dtp))
            self.assertEqual(shape,a1.shape)
            self.assert_((a1 == a2).all())
            self.assertEqual(type(a1), type(a2))

    def testNdarray(self):    
        a = range(0,60)
        for dtp in (numpy.int16, numpy.int32, numpy.float32, numpy.complex128):
            v = numpy.array(a, dtype=dtp)
            dt = numpy.dtype(dtp)
            a1 = ndarray_ext.array(a)
            a2 = ndarray_ext.array(a,dt)
            self.assert_((a1 == v).all())
            self.assert_((a2 == v).all())
            for shape in ((60,),(6,10),(4,3,5),(2,2,3,5)):
                a1 = a1.reshape(shape)
                self.assertEqual(shape,a1.shape)
                a2 = a2.reshape(shape)
                self.assertEqual(shape,a2.shape)

    def testNdempty(self):
        for dtp in (numpy.int16, numpy.int32, numpy.float32, numpy.complex128):
            dt = numpy.dtype(dtp)
            for shape in ((60,),(6,10),(4,3,5),(2,2,3,5)):
                a1 = ndarray_ext.empty(shape,dt)
                a2 = ndarray_ext.c_empty(shape,dt)
                self.assertEqual(shape,a1.shape)
                self.assertEqual(shape,a2.shape)

    def testTranspose(self):
        for dtp in (numpy.int16, numpy.int32, numpy.float32, numpy.complex128):
            dt = numpy.dtype(dtp)
            for shape in ((6,10),(4,3,5),(2,2,3,5)):
                a1 = numpy.empty(shape,dt)
                a2 = a1.transpose()
                a1 = ndarray_ext.transpose(a1)
                self.assertEqual(a1.shape,a2.shape)

    def testSqueeze(self):
        a1 = numpy.array([[[3,4,5]]])
        a2 = a1.squeeze()
        a1 = ndarray_ext.squeeze(a1)
        self.assertEqual(a1.shape,a2.shape)

    def testReshape(self):
        a1 = numpy.empty((2,2))
        a2 = ndarray_ext.reshape(a1,(1,4))
        self.assertEqual(a2.shape,(1,4))

    def testShapeIndex(self):
        a = numpy.arange(24)
        a.shape = (1,2,3,4)
        def shape_check(i):
            print(i)
            self.assertEqual(ndarray_ext.shape_index(a,i) ,a.shape[i] )
        for i in range(4):
            shape_check(i)
        for i in range(-1,-5,-1):
            shape_check(i)
        try:
            ndarray_ext.shape_index(a,4) # out of bounds -- should raise IndexError
            self.assertTrue(False)
        except IndexError:
            pass

    def testStridesIndex(self):
        a = numpy.arange(24)
        a.shape = (1,2,3,4)
        def strides_check(i):
            print(i)
            self.assertEqual(ndarray_ext.strides_index(a,i) ,a.strides[i] )
        for i in range(4):
            strides_check(i)
        for i in range(-1,-5,-1):
            strides_check(i)
        try:
            ndarray_ext.strides_index(a,4) # out of bounds -- should raise IndexError
            self.assertTrue(False)
        except IndexError:
            pass


if __name__=="__main__":
    unittest.main()
