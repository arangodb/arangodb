# Copyright 2011-2014 Mario Mulansky
# Copyright 2011-2014 Karsten Ahnert
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or
# copy at http://www.boost.org/LICENSE_1_0.txt)

# make sure BOOST_ROOT is pointing to your boost directory
# otherwise, set it here:
# BOOST_ROOT = /path/to/boost

INCLUDES += -I../../include/ -I$(BOOST_ROOT)
GCCFLAGS = -O3 -ffast-math -DNDEBUG
# disabling -ffast-math might give slightly better performance
ICCFLAGS = -Ofast -xHost -ip -inline-forceinline -DNDEBUG
# Possible options: -fp-model source -no-fma
GFORTFLAGS = -Ofast

bin/gcc:
	mkdir -p bin/gcc

bin/intel:
	mkdir -p bin/intel

bin/gfort:
	mkdir -p bin/gfort

bin/gcc/odeint_rk4_array: odeint_rk4_array.cpp bin/gcc
	g++ ${GCCFLAGS} ${INCLUDES} -o bin/gcc/odeint_rk4_array odeint_rk4_array.cpp

bin/gcc/c_lorenz: c_lorenz.c bin/gcc
	gcc -std=c99 -Ofast -mtune=corei7-avx c_lorenz.c -o bin/gcc/c_lorenz 

bin/intel/odeint_rk4_array: odeint_rk4_array.cpp bin/intel
	icpc ${ICCFLAGS} ${INCLUDES} -o bin/intel/odeint_rk4_array odeint_rk4_array.cpp

bin/intel/c_lorenz: c_lorenz.c bin/intel
	icc -std=c99 -Ofast -xHost -ansi-alias -o bin/intel/c_lorenz c_lorenz.c

bin/gfort/fortran_lorenz: fortran_lorenz.f90 bin/gfort
	gfortran ${GFORTFLAGS} fortran_lorenz.f90 -o bin/gfort/fortran_lorenz

all: bin/gcc/odeint_rk4_array bin/intel/odeint_rk4_array bin/gcc/c_lorenz bin/intel/c_lorenz bin/gfort/fortran_lorenz
