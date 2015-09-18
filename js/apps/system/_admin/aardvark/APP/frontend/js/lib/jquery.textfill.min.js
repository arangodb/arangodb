/*
  textfill
 @name      jquery.textfill.js
 @author    Russ Painter
 @author    Yu-Jie Lin
 @author    Alexandre Dantas
 @version   0.6.0
 @date      2014-08-19
 @copyright (c) 2014 Alexandre Dantas
 @copyright (c) 2012-2013 Yu-Jie Lin
 @copyright (c) 2009 Russ Painter
 @license   MIT License
 @homepage  https://github.com/jquery-textfill/jquery-textfill
 @example   http://jquery-textfill.github.io/jquery-textfill/index.html
*/
(function(m){m.fn.textfill=function(r){function f(){a.debug&&"undefined"!=typeof console&&"undefined"!=typeof console.debug&&console.debug.apply(console,arguments)}function s(){"undefined"!=typeof console&&"undefined"!=typeof console.warn&&console.warn.apply(console,arguments)}function p(a,b,e,k,n,g){function d(a,b){var c=" / ";a>b?c=" > ":a==b&&(c=" = ");return c}f("[TextFill] "+a+" { font-size: "+b.css("font-size")+",Height: "+b.height()+"px "+d(b.height(),e)+e+"px,Width: "+b.width()+d(b.width(),
k)+k+",minFontPixels: "+n+"px, maxFontPixels: "+g+"px }")}function q(a,b,e,k,f,g,d,h){for(p(a,b,f,g,d,h);d<h-1;){var l=Math.floor((d+h)/2);b.css("font-size",l);if(e.call(b)<=k){if(d=l,e.call(b)==k)break}else h=l;p(a,b,f,g,d,h)}b.css("font-size",h);e.call(b)<=k&&(d=h,p(a+"* ",b,f,g,d,h));return d}var a=m.extend({debug:!1,maxFontPixels:40,minFontPixels:4,innerTag:"span",widthOnly:!1,success:null,callback:null,fail:null,complete:null,explicitWidth:null,explicitHeight:null,changeLineHeight:!1},r);f("[TextFill] Start Debug");
this.each(function(){var c=m(a.innerTag+":visible:first",this),b=a.explicitHeight||m(this).height(),e=a.explicitWidth||m(this).width(),k=c.css("font-size"),n=parseFloat(c.css("line-height"))/parseFloat(k);f("[TextFill] Inner text: "+c.text());f("[TextFill] All options: ",a);f("[TextFill] Maximum sizes: { Height: "+b+"px, Width: "+e+"px }");var g=a.minFontPixels,d=0>=a.maxFontPixels?b:a.maxFontPixels,h=void 0;a.widthOnly||(h=q("Height",c,m.fn.height,b,b,e,g,d));var l=void 0,l=q("Width",c,m.fn.width,
e,b,e,g,d);a.widthOnly?(c.css({"font-size":l,"white-space":"nowrap"}),a.changeLineHeight&&c.parent().css("line-height",n*l+"px")):(g=Math.min(h,l),c.css("font-size",g),a.changeLineHeight&&c.parent().css("line-height",n*g+"px"));f("[TextFill] Finished { Old font-size: "+k+", New font-size: "+c.css("font-size")+" }");c.width()>e||c.height()>b&&!a.widthOnly?(c.css("font-size",k),a.fail&&a.fail(this),f("[TextFill] Failure { Current Width: "+c.width()+", Maximum Width: "+e+", Current Height: "+c.height()+
", Maximum Height: "+b+" }")):a.success?a.success(this):a.callback&&(s("callback is deprecated, use success, instead"),a.callback(this))});a.complete&&a.complete(this);f("[TextFill] End Debug");return this}})(window.jQuery);
