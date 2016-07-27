!CHAPTER ArangoDB Shell Configuration

_arangosh_ will look for a user-defined startup script named *.arangosh.rc* in the
user's home directory on startup. The home directory will likely be `/home/<username>/`
on Unix/Linux, and is determined on Windows by peeking into the environment variables
`%HOMEDRIVE%` and `%HOMEPATH%`. 

If the file *.arangosh.rc* is present in the home directory, _arangosh_ will execute
the contents of this file inside the global scope.

You can use this to define your own extra variables and functions that you need often.
For example, you could put the following into the *.arangosh.rc* file in your home
directory:

```js
// "var" keyword avoided intentionally...
// otherwise "timed" would not survive the scope of this script
global.timed = function (cb) {
  console.time("callback");
  cb();
  console.timeEnd("callback");
};
```

This will make a function named *timed* available in _arangosh_ in the global scope.

You can now start _arangosh_ and invoke the function like this:

```js
timed(function () { 
  for (var i = 0; i < 1000; ++i) {
    db.test.save({ value: i }); 
  }
});
```

Please keep in mind that, if present, the *.arangosh.rc* file needs to contain valid
JavaScript code. If you want any variables in the global scope to survive you need to
omit the *var* keyword for them. Otherwise the variables will only be visible inside
the script itself, but not outside.
