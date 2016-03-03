/*global window, _*/
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
    exports = {},
    createInBlobContext = function() {
      var onmessage = function(e) {
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
                error: err.message || err
            });
            }
            break;
          default:
            var msg = {
              cmd: e.data.cmd
            },
            res;
            if (w && typeof w[e.data.cmd] === "function") {
              try {
                res = w[e.data.cmd].apply(w, e.data.args);
                if (res) {
                  msg.result = res;
                }
                self.postMessage(msg);
              } catch (err1) {
                msg.error = err1.message || err1;
                self.postMessage(msg);
              }
            } else {
              msg.error = "Method not known";
              self.postMessage(msg);
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
      url = window.URL,
      blobPointer = new BlobObject(Class);
    worker = new window.Worker(url.createObjectURL(blobPointer));
    worker.onmessage = callback;
    return worker;
  },
  Wrap = function() {
    return Class.apply(this, args);
  },
  worker;

  try {
    worker = createInBlobContext();
    exports.call = function(cmd) {
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
    exports.call.apply(this, args);
    return exports;
  } catch (e) {
    args.shift();
    args.shift();
    Wrap.prototype = Class.prototype;
    try {
      worker = new Wrap();
    } catch (err) {
      callback({
        data: {
          cmd: "construct",
          error: err
        }
      });
      return;
    }
    exports.call = function(cmd) {
      var args = Array.prototype.slice.call(arguments),
        resp = {
          data: {
            cmd: cmd
          }
        };
      if (!_.isFunction(worker[cmd])) {
        resp.data.error = "Method not known";
        callback(resp);
        return;
      }
      args.shift();
      try {
        resp.data.result = worker[cmd].apply(worker, args);
        callback(resp);
      } catch (e) {
        resp.data.error = e;
        callback(resp);
      }
    };
    callback({
      data: {
        cmd: "construct",
        result: true
      }
    });
    return exports;
  }
}
