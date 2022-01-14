///////////////////////////////////////////////////////////////////////////////
//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/eigen.hpp>
#include <iostream>
#include <Eigen/Dense>
#include "test.hpp"

using namespace Eigen;

template <class T>
struct related_number
{
   typedef T type;
};

template <class Num>
void example1()
{
   // expected results first:
   Matrix<Num, 2, 2> r1, r2;
   r1 << 3, 5, 4, 8;
   r2 << -1, -1, 2, 0;
   Matrix<Num, 3, 1> r3;
   r3 << -1, -4, -6;

   Matrix<Num, 2, 2> a;
   a << 1, 2, 3, 4;
   Matrix<Num, Dynamic, Dynamic> b(2, 2);
   b << 2, 3, 1, 4;
   std::cout << "a + b =\n"
             << a + b << std::endl;
   BOOST_CHECK_EQUAL(a + b, r1);
   std::cout << "a - b =\n"
             << a - b << std::endl;
   BOOST_CHECK_EQUAL(a - b, r2);
   std::cout << "Doing a += b;" << std::endl;
   a += b;
   std::cout << "Now a =\n"
             << a << std::endl;
   Matrix<Num, 3, 1> v(1, 2, 3);
   Matrix<Num, 3, 1> w(1, 0, 0);
   std::cout << "-v + w - v =\n"
             << -v + w - v << std::endl;
   BOOST_CHECK_EQUAL(-v + w - v, r3);
}

template <class Num>
void example2()
{
   Matrix<Num, 2, 2> a;
   a << 1, 2, 3, 4;
   Matrix<Num, 3, 1> v(1, 2, 3);
   std::cout << "a * 2.5 =\n"
             << a * 2.5 << std::endl;
   std::cout << "0.1 * v =\n"
             << 0.1 * v << std::endl;
   std::cout << "Doing v *= 2;" << std::endl;
   v *= 2;
   std::cout << "Now v =\n"
             << v << std::endl;
   Num n(4);
   std::cout << "Doing v *= Num;" << std::endl;
   v *= n;
   std::cout << "Now v =\n"
             << v << std::endl;
   typedef typename related_number<Num>::type related_type;
   related_type                               r(6);
   std::cout << "Doing v *= RelatedType;" << std::endl;
   v *= r;
   std::cout << "Now v =\n"
             << v << std::endl;
   std::cout << "RelatedType * v =\n"
             << r * v << std::endl;
   std::cout << "Doing v *= RelatedType^2;" << std::endl;
   v *= r * r;
   std::cout << "Now v =\n"
             << v << std::endl;
   std::cout << "RelatedType^2 * v =\n"
             << r * r * v << std::endl;
}

template <class Num>
void example3()
{
   using namespace std;
   Matrix<Num, Dynamic, Dynamic> a = Matrix<Num, Dynamic, Dynamic>::Random(2, 2);
   cout << "Here is the matrix a\n"
        << a << endl;
   cout << "Here is the matrix a^T\n"
        << a.transpose() << endl;
   cout << "Here is the conjugate of a\n"
        << a.conjugate() << endl;
   cout << "Here is the matrix a^*\n"
        << a.adjoint() << endl;
}

template <class Num>
void example4()
{
   Matrix<Num, 2, 2> mat;
   mat << 1, 2,
       3, 4;
   Matrix<Num, 2, 1> u(-1, 1), v(2, 0);
   std::cout << "Here is mat*mat:\n"
             << mat * mat << std::endl;
   std::cout << "Here is mat*u:\n"
             << mat * u << std::endl;
   std::cout << "Here is u^T*mat:\n"
             << u.transpose() * mat << std::endl;
   std::cout << "Here is u^T*v:\n"
             << u.transpose() * v << std::endl;
   std::cout << "Here is u*v^T:\n"
             << u * v.transpose() << std::endl;
   std::cout << "Let's multiply mat by itself" << std::endl;
   mat = mat * mat;
   std::cout << "Now mat is mat:\n"
             << mat << std::endl;
}

template <class Num>
void example5()
{
   using namespace std;
   Matrix<Num, 3, 1> v(1, 2, 3);
   Matrix<Num, 3, 1> w(0, 1, 2);
   cout << "Dot product: " << v.dot(w) << endl;
   Num dp = v.adjoint() * w; // automatic conversion of the inner product to a scalar
   cout << "Dot product via a matrix product: " << dp << endl;
   cout << "Cross product:\n"
        << v.cross(w) << endl;
}

template <class Num>
void example6()
{
   using namespace std;
   Matrix<Num, 2, 2> mat;
   mat << 1, 2,
       3, 4;
   cout << "Here is mat.sum():       " << mat.sum() << endl;
   cout << "Here is mat.prod():      " << mat.prod() << endl;
   cout << "Here is mat.mean():      " << mat.mean() << endl;
   cout << "Here is mat.minCoeff():  " << mat.minCoeff() << endl;
   cout << "Here is mat.maxCoeff():  " << mat.maxCoeff() << endl;
   cout << "Here is mat.trace():     " << mat.trace() << endl;
}

template <class Num>
void example7()
{
   using namespace std;

   Array<Num, Dynamic, Dynamic> m(2, 2);

   // assign some values coefficient by coefficient
   m(0, 0) = 1.0;
   m(0, 1) = 2.0;
   m(1, 0) = 3.0;
   m(1, 1) = m(0, 1) + m(1, 0);

   // print values to standard output
   cout << m << endl
        << endl;

   // using the comma-initializer is also allowed
   m << 1.0, 2.0,
       3.0, 4.0;

   // print values to standard output
   cout << m << endl;
}

template <class Num>
void example8()
{
   using namespace std;
   Array<Num, Dynamic, Dynamic> a(3, 3);
   Array<Num, Dynamic, Dynamic> b(3, 3);
   a << 1, 2, 3,
       4, 5, 6,
       7, 8, 9;
   b << 1, 2, 3,
       1, 2, 3,
       1, 2, 3;

   // Adding two arrays
   cout << "a + b = " << endl
        << a + b << endl
        << endl;
   // Subtracting a scalar from an array
   cout << "a - 2 = " << endl
        << a - 2 << endl;
}

template <class Num>
void example9()
{
   using namespace std;
   Array<Num, Dynamic, Dynamic> a(2, 2);
   Array<Num, Dynamic, Dynamic> b(2, 2);
   a << 1, 2,
       3, 4;
   b << 5, 6,
       7, 8;
   cout << "a * b = " << endl
        << a * b << endl;
}

template <class Num>
void example10()
{
   using namespace std;
   Array<Num, Dynamic, 1> a = Array<Num, Dynamic, 1>::Random(5);
   a *= 2;
   cout << "a =" << endl
        << a << endl;
   cout << "a.abs() =" << endl
        << a.abs() << endl;
   cout << "a.abs().sqrt() =" << endl
        << a.abs().sqrt() << endl;
   cout << "a.min(a.abs().sqrt()) =" << endl
        << (a.min)(a.abs().sqrt()) << endl;
}

template <class Num>
void example11()
{
   using namespace std;
   Matrix<Num, Dynamic, Dynamic> m(2, 2);
   Matrix<Num, Dynamic, Dynamic> n(2, 2);
   Matrix<Num, Dynamic, Dynamic> result(2, 2);
   m << 1, 2,
       3, 4;
   n << 5, 6,
       7, 8;
   result = m * n;
   cout << "-- Matrix m*n: --" << endl
        << result << endl
        << endl;
   result = m.array() * n.array();
   cout << "-- Array m*n: --" << endl
        << result << endl
        << endl;
   result = m.cwiseProduct(n);
   cout << "-- With cwiseProduct: --" << endl
        << result << endl
        << endl;
   result = m.array() + 4;
   cout << "-- Array m + 4: --" << endl
        << result << endl
        << endl;
}

template <class Num>
void example12()
{
   using namespace std;
   Matrix<Num, Dynamic, Dynamic> m(2, 2);
   Matrix<Num, Dynamic, Dynamic> n(2, 2);
   Matrix<Num, Dynamic, Dynamic> result(2, 2);
   m << 1, 2,
       3, 4;
   n << 5, 6,
       7, 8;

   result = (m.array() + 4).matrix() * m;
   cout << "-- Combination 1: --" << endl
        << result << endl
        << endl;
   result = (m.array() * n.array()).matrix() * m;
   cout << "-- Combination 2: --" << endl
        << result << endl
        << endl;
}

template <class Num>
void example13()
{
   using namespace std;
   Matrix<Num, Dynamic, Dynamic> m(4, 4);
   m << 1, 2, 3, 4,
       5, 6, 7, 8,
       9, 10, 11, 12,
       13, 14, 15, 16;
   cout << "Block in the middle" << endl;
   cout << m.template block<2, 2>(1, 1) << endl
        << endl;
   for (int i = 1; i <= 3; ++i)
   {
      cout << "Block of size " << i << "x" << i << endl;
      cout << m.block(0, 0, i, i) << endl
           << endl;
   }
}

template <class Num>
void example14()
{
   using namespace std;
   Array<Num, 2, 2> m;
   m << 1, 2,
       3, 4;
   Array<Num, 4, 4> a = Array<Num, 4, 4>::Constant(0.6);
   cout << "Here is the array a:" << endl
        << a << endl
        << endl;
   a.template block<2, 2>(1, 1) = m;
   cout << "Here is now a with m copied into its central 2x2 block:" << endl
        << a << endl
        << endl;
   a.block(0, 0, 2, 3) = a.block(2, 1, 2, 3);
   cout << "Here is now a with bottom-right 2x3 block copied into top-left 2x2 block:" << endl
        << a << endl
        << endl;
}

template <class Num>
void example15()
{
   using namespace std;
   Eigen::Matrix<Num, Dynamic, Dynamic> m(3, 3);
   m << 1, 2, 3,
       4, 5, 6,
       7, 8, 9;
   cout << "Here is the matrix m:" << endl
        << m << endl;
   cout << "2nd Row: " << m.row(1) << endl;
   m.col(2) += 3 * m.col(0);
   cout << "After adding 3 times the first column into the third column, the matrix m is:\n";
   cout << m << endl;
}

template <class Num>
void example16()
{
   using namespace std;
   Matrix<Num, 4, 4> m;
   m << 1, 2, 3, 4,
       5, 6, 7, 8,
       9, 10, 11, 12,
       13, 14, 15, 16;
   cout << "m.leftCols(2) =" << endl
        << m.leftCols(2) << endl
        << endl;
   cout << "m.bottomRows<2>() =" << endl
        << m.template bottomRows<2>() << endl
        << endl;
   m.topLeftCorner(1, 3) = m.bottomRightCorner(3, 1).transpose();
   cout << "After assignment, m = " << endl
        << m << endl;
}

template <class Num>
void example17()
{
   using namespace std;
   Array<Num, Dynamic, 1> v(6);
   v << 1, 2, 3, 4, 5, 6;
   cout << "v.head(3) =" << endl
        << v.head(3) << endl
        << endl;
   cout << "v.tail<3>() = " << endl
        << v.template tail<3>() << endl
        << endl;
   v.segment(1, 4) *= 2;
   cout << "after 'v.segment(1,4) *= 2', v =" << endl
        << v << endl;
}

template <class Num>
void example18()
{
   using namespace std;
   Matrix<Num, 2, 2> mat;
   mat << 1, 2,
       3, 4;
   cout << "Here is mat.sum():       " << mat.sum() << endl;
   cout << "Here is mat.prod():      " << mat.prod() << endl;
   cout << "Here is mat.mean():      " << mat.mean() << endl;
   cout << "Here is mat.minCoeff():  " << mat.minCoeff() << endl;
   cout << "Here is mat.maxCoeff():  " << mat.maxCoeff() << endl;
   cout << "Here is mat.trace():     " << mat.trace() << endl;

   BOOST_CHECK_EQUAL(mat.sum(), 10);
   BOOST_CHECK_EQUAL(mat.prod(), 24);
   BOOST_CHECK_EQUAL(mat.mean(), Num(5) / 2);
   BOOST_CHECK_EQUAL(mat.minCoeff(), 1);
   BOOST_CHECK_EQUAL(mat.maxCoeff(), 4);
   BOOST_CHECK_EQUAL(mat.trace(), 5);
}

template <class Num>
void example18a()
{
   using namespace std;
   Matrix<Num, 2, 2> mat;
   mat << 1, 2,
       3, 4;
   cout << "Here is mat.sum():       " << mat.sum() << endl;
   cout << "Here is mat.prod():      " << mat.prod() << endl;
   cout << "Here is mat.mean():      " << mat.mean() << endl;
   //cout << "Here is mat.minCoeff():  " << mat.minCoeff() << endl;
   //cout << "Here is mat.maxCoeff():  " << mat.maxCoeff() << endl;
   cout << "Here is mat.trace():     " << mat.trace() << endl;
}

template <class Num>
void example19()
{
   using namespace std;
   Matrix<Num, Dynamic, 1>       v(2);
   Matrix<Num, Dynamic, Dynamic> m(2, 2), n(2, 2);

   v << -1,
       2;

   m << 1, -2,
       -3, 4;
   cout << "v.squaredNorm() = " << v.squaredNorm() << endl;
   cout << "v.norm() = " << v.norm() << endl;
   cout << "v.lpNorm<1>() = " << v.template lpNorm<1>() << endl;
   cout << "v.lpNorm<Infinity>() = " << v.template lpNorm<Infinity>() << endl;
   cout << endl;
   cout << "m.squaredNorm() = " << m.squaredNorm() << endl;
   cout << "m.norm() = " << m.norm() << endl;
   cout << "m.lpNorm<1>() = " << m.template lpNorm<1>() << endl;
   cout << "m.lpNorm<Infinity>() = " << m.template lpNorm<Infinity>() << endl;
}

template <class Num>
void example20()
{
   using namespace std;
   Matrix<Num, 3, 3> A;
   Matrix<Num, 3, 1> b;
   A << 1, 2, 3, 4, 5, 6, 7, 8, 10;
   b << 3, 3, 4;
   cout << "Here is the matrix A:\n"
        << A << endl;
   cout << "Here is the vector b:\n"
        << b << endl;
   Matrix<Num, 3, 1> x = A.colPivHouseholderQr().solve(b);
   cout << "The solution is:\n"
        << x << endl;
}

template <class Num>
void example21()
{
   using namespace std;
   Matrix<Num, 2, 2> A, b;
   A << 2, -1, -1, 3;
   b << 1, 2, 3, 1;
   cout << "Here is the matrix A:\n"
        << A << endl;
   cout << "Here is the right hand side b:\n"
        << b << endl;
   Matrix<Num, 2, 2> x = A.ldlt().solve(b);
   cout << "The solution is:\n"
        << x << endl;
}

template <class Num>
void example22()
{
   using namespace std;
   Matrix<Num, Dynamic, Dynamic> A              = Matrix<Num, Dynamic, Dynamic>::Random(100, 100);
   Matrix<Num, Dynamic, Dynamic> b              = Matrix<Num, Dynamic, Dynamic>::Random(100, 50);
   Matrix<Num, Dynamic, Dynamic> x              = A.fullPivLu().solve(b);
   Matrix<Num, Dynamic, Dynamic> axmb           = A * x - b;
   double                        relative_error = static_cast<double>(abs(axmb.norm() / b.norm())); // norm() is L2 norm
   cout << "norm1 = " << axmb.norm() << endl;
   cout << "norm2 = " << b.norm() << endl;
   cout << "The relative error is:\n"
        << relative_error << endl;
}

template <class Num>
void example23()
{
   using namespace std;
   Matrix<Num, 2, 2> A;
   A << 1, 2, 2, 3;
   cout << "Here is the matrix A:\n"
        << A << endl;
   SelfAdjointEigenSolver<Matrix<Num, 2, 2> > eigensolver(A);
   if (eigensolver.info() != Success)
   {
      std::cout << "Eigenvalue solver failed!" << endl;
   }
   else
   {
      cout << "The eigenvalues of A are:\n"
           << eigensolver.eigenvalues() << endl;
      cout << "Here's a matrix whose columns are eigenvectors of A \n"
           << "corresponding to these eigenvalues:\n"
           << eigensolver.eigenvectors() << endl;
   }
}

template <class Num>
void example24()
{
   using namespace std;
   Matrix<Num, 3, 3> A;
   A << 1, 2, 1,
       2, 1, 0,
       -1, 1, 2;
   cout << "Here is the matrix A:\n"
        << A << endl;
   cout << "The determinant of A is " << A.determinant() << endl;
   cout << "The inverse of A is:\n"
        << A.inverse() << endl;
}

template <class Num>
void test_integer_type()
{
   example1<Num>();
   //example2<Num>();
   example18<Num>();
}

template <class Num>
void test_float_type()
{
   std::cout << "Epsilon    = " << Eigen::NumTraits<Num>::epsilon() << std::endl;
   std::cout << "Dummy Prec = " << Eigen::NumTraits<Num>::dummy_precision() << std::endl;
   std::cout << "Highest    = " << Eigen::NumTraits<Num>::highest() << std::endl;
   std::cout << "Lowest     = " << Eigen::NumTraits<Num>::lowest() << std::endl;
   std::cout << "Digits10   = " << Eigen::NumTraits<Num>::digits10() << std::endl;

   example1<Num>();
   example2<Num>();
   example4<Num>();
   example5<Num>();
   example6<Num>();
   example7<Num>();
   example8<Num>();
   example9<Num>();
   example10<Num>();
   example11<Num>();
   example12<Num>();
   example13<Num>();
   example14<Num>();
   example15<Num>();
   example16<Num>();
   example17<Num>();
   /*
   example18<Num>();
   example19<Num>();
   example20<Num>();
   example21<Num>();
   example22<Num>();
   example23<Num>();
   example24<Num>();
   */
}

template <class Num>
void test_float_type_2()
{
   std::cout << "Epsilon    = " << Eigen::NumTraits<Num>::epsilon() << std::endl;
   std::cout << "Dummy Prec = " << Eigen::NumTraits<Num>::dummy_precision() << std::endl;
   std::cout << "Highest    = " << Eigen::NumTraits<Num>::highest() << std::endl;
   std::cout << "Lowest     = " << Eigen::NumTraits<Num>::lowest() << std::endl;
   std::cout << "Digits10   = " << Eigen::NumTraits<Num>::digits10() << std::endl;

   example18<Num>();
   example19<Num>();
   example20<Num>();
   example21<Num>();

   //example22<Num>();
   //example23<Num>();
   //example24<Num>();
}

template <class Num>
void test_float_type_3()
{
   std::cout << "Epsilon    = " << Eigen::NumTraits<Num>::epsilon() << std::endl;
   std::cout << "Dummy Prec = " << Eigen::NumTraits<Num>::dummy_precision() << std::endl;
   std::cout << "Highest    = " << Eigen::NumTraits<Num>::highest() << std::endl;
   std::cout << "Lowest     = " << Eigen::NumTraits<Num>::lowest() << std::endl;
   std::cout << "Digits10   = " << Eigen::NumTraits<Num>::digits10() << std::endl;

   example22<Num>();
   example23<Num>();
   example24<Num>();
}

template <class Num>
void test_complex_type()
{
   std::cout << "Epsilon    = " << Eigen::NumTraits<Num>::epsilon() << std::endl;
   std::cout << "Dummy Prec = " << Eigen::NumTraits<Num>::dummy_precision() << std::endl;
   std::cout << "Highest    = " << Eigen::NumTraits<Num>::highest() << std::endl;
   std::cout << "Lowest     = " << Eigen::NumTraits<Num>::lowest() << std::endl;
   std::cout << "Digits10   = " << Eigen::NumTraits<Num>::digits10() << std::endl;

   example1<Num>();
   example2<Num>();
   example3<Num>();
   example4<Num>();
   example5<Num>();
   example7<Num>();
   example8<Num>();
   example9<Num>();
   example11<Num>();
   example12<Num>();
   example13<Num>();
   example14<Num>();
   example15<Num>();
   example16<Num>();
   example17<Num>();
   example18a<Num>();
   example19<Num>();
   example20<Num>();
   example21<Num>();
   example22<Num>();
   // example23<Num>();  //requires comparisons.
   example24<Num>();
}
