// The Computer Language Shootout
// http://shootout.alioth.debian.org/
// contributed by Isaac Gouy

function partial(n){
    var a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = a9 = 0.0;
    var twothirds = 2.0/3.0;
    var alt = -1.0;
    var k2 = k3 = sk = ck = 0.0;
    
    for (var k = 1; k <= n; k++){
        k2 = k*k;
        k3 = k2*k;
        sk = Math.sin(k);
        ck = Math.cos(k);
        alt = -alt;
        
        a1 += Math.pow(twothirds,k-1);
        a2 += Math.pow(k,-0.5);
        a3 += 1.0/(k*(k+1.0));
        a4 += 1.0/(k3 * sk*sk);
        a5 += 1.0/(k3 * ck*ck);
        a6 += 1.0/k;
        a7 += 1.0/k2;
        a8 += alt/k;
        a9 += alt/(2*k -1);
    }
    
    // NOTE: We don't try to validate anything from pow(),  sin() or cos() because those aren't
    // well-specified in ECMAScript.
    return a6 + a7 + a8 + a9;
}

var total = 0;

for (var i = 1024; i <= 16384; i *= 2) {
    total += partial(i);
}

var expected = 60.08994194659945;

if (total != expected) {
    throw "ERROR: bad result: expected " + expected + " but got " + total;
}

