safe_numerics
=============

Arithmetic operations in C++ are NOT guaranteed to yield a correct mathematical result. This feature is inherited from the early days of C. The behavior of int, unsigned int and others were designed to map closely to the underlying hardware. Computer hardware implements these types as a fixed number of bits. When the result of arithmetic operations exceeds this number of bits, the result is undefined and usually not what the programmer intended. It is incumbent upon the C++ programmer to guarantee that this behavior does not result in incorrect behavior of the program. This library implements special versions of these data types which behave exactly like the original ones EXCEPT that the results of these operations are checked to be sure that an exception will be thrown anytime an attempt is made to store the result of an undefined operation.

Note: This is the subject of a various presentations at CPPCon.  

The first one is a short version which gives the main motivation for the library with a rowsing sales pitch.  Fun and suitable for upper management. https://www.youtube.com/watch?v=cw_8QkFXZjI&t=1s

The second is more extensive in that it addresses a real world case study which touches on most of the important aspects of the libary.  https://www.youtube.com/watch?v=93Cjg42bGEw .

Finally, for those who still enjoy the written word there is the documentation in which significant effort has been invested. http://htmlpreview.github.io/?https://github.com/robertramey/safe_numerics/master/doc/html/index.html

If you use this libary and find it useful, please add a star.  I need motivation!!!
