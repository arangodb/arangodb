/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global window*/
// These values are injected
/*global w:true, Construct, self*/
////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


function WebWorkerWrapper(Class, callback) {
  "use strict";
  
  if (Class === undefined) {
    throw "A class has to be given.";
  } 
 
  if (callback === undefined) {
    throw "A callback has to be given.";
  }
  
  var args = Array.prototype.slice.call(arguments),
  
  onmessage = function(e) {
    switch(e.data.cmd) {
      case "construct":
        try {
          w = new (Function.prototype.bind.apply(
            Construct, [null].concat(e.data.args)
          ))();
          if (w) {
            self.postMessage({
              cmd: "construct",
              result: true
            });
          } else {
            self.postMessage({
              cmd: "construct",
              result: false
            });
          }
        } catch (err) {
          self.postMessage({
            cmd: "construct",
            result: false,
            error: err
          });
        }
        break;
      default:
        if (w && typeof w[e.data.cmd] === "function") {
          try {
            self.postMessage({
              cmd: e.data.cmd,
              result: w[e.data.cmd].apply(w, e.data.args)
            });
          } catch (err1) {
            self.postMessage({
              cmd: e.data.cmd,
              error: err1
            });
          }
        } else {
          self.postMessage({
            cmd: e.data.cmd,
            error: "Method not known"
          });
        }
    }
  },
    
  BlobObject = function(c) {
    var code = "var w, Construct = "
      + c.toString()
      + ";self.onmessage = "
      + onmessage.toString();     
    return new window.Blob(code.split());
  },
  worker,
  url = window.webkitURL || window.URL,
  blobPointer = new BlobObject(Class);
  worker = new window.Worker(url.createObjectURL(blobPointer));
  worker.onmessage = callback;
  
  this.call = function(cmd) {
    var args = Array.prototype.slice.call(arguments);
    args.shift();
    worker.postMessage({
      cmd: cmd,
      args: args
    });
  };

  args.shift();
  args.shift();
  args.unshift("construct");
  this.call.apply(this, args);
}