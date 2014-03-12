/*
Author: James Cryer
Company: Huddle
Last updated date: 03 Mar 2014
URL: https://github.com/Huddle/PhantomCSS
More: http://tldr.huddle.com/blog/css-testing/
*/

var fs = require('fs');

var _src = '.'+fs.separator+'screenshots';
var _results; // for backwards compatibility results and src are the same - but you can change it!
var _failures = '.'+fs.separator+'failures';

var _count = 0;
var _realPath;
var _diffsToProcess = [];
var _libraryRoot = '.';
var exitStatus;
var _hideElements;
var _addLabelToFailedImage = true;
var _test_match;
var _test_exclude;
var _mismatchTolerance = 0.05;
var diffsCreated = [];

exports.screenshot = screenshot;
exports.compareAll = compareAll;
exports.compareMatched = compareMatched;
exports.compareExplicit = compareExplicit;
exports.compareSession = compareSession;
exports.init = init;
exports.update = update;
exports.turnOffAnimations = turnOffAnimations;
exports.getExitStatus = getExitStatus;
exports.getCreatedDiffFiles = getCreatedDiffFiles;

function update(options){

	function stripslash ( str ){
		return str.replace(/\/\//g,'/').replace(/\\/g,'\\');
	}

	options = options || {};

	casper = options.casper || casper;
	_libraryRoot = options.libraryRoot || _libraryRoot;
	
	_src = stripslash(options.screenshotRoot || _src);
	_results = stripslash(options.comparisonResultRoot || _results || _src);
	_failures = stripslash(options.failedComparisonsRoot || _failures);
	
	_fileNameGetter = options.fileNameGetter || _fileNameGetter;

	_onPass = options.onPass || _onPass;
	_onFail = options.onFail || _onFail;
	_onTimeout = options.onTimeout || _onTimeout;
	_onComplete = options.onComplete || options.report || _onComplete;

	_hideElements = options.hideElements;

	_mismatchTolerance = options.mismatchTolerance || _mismatchTolerance;

	if(options.addLabelToFailedImage !== undefined){
		_addLabelToFailedImage = options.addLabelToFailedImage;
	}
}

function init(options){
	update(options);
}

function turnOffAnimations(){
	console.log('[PhantomCSS] Turning off animations');
	casper.evaluate(function turnOffAnimations(){
		window.addEventListener('load', function(){

			var css = document.createElement("style");
			css.type = "text/css";
			css.innerHTML = "* { -webkit-transition: none !important; transition: none !important; }";
			document.body.appendChild(css);

			if(jQuery){
				jQuery.fx.off = true;
			}
		},false);
	});
}

function _fileNameGetter(root, fileName){
	var name;

	fileName = fileName || "screenshot";
	name = root + fs.separator + fileName + "_" + _count++;

	if(fs.isFile(name+'.png')){
		return name+'.diff.png';
	} else {
		return name+'.png';
	}
}

function screenshot(selector, timeToWait, hideSelector, fileName){
	
	if(isNaN(Number(timeToWait)) && typeof timeToWait === 'string'){
		fileName = timeToWait;
		timeToWait = void 0;
	}

	casper.captureBase64('png'); // force pre-render
	casper.wait(timeToWait || 250, function(){

		var srcPath = _fileNameGetter(_src, fileName);
		var resultPath =  srcPath.replace(_src, _results);

		if(hideSelector || _hideElements){
			casper.evaluate(function(s1, s2){
				if(s1){
					$(s1).css('visibility', 'hidden');
				}
				$(s2).css('visibility', 'hidden');
			}, {
				s1: _hideElements,
				s2: hideSelector
			});
		}

		capture(srcPath, resultPath, selector);

	}); // give a bit of time for all the images appear
}

function capture(srcPath, resultPath, selector){
	var originalForResult = resultPath.replace('.diff', '');
	var originalFromSource = srcPath.replace('.diff', '');

	try {

		if( isThisImageADiff(resultPath) ){

			casper.captureSelector( resultPath , selector );
			
			diffsCreated.push(resultPath);

			if(srcPath !== resultPath){
				// also copy the original over to the result directory
				copyAndReplaceFile(originalFromSource, originalForResult);
			}

		} else {

			casper.captureSelector( srcPath , selector );

			if(srcPath !== resultPath){
				copyAndReplaceFile(srcPath, resultPath);
			}
		}

	}
	catch(ex){
		console.log("[PhantomCSS] Screenshot capture failed: ", ex.message);
	}
}

function isThisImageADiff(path){
	return /\.diff\.png/.test(path);
}

function copyAndReplaceFile(src, dest){
	if(fs.isFile(dest)){
		fs.remove(dest);
	}
	fs.copy(src, dest);
}

function asyncCompare(one, two, func){

	if(!casper.evaluate(function(){ return window._imagediff_;})){
		initClient();
	}

	casper.fill('form#image-diff', {
		'one': one,
		'two': two
	});

	casper.evaluate(function(filename){
		window._imagediff_.run( filename );
	}, {
		label: _addLabelToFailedImage ? one : false
	});

	casper.waitFor(
		function check() {
			return this.evaluate(function(){
				return window._imagediff_.hasResult;
			});
		},
		function () {

			var mismatch = casper.evaluate(function(){
				return window._imagediff_.getResult();
			});

			if(Number(mismatch)){
				func(false, mismatch);
			} else {
				func(true);
			}

		}, function(){
			func(false);
		},
		10000
	);
}

function getDiffs (path){

	var filePath;

	if(({'..':1,'.':1})[path]){ return true; }

	if(_realPath){
		_realPath += fs.separator + path;
	} else {
		_realPath = path;
	}

	filePath = _realPath;

	if(fs.isDirectory(_realPath) ){
		fs.list(_realPath).forEach(getDiffs);
	} else {
		if( /\.diff\./.test(path.toLowerCase()) ){
			if(_test_match){
				if( _test_match.test(_realPath.toLowerCase()) ){
					if( !(_test_exclude && _test_exclude.test(_realPath.toLowerCase())) ){
						console.log('[PhantomCSS] Analysing', _realPath);
						_diffsToProcess.push(filePath);
					}
				}
			} else {
				if( !(_test_exclude && _test_exclude.test(_realPath.toLowerCase())) ){
					_diffsToProcess.push(filePath);
				}
			}
		}
	}

	_realPath = _realPath.replace(fs.separator + path, '');
}

function getCreatedDiffFiles(){
	var d = diffsCreated;
	diffsCreated = [];
	return d;
}

function compareMatched(match, exclude){
	// Search for diff images, but only compare matched filenames
	_test_match = typeof match === 'string' ? new RegExp(match) : match;
	compareAll(exclude);
}

function compareExplicit(list){
	// An explicit list of diff images to compare ['/dialog.diff.png', '/header.diff.png']
	compareAll(void 0, list);
}

function compareSession(list){
	// compare the diffs created in this session
	compareAll(void 0, getCreatedDiffFiles() );
}

function compareAll(exclude, list){
	var tests = [];
	var fails = 0;
	var errors = 0;

	_test_exclude = typeof exclude === 'string' ? new RegExp(exclude) : exclude;
	
	if (list){
		_diffsToProcess = list;
	} else {
		_realPath = undefined;
		getDiffs(_results);
	}

	_diffsToProcess.forEach(function(file){

		var baseFile = file.replace('.diff', '');
		var html = _libraryRoot+fs.separator+"ResembleJs"+fs.separator+"resemblejscontainer.html";
		var test = {
			filename: baseFile
		};

		if(!fs.isFile(baseFile)) {
			test.error = true;
			errors++;
			tests.push(test);
		} else {

			if( !fs.isFile(html) ){
				console.log('[PhantomCSS] Can\'t find Resemble container. Perhaps the library root is mis configured. ('+html+')');
				return;
			}

			casper.thenOpen ( html , function (){

				asyncCompare(baseFile, file, function(isSame, mismatch){

					tests.push(test);

					if(!isSame){

						test.fail = true;
						fails++;

						casper.waitFor(
							function check() {
								return casper.evaluate(function(){
									return window._imagediff_.hasImage;
								});
							},
							function () {
								var failFile, safeFileName, increment;

								if(_failures){
									// flattened structure for failed diffs so that it is easier to preview
									failFile = _failures + fs.separator + file.split(fs.separator).pop().replace('.diff.png', '');
									safeFileName = failFile;
									increment = 0;

									while ( fs.isFile(safeFileName+'.fail.png') ){
										increment++;
										safeFileName = failFile+'.'+increment;
									}

									failFile = safeFileName + '.fail.png';

									casper.captureSelector(failFile, 'img');
								}

								casper.captureSelector(file.replace('.diff.png', '.fail.png'), 'img');

								casper.evaluate(function(){
									window._imagediff_.hasImage = false;
								});

								if(mismatch){
									test.mismatch = mismatch;
									_onFail(test); // casper.test.fail throws and error, this function call is aborted
									return;  // Just to make it clear what is happening
								} else {
									_onTimeout(test);
								}

							}, function(){},
							10000
						);
					} else {
						_onPass(test);
					}

				});
			});
		}
	});

	casper.then(function(){
		casper.waitFor(function(){
			return _diffsToProcess.length === tests.length;
		}, function(){
			_onComplete(tests, fails, errors);
		}, function(){

		},
		10000);
	});
}

function initClient(){

	casper.page.injectJs(_libraryRoot+fs.separator+'ResembleJs'+fs.separator+'resemble.js');

	casper.evaluate(function(mismatchTolerance){
		
		var result;

		var div = document.createElement('div');

		// this is a bit of hack, need to get images into browser for analysis
		div.style = "display:block;position:absolute;border:0;top:-1px;left:-1px;height:1px;width:1px;overflow:hidden;";
		div.innerHTML = '<form id="image-diff">'+
			'<input type="file" id="image-diff-one" name="one"/>'+
			'<input type="file" id="image-diff-two" name="two"/>'+
		'</form><div id="image-diff"></div>';
		document.body.appendChild(div);

		window._imagediff_ = {
			hasResult: false,
			hasImage: false,
			run: run,
			getResult: function(){
				window._imagediff_.hasResult = false;
				return result;
			}
		};

		function run(label){

			function render(data){
				var img = new Image();

				img.onload = function(){
					window._imagediff_.hasImage = true;
				};
				document.getElementById('image-diff').appendChild(img);
				img.src = data.getImageDataUrl(label);
			}

			resemble(document.getElementById('image-diff-one').files[0]).
				compareTo(document.getElementById('image-diff-two').files[0]).
				ignoreAntialiasing(). // <-- muy importante
				onComplete(function(data){
					var diffImage;

					if(Number(data.misMatchPercentage) > mismatchTolerance){
						result = data.misMatchPercentage;
					} else {
						result = false;
					}

					window._imagediff_.hasResult = true;

					if(Number(data.misMatchPercentage) > mismatchTolerance){
						render(data);
					}
					
				});
		}
	}, {
		mismatchTolerance: _mismatchTolerance
	});
}

function _onPass(test){
	console.log('\n');
	casper.test.pass('No changes found for screenshot ' + test.filename);
}
function _onFail(test){
	console.log('\n');
	casper.test.fail('Visual change found for screenshot ' + test.filename + ' (' + test.mismatch + '% mismatch)');
}
function _onTimeout(test){
	console.log('\n');
	casper.test.info('Could not complete image comparison for ' + test.filename);
}
function _onComplete(tests, noOfFails, noOfErrors){

	if( tests.length === 0){
		console.log("\nMust be your first time?");
		console.log("Some screenshots have been generated in the directory " + _results);
		console.log("This is your 'baseline', check the images manually. If they're wrong, delete the images.");
		console.log("The next time you run these tests, new screenshots will be taken.  These screenshots will be compared to the original.");
		console.log('If they are different, PhantomCSS will report a failure.');
	} else {
				
		if(noOfFails === 0){
			console.log("\nPhantomCSS found " + tests.length + " tests, None of them failed. Which is good right?");
			console.log("\nIf you want to make them fail, go change some CSS - weirdo.");
		} else {
			console.log("\nPhantomCSS found " + tests.length + " tests, " + noOfFails + ' of them failed.');
			console.log('\nPhantomCSS has created some images that try to show the difference (in the directory '+_failures+'). Fuchsia colored pixels indicate a difference betwen the new and old screenshots.');
		}

		if(noOfErrors !== 0){
			console.log("There were " + noOfErrors + "errors.  Is it possible that a baseline image was deleted but not the diff?");
		}

		exitStatus = noOfErrors+noOfFails;
	}
}

function getExitStatus() {
	return exitStatus;
}