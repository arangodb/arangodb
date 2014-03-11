Resemble.js
==========

Analyse and compare images with Javascript and HTML5. [Resemble.js Demo](http://huddle.github.com/Resemble.js/)

### Example

Retrieve basic analysis on image.

```javascript
var api = resemble(fileData).onComplete(function(data){
	console.log(data);
	/*
	{
	  red: 255,
	  green: 255,
	  blue: 255,
	  brightness: 255
	}
	*/
});
```

Use resemble to compare two images.

```javascript
var diff = resemble(file).compareTo(file2).ignoreColors().onComplete(function(data){
	console.log(data);
	/*
	{
	  misMatchPercentage : 100, // %
	  isSameDimensions: true, // or false
	  dimensionDifference: { width: 0, height: -1 }, // defined if dimensions are not the same
	  getImageDataUrl: function(){}
	}
	*/
});
```

You can also change the comparison method after the first analysis.

```javascript
// diff.ignoreNothing();
// diff.ignoreColors();
diff.ignoreAntialiasing();
```

--------------------------------------

Created by [James Cryer](http://github.com/jamescryer) and the Huddle development team.