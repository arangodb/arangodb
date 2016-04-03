var iterations = 1000;

var fft = fft = new FFT(frameBufferLength / channels, rate);
var bd = new BeatDetektor();
var kick_det = new BeatDetektor.modules.vis.BassKick();
var vu = new BeatDetektor.modules.vis.VU();

var calcBeat = function() {
  var fb     = getFramebuffer(), signal = DSP.getChannel(DSP.MIX, fb);

  fft.forward(signal);

  var timestamp = (new Date()).getTime();
  bd.process(timestamp, fft.spectrum);
  kick_det.process(bd);
  vu.process(bd);
};

runTest(calcBeat, iterations);