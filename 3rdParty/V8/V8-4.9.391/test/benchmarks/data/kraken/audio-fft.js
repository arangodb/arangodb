var iterations = 1000;

var fft = new FFT(frameBufferLength / channels, rate);

var calcFFT = function() {
  var fb     = getFramebuffer(),
      signal = DSP.getChannel(DSP.MIX, fb);

  fft.forward(signal);
};

runTest(calcFFT, iterations);
