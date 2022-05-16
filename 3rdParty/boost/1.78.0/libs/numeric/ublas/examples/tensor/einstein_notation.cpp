//
//  Copyright (c) 2018-2019, Cem Bassoy, cem.bassoy@gmail.com
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  The authors gratefully acknowledge the support of
//  Fraunhofer IOSB, Ettlingen, Germany
//


#include <boost/numeric/ublas/tensor.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <iostream>

int main()
{
	using namespace boost::numeric::ublas;

	using format_t  = column_major;
	using value_t   = float;
	using tensor_t = tensor<value_t,format_t>;
	using matrix_t = matrix<value_t,format_t>;
	using namespace boost::numeric::ublas::index;

	// Tensor-Vector-Multiplications - Including Transposition
	{

		auto n = shape{3,4,2};
		auto A = tensor_t(n,1);
		auto B1 = matrix_t(n[1],n[2],2);
		auto v1 = tensor_t(shape{n[0],1},2);
		auto v2 = tensor_t(shape{n[1],1},2);
//		auto v3 = tensor_t(shape{n[2],1},2);

		// C1(j,k) = B1(j,k) + A(i,j,k)*v1(i);
		// tensor_t C1 = B1 + prod(A,vector_t(n[0],1),1);
//		tensor_t C1 = B1 + A(_i,_,_) * v1(_i,_);

		// C2(i,k) = A(i,j,k)*v2(j) + 4;
		//tensor_t C2 = prod(A,vector_t(n[1],1),2) + 4;
//		tensor_t C2 = A(_,_i,_) * v2(_i,_) + 4;

		// not yet implemented!
		// C3() = A(i,j,k)*T1(i)*T2(j)*T2(k);		
		// tensor_t C3 = prod(prod(prod(A,v1,1),v2,1),v3,1);
		// tensor_t C3 = A(_i,_j,_k) * v1(_i,_) * v2(_j,_) * v3(_k,_);

		// formatted output
		std::cout << "% --------------------------- " << std::endl;
		std::cout << "% --------------------------- " << std::endl << std::endl;
		std::cout << "% C1(j,k) = B1(j,k) + A(i,j,k)*v1(i);" << std::endl << std::endl;
//		std::cout << "C1=" << C1 << ";" << std::endl << std::endl;

		// formatted output
		std::cout << "% --------------------------- " << std::endl;
		std::cout << "% --------------------------- " << std::endl << std::endl;
		std::cout << "% C2(i,k) = A(i,j,k)*v2(j) + 4;" << std::endl << std::endl;
//		std::cout << "C2=" << C2 << ";" << std::endl << std::endl;

	}


	// Tensor-Matrix-Multiplications - Including Transposition
	{
		auto n = shape{3,4,2};
		auto m = 5u;
		auto A = tensor_t(n,2);
		auto B  = tensor_t(shape{n[1],n[2],m},2);
		auto B1 = tensor_t(shape{m,n[0]},1);
		auto B2 = tensor_t(shape{m,n[1]},1);


		// C1(l,j,k) = B(j,k,l) + A(i,j,k)*B1(l,i);
		// tensor_t C1 = B + prod(A,B1,1);
//		tensor_t C1 = B + A(_i,_,_) * B1(_,_i);

		// C2(i,l,k) = A(i,j,k)*B2(l,j) + 4;
		// tensor_t C2 = prod(A,B2) + 4;
//		tensor_t C2 =  A(_,_j,_) * B2(_,_j) + 4;

		// C3(i,l1,l2) = A(i,j,k)*T1(l1,j)*T2(l2,k);
		// not yet implemented.

		// formatted output
		std::cout << "% --------------------------- " << std::endl;
		std::cout << "% --------------------------- " << std::endl << std::endl;
		std::cout << "% C1(l,j,k) = B(j,k,l) + A(i,j,k)*B1(l,i);" << std::endl << std::endl;
//		std::cout << "C1=" << C1 << ";" << std::endl << std::endl;

		// formatted output
		std::cout << "% --------------------------- " << std::endl;
		std::cout << "% --------------------------- " << std::endl << std::endl;
		std::cout << "% C2(i,l,k) = A(i,j,k)*B2(l,j) + 4;" << std::endl << std::endl;
//		std::cout << "C2=" << C2 << ";" << std::endl << std::endl;

//		// formatted output
//		std::cout << "% --------------------------- " << std::endl;
//		std::cout << "% --------------------------- " << std::endl << std::endl;
//		std::cout << "% C3(i,l1,l2) = A(i,j,k)*T1(l1,j)*T2(l2,k);" << std::endl << std::endl;
//		std::cout << "C3=" << C3 << ";" << std::endl << std::endl;
	}


	// Tensor-Tensor-Multiplications Including Transposition
	{
		auto na = shape{3,4,5};
		auto nb = shape{4,6,3,2};
		auto A = tensor_t(na,2);
		auto B = tensor_t(nb,3);
		auto T1 = tensor_t(shape{na[2],na[2]},2);
		auto T2 = tensor_t(shape{na[2],nb[1],nb[3]},2);


		// C1(j,l) = T1(j,l) + A(i,j,k)*A(i,j,l) + 5;
		// tensor_t C1 = T1 + prod(A,A,perm_t{1,2}) + 5;
//		tensor_t C1 = T1 + A(_i,_j,_m)*A(_i,_j,_l) + 5;

		// formatted output
		std::cout << "% --------------------------- " << std::endl;
		std::cout << "% --------------------------- " << std::endl << std::endl;
		std::cout << "% C1(k,l) = T1(k,l) + A(i,j,k)*A(i,j,l) + 5;" << std::endl << std::endl;
//		std::cout << "C1=" << C1 << ";" << std::endl << std::endl;


		// C2(k,l,m) = T2(k,l,m) + A(i,j,k)*B(j,l,i,m) + 5;
		//tensor_t C2 = T2 + prod(A,B,perm_t{1,2},perm_t{3,1}) + 5;
//		tensor_t C2 = T2 + A(_i,_j,_k)*B(_j,_l,_i,_m) + 5;

		// formatted output
		std::cout << "% --------------------------- " << std::endl;
		std::cout << "% --------------------------- " << std::endl << std::endl;
		std::cout << "%  C2(k,l,m) = T2(k,l,m) + A(i,j,k)*B(j,l,i,m) + 5;" << std::endl << std::endl;
//		std::cout << "C2=" << C2 << ";" << std::endl << std::endl;

	}
}
