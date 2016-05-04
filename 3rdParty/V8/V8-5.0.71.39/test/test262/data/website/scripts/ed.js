/// Copyright (c) 2012 Ecma International.  All rights reserved. 
/// This code is governed by the BSD license found in the LICENSE file.

//Error Detector
if (this.window!==undefined) {  //for console support
    this.window.onerror = function(errorMsg, url, lineNumber, colNumber, error) {
        var cookedError;

        if (error) {
            cookedError = error.toString();
        } else {
            if (/Error:/.test(errorMsg)) {
                cookedError = errorMsg;
            } else {
                cookedError = "UnknownError: " + errorMsg;
            }
        }

        $DONE(cookedError);
    };
}

