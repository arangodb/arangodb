[![Build Status](https://secure.travis-ci.org/soldair/node-walkdir.png)](http://travis-ci.org/soldair/node-walkdir)
 
## walkdir

Find files. Walks a directory tree emitting events based on what it finds. Presents a familliar callback/emitter/sync interface. Walk a tree of any depth. This is a performant option any pull requests to make it more so will be taken into consderation.. 

## Example

```js

var walk = require('walkdir');

//async with path callback 

walk('../',function(path,stat){
  console.log('found: ',path);
});

//use async emitter to capture more events

var emitter = walk('../');

emitter.on('file',function(filename,stat){
  console.log('file from emitter: ', filename);
});


//sync with callback

walk.sync('../',function(path,stat){
  console.log('found sync:',path);
});

//sync just need paths

var paths = walk.sync('../');
console.log('found paths sync: ',paths);

```


## install

	npm install walkdir

## arguments

walkdir(path, [options], [callback])
walkdir.sync(path, [options], [callback]);

- path
  - the starting point of your directory walk

- options. supported options are
  - general

	```js
	{
	"follow_symlinks":false, // default is off 
	"no_recurse":false,      // only recurse one level deep
	"max_depth":undefined    // only recurse down to max_depth. if you need more than no_recurse
	}
	```

  - sync only

	```js
	{
	"return_object":false, // if true the sync return will be in {path:stat} format instead of [path,path,...]
	"no_return":false, // if true null will be returned and no array or object will be created with found paths. useful for large listings
	}
	```

- callback
  - this is bound to the path event of the emitter. its optional in all cases.

	```js
	callback(path,stat)
	```

## events

non error type events are emitted with (path,stat). stat is an instanceof fs.Stats

###path
fired for everything

###file
fired only for regular files

###directory
fired only for directories

###link
fired when a symbolic link is found

###end
fired when the entire tree has been read and emitted.

###socket
fired when a socket descriptor is found

###fifo
fired when a fifo is found

###characterdevice
fired when a character device is found

###blockdevice
fired when a block device is found

###targetdirectory
fired for the stat of the path you provided as the first argument. is is only fired if it is a directory.

###empty
fired for empty directory

## error events
error type events are emitted with (path,error). error being the error object returned from an fs call or other opperation.

###error
if the target path cannot be read an error event is emitted. this is the only failure case.

###fail
when stat or read fails on a path somewhere in the walk and it is not your target path you get a fail event instead of error.
This is handy if you want to find places you dont have access too.

## notes
the async emitter returned supports 3 methods

###end
  stop a walk in progress

###pause
  pause the walk. no more events will be emitted until resume

###resume
  resume the walk

### ignore(path or array of paths)
  will not traverse these directories. may be called in the path event handler to ignore dynamically. 
  ```js
  var walk = require('walkdir');
  var p = require('path');
  walk('/',function(path,stat){
    // ignore all .git directories.
    if(p.basename(path) === '.git') {
      this.ignore(path)
    }
  })
  ```

### cancel a walk in progress
  ```js
  //cancel a walk in progress within callback.

  var walk = require('walkdir');
  walk('../',function(path,stat){
    this.end();
  });

  //cancel a walk in progress with emitter handle
  var walk = require('walkdir');
  var emitter = walk('../');

  doSomethingAsync(function(){
	emitter.end();
  })
  ```

## thanks
thanks to substack. the interface for this module is based off of node-findit

## contributing
see `CONTRIBUTING.md` for guidelines. this is an open opensource project.

