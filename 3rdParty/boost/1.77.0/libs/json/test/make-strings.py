# Build array of random strings

import os
import random
from random import randint

def randstr():
    letters = "0123456789!@#$%^&*()_-+=[]{}|;:,<.>/?ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    return ''.join(random.choice(letters) for i in range(randint(1, 2000)));

print "[";
for i in range(1000):
    print "\"", randstr(), "\",";
print "\"", randstr(), "\"";
print "]";


