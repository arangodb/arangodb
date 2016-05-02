//print("i: " + i + "j: " + j);

function gaussianBlur() {
    for (var y = 0; y < height; ++y) {
	for (var x = 0; x < width; ++x) {
	    var r = 0, g = 0, b = 0, a = 0;
	    for (var j = 1 - kernelSize; j < kernelSize; ++j) {
		if (y + j < 0 || y + j >= height) continue;
		for (var i = 1 - kernelSize; i < kernelSize; ++i) {
		    if (x + i < 0 || x + i >= width) continue;
		    r += squidImageData[4 * ((y + j) * width + (x + i)) + 0] * kernel[Math.abs(j)][Math.abs(i)];
		    g += squidImageData[4 * ((y + j) * width + (x + i)) + 1] * kernel[Math.abs(j)][Math.abs(i)];
		    b += squidImageData[4 * ((y + j) * width + (x + i)) + 2] * kernel[Math.abs(j)][Math.abs(i)];
		    a += squidImageData[4 * ((y + j) * width + (x + i)) + 3] * kernel[Math.abs(j)][Math.abs(i)];
		}
	    }
	    squidImageData[4 * (y * width + x) + 0] = r / kernelSum;
	    squidImageData[4 * (y * width + x) + 1] = g / kernelSum;
	    squidImageData[4 * (y * width + x) + 2] = b / kernelSum;
	    squidImageData[4 * (y * width + x) + 3] = a / kernelSum;
	}
    }
    return squidImageData;
}
gaussianBlur();