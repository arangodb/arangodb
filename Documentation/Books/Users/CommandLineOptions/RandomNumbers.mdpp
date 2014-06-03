!SUBSECTION Command-Line Options for Random Numbers

`--random.generator arg`

The argument is an integer (1,2,3 or 4) which sets the manner in which random numbers are generated. The default method (3) is to use the a non-blocking random (or pseudorandom) number generator supplied by the operating system. Specifying an argument of 2, uses a blocking random (or pseudorandom) number generator. Specifying an argument 1 sets a pseudorandom number generator using an implication of the Mersenne Twister MT19937 algorithm. Algorithm 4 is a combination of the blocking random number generator and the Mersenne Twister.

`--random.no-seed`

By default, the random generator is seeded. Setting this option causes the random number generator not to be seeded. (Seeding the random number generator only occurs if the generator is set to Mersenne, refer to random.generator for details.)

<!--
@anchor CommandLineRandomGenerator
@copydetails triagens::rest::ApplicationServer::_randomGenerator
-->