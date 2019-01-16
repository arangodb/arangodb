# Copyright David Abrahams 2004. Distributed under the Boost
# Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
import unittest
import sys

class PolymorphTest(unittest.TestCase):

   def testReturnCpp(self):

      # Python Created Object With Same Id As
      # Cpp Created B Object 
      # b = B(872)  

      #  Get Reference To Cpp Created B Object
      a = getBCppObj()

      # Python Created B Object and Cpp B Object
      # Should have same result by calling f()
      self.assertEqual ('B::f()', a.f())
      self.assertEqual ('B::f()', call_f(a))
      self.assertEqual ('A::f()', call_f(A()))

   def test_references(self):
      # B is not exposed to Python
      a = getBCppObj()
      self.assertEqual(type(a), A)

      # C is exposed to Python
      c = getCCppObj()
      self.assertEqual(type(c), C)
      
   def test_factory(self):
      self.assertEqual(type(factory(0)), A)
      self.assertEqual(type(factory(1)), A)
      self.assertEqual(type(factory(2)), C)

   def test_return_py(self):

      class X(A):
         def f(self):
            return 'X.f'

      x = X()
      
      self.assertEqual ('X.f', x.f())
      self.assertEqual ('X.f', call_f(x))

   def test_self_default(self):

      class X(A):
         def f(self):
            return 'X.f() -> ' + A.f(self)

      x = X()
      
      self.assertEqual ('X.f() -> A::f()', x.f())
      
      # This one properly raises the "dangling reference" exception
      # self.failUnlessEqual ('X.f() -> A::f()', call_f(x))

   def test_wrapper_downcast(self):
      a = pass_a(D())
      self.assertEqual('D::g()', a.g())

   def test_pure_virtual(self):
      p = P()
      self.assertRaises(RuntimeError, p.f)
      
      q = Q()
      self.assertEqual ('Q::f()', q.f())
      
      class R(P):
         def f(self):
            return 'R.f'

      r = R()
      self.assertEqual ('R.f', r.f())
      

def test():
   # remove the option that upsets unittest
   import sys
   sys.argv = [ x for x in sys.argv if x != '--broken-auto-ptr' ]
   unittest.main()

# This nasty hack basically says that if we're loaded by another module, we'll
# be testing polymorphism2_auto_ptr_ext instead of polymorphism2_ext.
if __name__ == "__main__":
   from polymorphism2_ext import *
   test()
else:
   from polymorphism2_auto_ptr_ext import *
   
