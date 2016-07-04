/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief binary min heap
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2011-2013 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / This file contains significant portions from the min heap published here:
// / http://github.com/bgrins/javascript-astar
// / Copyright (c) 2010, Brian Grinstead, http://briangrinstead.com
// / Freely distributable under the MIT License.
// / Includes Binary Heap (with modifications) from Marijn Haverbeke.
// / http://eloquentjavascript.net/appendix2.html
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

function BinaryHeap (scoreFunction) {
  this.values = [];
  this.scoreFunction = scoreFunction;
}

BinaryHeap.prototype = {

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief push an element into the heap
  // //////////////////////////////////////////////////////////////////////////////

  push: function (element) {
    this.values.push(element);
    this._sinkDown(this.values.length - 1);
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief pop the min element from the heap
  // //////////////////////////////////////////////////////////////////////////////

  pop: function () {
    var result = this.values[0];
    var end = this.values.pop();
    if (this.values.length > 0) {
      this.values[0] = end;
      this._bubbleUp(0);
    }
    return result;
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief remove a specific element from the heap
  // //////////////////////////////////////////////////////////////////////////////

  remove: function (node) {
    var i = this.values.indexOf(node);
    var end = this.values.pop();

    if (i !== this.values.length - 1) {
      this.values[i] = end;

      if (this.scoreFunction(end) < this.scoreFunction(node)) {
        this._sinkDown(i);
      } else {
        this._bubbleUp(i);
      }
    }
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief return number of elements in heap
  // //////////////////////////////////////////////////////////////////////////////

  size: function () {
    return this.values.length;
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief reposition an element in the heap
  // //////////////////////////////////////////////////////////////////////////////

  rescoreElement: function (node) {
    this._sinkDown(this.values.indexOf(node));
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief move an element down the heap
  // //////////////////////////////////////////////////////////////////////////////

  _sinkDown: function (n) {
    var element = this.values[n];

    while (n > 0) {
      var parentN = Math.floor((n + 1) / 2) - 1,
        parent = this.values[parentN];

      if (this.scoreFunction(element) < this.scoreFunction(parent)) {
        this.values[parentN] = element;
        this.values[n] = parent;
        n = parentN;
      } else {
        break;
      }
    }
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief bubble up an element
  // //////////////////////////////////////////////////////////////////////////////

  _bubbleUp: function (n) {
    var length = this.values.length,
      element = this.values[n],
      elemScore = this.scoreFunction(element);

    while (true) {
      var child2n = (n + 1) * 2;
      var child1n = child2n - 1;
      var swap = null;
      var child1Score;

      if (child1n < length) {
        var child1 = this.values[child1n];
        child1Score = this.scoreFunction(child1);

        if (child1Score < elemScore) {
          swap = child1n;
        }
      }

      if (child2n < length) {
        var child2 = this.values[child2n];
        var child2Score = this.scoreFunction(child2);
        if (child2Score < (swap === null ? elemScore : child1Score)) {
          swap = child2n;
        }
      }

      if (swap !== null) {
        this.values[n] = this.values[swap];
        this.values[swap] = element;
        n = swap;
      } else {
        break;
      }
    }
  }
};

exports.BinaryHeap = BinaryHeap;
