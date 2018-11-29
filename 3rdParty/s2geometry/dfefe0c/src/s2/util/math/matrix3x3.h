// Copyright 2003 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//
// A simple class to handle 3x3 matrices
// The aim of this class is to be able to manipulate 3x3 matrices
// and 3D vectors as naturally as possible and make calculations
// readable.
// For that reason, the operators +, -, * are overloaded.
// (Reading a = a + b*2 - c is much easier to read than
// a = Sub(Add(a, Mul(b,2)),c)   )
//
// Please be careful about overflows when using those matrices wth integer types
// The calculations are carried with VType. eg : if you are using uint8 as the
// base type, all values will be modulo 256.
// This feature is necessary to use the class in a more general framework with
// VType != plain old data type.

#ifndef S2_UTIL_MATH_MATRIX3X3_H_
#define S2_UTIL_MATH_MATRIX3X3_H_

#include <cmath>
#include <iosfwd>
#include <type_traits>

#include "s2/base/logging.h"
#include "s2/util/math/mathutil.h"
#include "s2/util/math/vector.h"

template <class VType>
class Matrix3x3 {
 private:
  VType m_[3][3];

 public:
  typedef Matrix3x3<VType> Self;
  typedef VType BaseType;
  typedef Vector3<VType> MVector;

  // Initialize the matrix to 0
  Matrix3x3() {
    m_[0][2] = m_[0][1] = m_[0][0] = VType();
    m_[1][2] = m_[1][1] = m_[1][0] = VType();
    m_[2][2] = m_[2][1] = m_[2][0] = VType();
  }

  // Constructor explicitly setting the values of all the coefficient of
  // the matrix
  Matrix3x3(const VType &m00, const VType &m01, const VType &m02,
            const VType &m10, const VType &m11, const VType &m12,
            const VType &m20, const VType &m21, const VType &m22) {
    m_[0][0] = m00;
    m_[0][1] = m01;
    m_[0][2] = m02;

    m_[1][0] = m10;
    m_[1][1] = m11;
    m_[1][2] = m12;

    m_[2][0] = m20;
    m_[2][1] = m21;
    m_[2][2] = m22;
  }

  // Casting constructor
  template <class VType2>
  static Matrix3x3 Cast(const Matrix3x3<VType2> &mb) {
    return Matrix3x3(static_cast<VType>(mb(0, 0)),
                     static_cast<VType>(mb(0, 1)),
                     static_cast<VType>(mb(0, 2)),
                     static_cast<VType>(mb(1, 0)),
                     static_cast<VType>(mb(1, 1)),
                     static_cast<VType>(mb(1, 2)),
                     static_cast<VType>(mb(2, 0)),
                     static_cast<VType>(mb(2, 1)),
                     static_cast<VType>(mb(2, 2)));
  }

  // Change the value of all the coefficients of the matrix
  inline Matrix3x3 &
    Set(const VType &m00, const VType &m01, const VType &m02,
        const VType &m10, const VType &m11, const VType &m12,
        const VType &m20, const VType &m21, const VType &m22) {
    m_[0][0] = m00;
    m_[0][1] = m01;
    m_[0][2] = m02;

    m_[1][0] = m10;
    m_[1][1] = m11;
    m_[1][2] = m12;

    m_[2][0] = m20;
    m_[2][1] = m21;
    m_[2][2] = m22;
    return (*this);
  }

  // Matrix addition
  inline Matrix3x3& operator+=(const Matrix3x3 &mb) {
    m_[0][0] += mb.m_[0][0];
    m_[0][1] += mb.m_[0][1];
    m_[0][2] += mb.m_[0][2];

    m_[1][0] += mb.m_[1][0];
    m_[1][1] += mb.m_[1][1];
    m_[1][2] += mb.m_[1][2];

    m_[2][0] += mb.m_[2][0];
    m_[2][1] += mb.m_[2][1];
    m_[2][2] += mb.m_[2][2];
    return (*this);
  }

  // Matrix subtration
  inline Matrix3x3& operator-=(const Matrix3x3 &mb) {
    m_[0][0] -= mb.m_[0][0];
    m_[0][1] -= mb.m_[0][1];
    m_[0][2] -= mb.m_[0][2];

    m_[1][0] -= mb.m_[1][0];
    m_[1][1] -= mb.m_[1][1];
    m_[1][2] -= mb.m_[1][2];

    m_[2][0] -= mb.m_[2][0];
    m_[2][1] -= mb.m_[2][1];
    m_[2][2] -= mb.m_[2][2];
    return (*this);
  }

  // Matrix multiplication by a scalar
  inline Matrix3x3& operator*=(const VType &k) {
    m_[0][0] *= k;
    m_[0][1] *= k;
    m_[0][2] *= k;

    m_[1][0] *= k;
    m_[1][1] *= k;
    m_[1][2] *= k;

    m_[2][0] *= k;
    m_[2][1] *= k;
    m_[2][2] *= k;
    return (*this);
  }

  // Matrix addition
  inline Matrix3x3 operator+(const Matrix3x3 &mb) const {
    return Matrix3x3(*this) += mb;
  }

  // Matrix subtraction
  inline Matrix3x3 operator-(const Matrix3x3 &mb) const {
    return Matrix3x3(*this) -= mb;
  }

  // Change the sign of all the coefficients in the matrix
  friend inline Matrix3x3 operator-(const Matrix3x3 &vb) {
    return Matrix3x3(-vb.m_[0][0], -vb.m_[0][1], -vb.m_[0][2],
                     -vb.m_[1][0], -vb.m_[1][1], -vb.m_[1][2],
                     -vb.m_[2][0], -vb.m_[2][1], -vb.m_[2][2]);
  }

  // Matrix multiplication by a scalar
  inline Matrix3x3 operator*(const VType &k) const {
    return Matrix3x3(*this) *= k;
  }

  friend inline Matrix3x3 operator*(const VType &k, const Matrix3x3 &mb) {
    return Matrix3x3(mb)*k;
  }

  // Matrix multiplication
  inline Matrix3x3 operator*(const Matrix3x3 &mb) const {
    return Matrix3x3(
      m_[0][0] * mb.m_[0][0] + m_[0][1] * mb.m_[1][0] + m_[0][2] * mb.m_[2][0],
      m_[0][0] * mb.m_[0][1] + m_[0][1] * mb.m_[1][1] + m_[0][2] * mb.m_[2][1],
      m_[0][0] * mb.m_[0][2] + m_[0][1] * mb.m_[1][2] + m_[0][2] * mb.m_[2][2],

      m_[1][0] * mb.m_[0][0] + m_[1][1] * mb.m_[1][0] + m_[1][2] * mb.m_[2][0],
      m_[1][0] * mb.m_[0][1] + m_[1][1] * mb.m_[1][1] + m_[1][2] * mb.m_[2][1],
      m_[1][0] * mb.m_[0][2] + m_[1][1] * mb.m_[1][2] + m_[1][2] * mb.m_[2][2],

      m_[2][0] * mb.m_[0][0] + m_[2][1] * mb.m_[1][0] + m_[2][2] * mb.m_[2][0],
      m_[2][0] * mb.m_[0][1] + m_[2][1] * mb.m_[1][1] + m_[2][2] * mb.m_[2][1],
      m_[2][0] * mb.m_[0][2] + m_[2][1] * mb.m_[1][2] + m_[2][2] * mb.m_[2][2]);
  }

  // Multiplication of a matrix by a vector
  inline MVector operator*(const MVector &v) const {
    return MVector(
      m_[0][0] * v[0] + m_[0][1] * v[1] + m_[0][2] * v[2],
      m_[1][0] * v[0] + m_[1][1] * v[1] + m_[1][2] * v[2],
      m_[2][0] * v[0] + m_[2][1] * v[1] + m_[2][2] * v[2]);
  }

  // Return the determinant of the matrix
  inline VType Det(void) const {
    return m_[0][0] * m_[1][1] * m_[2][2]
         + m_[0][1] * m_[1][2] * m_[2][0]
         + m_[0][2] * m_[1][0] * m_[2][1]
         - m_[2][0] * m_[1][1] * m_[0][2]
         - m_[2][1] * m_[1][2] * m_[0][0]
         - m_[2][2] * m_[1][0] * m_[0][1];
  }

  // Return the trace of the matrix
  inline VType Trace(void) const {
    return m_[0][0] + m_[1][1] + m_[2][2];
  }

  // Return a pointer to the data array for interface with other libraries
  // like opencv
  VType* Data() {
    return reinterpret_cast<VType*>(m_);
  }
  const VType* Data() const {
    return reinterpret_cast<const VType*>(m_);
  }

  // Return matrix element (i,j) with 0<=i<=2 0<=j<=2
  inline VType &operator()(const int i, const int j) {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 3);
    S2_DCHECK_GE(j, 0);
    S2_DCHECK_LT(j, 3);
    return m_[i][j];
  }
  inline VType operator()(const int i, const int j) const {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 3);
    S2_DCHECK_GE(j, 0);
    S2_DCHECK_LT(j, 3);
    return m_[i][j];
  }

  // Return matrix element (i/3,i%3) with 0<=i<=8 (access concatenated rows).
  inline VType &operator[](const int i) {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 9);
    return reinterpret_cast<VType*>(m_)[i];
  }
  inline VType operator[](const int i) const {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 9);
    return reinterpret_cast<const VType*>(m_)[i];
  }

  // Return the transposed matrix
  inline Matrix3x3 Transpose(void) const {
    return Matrix3x3(m_[0][0], m_[1][0], m_[2][0],
                     m_[0][1], m_[1][1], m_[2][1],
                     m_[0][2], m_[1][2], m_[2][2]);
  }

  // Return the transposed of the matrix of the cofactors
  // (Useful for inversion for example)
  inline Matrix3x3 ComatrixTransposed(void) const {
    return Matrix3x3(
      m_[1][1] * m_[2][2] - m_[2][1] * m_[1][2],
      m_[2][1] * m_[0][2] - m_[0][1] * m_[2][2],
      m_[0][1] * m_[1][2] - m_[1][1] * m_[0][2],

      m_[1][2] * m_[2][0] - m_[2][2] * m_[1][0],
      m_[2][2] * m_[0][0] - m_[0][2] * m_[2][0],
      m_[0][2] * m_[1][0] - m_[1][2] * m_[0][0],

      m_[1][0] * m_[2][1] - m_[2][0] * m_[1][1],
      m_[2][0] * m_[0][1] - m_[0][0] * m_[2][1],
      m_[0][0] * m_[1][1] - m_[1][0] * m_[0][1]);
  }
  // Matrix inversion
  inline Matrix3x3 Inverse(void) const {
    VType det = Det();
    S2_CHECK_NE(det, VType(0)) << " Can't inverse. Determinant = 0.";
    return (VType(1) / det) * ComatrixTransposed();
  }

  // Return the vector 3D at row i
  inline MVector Row(const int i) const {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 3);
    return MVector(m_[i][0], m_[i][1], m_[i][2]);
  }

  // Return the vector 3D at col i
  inline MVector Col(const int i) const {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 3);
    return MVector(m_[0][i], m_[1][i], m_[2][i]);
  }

  // Create a matrix from 3 row vectors
  static inline Matrix3x3 FromRows(const MVector &v1,
                              const MVector &v2,
                              const MVector &v3) {
    Matrix3x3 temp;
    temp.Set(v1[0], v1[1], v1[2],
             v2[0], v2[1], v2[2],
             v3[0], v3[1], v3[2]);
    return temp;
  }

  // Create a matrix from 3 column vectors
  static inline Matrix3x3 FromCols(const MVector &v1,
                              const MVector &v2,
                              const MVector &v3) {
    Matrix3x3 temp;
    temp.Set(v1[0], v2[0], v3[0],
             v1[1], v2[1], v3[1],
             v1[2], v2[2], v3[2]);
    return temp;
  }

  // Set the vector in row i to be v1
  void SetRow(int i, const MVector &v1) {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 3);
    m_[i][0] = v1[0];
    m_[i][1] = v1[1];
    m_[i][2] = v1[2];
  }

  // Set the vector in column i to be v1
  void SetCol(int i, const MVector &v1) {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 3);
    m_[0][i] = v1[0];
    m_[1][i] = v1[1];
    m_[2][i] = v1[2];
  }

  // Return a matrix M close to the original but verifying MtM = I
  // (useful to compensate for errors in a rotation matrix)
  Matrix3x3 Orthogonalize() const {
    MVector r1, r2, r3;
    r1 = Row(0).Normalize();
    r2 = (Row(2).CrossProd(r1)).Normalize();
    r3 = (r1.CrossProd(r2)).Normalize();
    return FromRows(r1, r2, r3);
  }

  // Return the identity matrix
  static inline Matrix3x3 Identity(void) {
    Matrix3x3 temp;
    temp.Set(VType(1), VType(0), VType(0),  //
             VType(0), VType(1), VType(0),  //
             VType(0), VType(0), VType(1));
    return temp;
  }

  // Return a matrix full of zeros
  static inline Matrix3x3 Zero(void) {
    return Matrix3x3();
  }

  // Return a diagonal matrix with the coefficients in v
  static inline Matrix3x3 Diagonal(const MVector &v) {
    return Matrix3x3(v[0], VType(), VType(),
                     VType(), v[1], VType(),
                     VType(), VType(), v[2]);
  }

  // Return the matrix vvT
  static Matrix3x3 Sym3(const MVector &v) {
    return Matrix3x3(
      v[0]*v[0], v[0]*v[1], v[0]*v[2],
      v[1]*v[0], v[1]*v[1], v[1]*v[2],
      v[2]*v[0], v[2]*v[1], v[2]*v[2]);
  }
  // Return a matrix M such that:
  // for each u,  M * u = v.CrossProd(u)
  static Matrix3x3 AntiSym3(const MVector &v) {
    return Matrix3x3(VType(),    -v[2],     v[1],
                     v[2],     VType(),    -v[0],
                     -v[1],       v[0],   VType());
  }

  // Returns matrix that rotates |rot| radians around axis rot.
  static Matrix3x3 Rodrigues(const MVector &rot) {
    Matrix3x3 R;
    VType theta = rot.Norm();
    MVector w = rot.Normalize();
    Matrix3x3 Wv = Matrix3x3::AntiSym3(w);
    Matrix3x3 I = Matrix3x3::Identity();
    Matrix3x3 A = Matrix3x3::Sym3(w);
    R = (1 - cos(theta)) * A + sin(theta) * Wv + cos(theta) * I;
    return R;
  }

  // Returns v.Transpose() * (*this) * u
  VType MulBothSides(const MVector &v, const MVector &u) const {
    return ((*this) * u).DotProd(v);
  }

  // Use the 3x3 matrix as a projective transform for 2d points
  Vector2<VType> Project(const Vector2<VType> &v) const {
    MVector temp = (*this) * MVector(v[0], v[1], 1);
    return Vector2<VType>(temp[0] / temp[2], temp[1] / temp[2]);
  }

  // Return the Frobenius norm of the matrix: sqrt(sum(aij^2))
  VType FrobeniusNorm() const {
    VType sum = VType();
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        sum += m_[i][j] * m_[i][j];
      }
    }
    return sqrt(sum);
  }

  // Finds the eigen values of the matrix. Return the number of real eigenvalues
  // found
  int EigenValues(MVector *eig_val) const {
    long double r1, r2, r3;  // NOLINT
    // characteristic polynomial is
    // x^3 + (a11*a22+a22*a33+a33*a11)*x^2 - trace(A)*x - det(A)
    VType a = -Trace();
    VType b = m_[0][0]*m_[1][1] + m_[1][1]*m_[2][2] + m_[2][2]*m_[0][0]
            - m_[1][0]*m_[0][1] - m_[2][1]*m_[1][2] - m_[0][2]*m_[2][0];
    VType c = -Det();
    bool res = MathUtil::RealRootsForCubic(a, b, c, &r1, &r2, &r3);
    (*eig_val)[0] = r1;
    if (res) {
      (*eig_val)[1] = r2;
      (*eig_val)[2] = r3;
      return 3;
    }
    return 1;
  }

  // Finds the eigen values and associated eigen vectors of a
  // symmetric positive definite 3x3 matrix,eigen values are
  // sorted in decreasing order, eig_val corresponds to the
  // columns of the eig_vec matrix.
  // Note: The routine will only use the lower diagonal part
  // of the matrix, i.e.
  // |  a00,          |
  // |  a10, a11,     |
  // |  a20, a21, a22 |
  void SymmetricEigenSolver(MVector *eig_val,
                            Matrix3x3 *eig_vec) const {
    // Compute characteristic polynomial coefficients
    double c2 = -(m_[0][0] + m_[1][1] + m_[2][2]);
    double c1 = -(m_[1][0] * m_[1][0] - m_[0][0] * m_[1][1]
                  - m_[0][0] * m_[2][2] - m_[1][1] * m_[2][2]
                  + m_[2][0] * m_[2][0] + m_[2][1] * m_[2][1]);
    double c0 = -(m_[0][0] * m_[1][1] * m_[2][2] - m_[2][0]
                  * m_[2][0] * m_[1][1] - m_[1][0] * m_[1][0]
                  * m_[2][2] - m_[0][0] * m_[2][1] * m_[2][1]
                  + 2 * m_[1][0] * m_[2][0] * m_[2][1]);

    // Root finding
    double q = (c2*c2-3*c1)/9.0;
    double r = (2*c2*c2*c2-9*c2*c1+27*c0)/54.0;
    // Assume R^3 <Q^3 so there are three real roots
    if (q < 0) q = 0;
    double sqrt_q = -2.0 * sqrt(q);
    double theta = acos(r / sqrt(q * q * q));
    double c2_3 = c2 / 3;
    (*eig_val)[0] = sqrt_q * cos(theta / 3.0) - c2_3;
    (*eig_val)[1] = sqrt_q * cos((theta + 2.0 * M_PI)/3.0) - c2_3;
    (*eig_val)[2] = sqrt_q * cos((theta - 2.0 * M_PI)/3.0) - c2_3;

    // Sort eigen value in decreasing order
    Vector3<int> d_order = eig_val->ComponentOrder();
    (*eig_val) = MVector((*eig_val)[d_order[2]],
                         (*eig_val)[d_order[1]],
                         (*eig_val)[d_order[0]]);
    // Compute eigenvectors
    for (int i = 0; i < 3; ++i) {
      MVector r1 , r2 , r3 , e1 , e2 , e3;
      r1[0] = m_[0][0] - (*eig_val)[i];
      r2[0] = m_[1][0];
      r3[0] = m_[2][0];
      r1[1] = m_[1][0];
      r2[1] = m_[1][1] - (*eig_val)[i];
      r3[1] = m_[2][1];
      r1[2] = m_[2][0];
      r2[2] = m_[2][1];
      r3[2] = m_[2][2] - (*eig_val)[i];

      e1 = r1.CrossProd(r2);
      e2 = r2.CrossProd(r3);
      e3 = r3.CrossProd(r1);

      // Make e2 and e3 point in the same direction as e1
      if (e2.DotProd(e1) < 0) e2 = -e2;
      if (e3.DotProd(e1) < 0) e3 = -e3;
      MVector e = (e1 + e2 + e3).Normalize();
      eig_vec->SetCol(i, e);
    }
  }

  // Return true is one of the elements of the matrix is NaN
  bool IsNaN() const {
    for ( int i = 0; i < 3; ++i ) {
      for ( int j = 0; j < 3; ++j ) {
        if ( isnan(m_[i][j]) ) {
          return true;
        }
      }
    }
    return false;
  }

  friend bool operator==(const Matrix3x3 &a, const Matrix3x3 &b) {
    return a.m_[0][0] == b.m_[0][0] &&
           a.m_[0][1] == b.m_[0][1] &&
           a.m_[0][2] == b.m_[0][2] &&
           a.m_[1][0] == b.m_[1][0] &&
           a.m_[1][1] == b.m_[1][1] &&
           a.m_[1][2] == b.m_[1][2] &&
           a.m_[2][0] == b.m_[2][0] &&
           a.m_[2][1] == b.m_[2][1] &&
           a.m_[2][2] == b.m_[2][2];
  }

  friend bool operator!=(const Matrix3x3 &a, const Matrix3x3 &b) {
    return !(a == b);
  }

  friend std::ostream &operator <<(std::ostream &out, const Matrix3x3 &mb) {
    int i, j;
    for (i = 0; i < 3; i++) {
      if (i ==0) {
        out << "[";
      } else {
        out << " ";
      }
      for (j = 0; j < 3; j++) {
        out << mb(i, j) << " ";
      }
      if (i == 2) {
        out << "]";
      } else {
        out << std::endl;
      }
    }
    return out;
  }
};

typedef Matrix3x3<int>    Matrix3x3_i;
typedef Matrix3x3<float>  Matrix3x3_f;
typedef Matrix3x3<double> Matrix3x3_d;


#endif  // S2_UTIL_MATH_MATRIX3X3_H_
