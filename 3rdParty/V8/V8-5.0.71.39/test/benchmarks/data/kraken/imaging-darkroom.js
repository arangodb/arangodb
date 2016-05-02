var width = 400, height = 267;
for (var index = 0; index < paramArray.length; index++) {
   var data = squidImageData;
   data = ProcessImageData(data, width, height, paramArray[index]);
}