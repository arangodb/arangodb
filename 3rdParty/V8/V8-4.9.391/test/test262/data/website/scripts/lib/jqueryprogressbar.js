
/*
 * Copyright (c) 2007 Josh Bush (digitalbush.com)
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:

 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE. 
 */
 
/*
 * Progress Bar Plugin for jQuery
 * Version: Alpha 2
 * Release: 2007-02-26
 */ 
(function($) {
    //Main Method
    $.fn.reportprogress = function(val, maxVal) {
        var max = 100;
        if (maxVal) {
            max = maxVal;
        }
        return this.each(
			function() {
			    var div = $(this);
			    var innerdiv = div.find(".progress");
			    if (innerdiv.length !== 1) {
			        innerdiv = $("<div class='progress'><span class='text'>&nbsp;</span></div>");			        
			        div.append(innerdiv);
			    }
			    var width = Math.round(val / max * 100);
			    innerdiv.css("width", width + "%");
			    div.find(".text").html(width + " %");
			}
		);
	};
})(jQuery);
