// The Computer Language Shootout
// http://shootout.alioth.debian.org/
// contributed by Isaac Gouy

function ack(m,n){
   if (m==0) { return n+1; }
   if (n==0) { return ack(m-1,1); }
   return ack(m-1, ack(m,n-1) );
}

function fib(n) {
    if (n < 2){ return 1; }
    return fib(n-2) + fib(n-1);
}

function tak(x,y,z) {
    if (y >= x) return z;
    return tak(tak(x-1,y,z), tak(y-1,z,x), tak(z-1,x,y));
}

var result = 0;

for ( var i = 3; i <= 5; i++ ) {
    result += ack(3,i);
    result += fib(17.0+i);
    result += tak(3*i+3,2*i+2,i+1);
}

var expected = 57775;
if (result != expected)
    throw "ERROR: bad result: expected " + expected + " but got " + result;

