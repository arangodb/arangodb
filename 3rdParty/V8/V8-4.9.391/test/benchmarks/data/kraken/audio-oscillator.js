var iterations = 500;

var bufferSize = 8192;
var sampleRate = 44100.0;
var nthHarmonic = 5;
var frequency = 344.53;
var sine = new Oscillator(Oscillator.Sine, frequency, 1, bufferSize, sampleRate);

var calcOsc = function() {
  sine.generate();
  harmonic = new Oscillator(Oscillator.Sine, frequency*nthHarmonic, 1/nthHarmonic, bufferSize, sampleRate);
  harmonic.generate();
  sine.add(harmonic);
};

runTest(calcOsc, iterations);
