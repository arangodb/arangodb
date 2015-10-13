/*! @license Copyright 2011 Dan Vanderkam (danvdk@gmail.com) MIT-licensed (http://opensource.org/licenses/MIT) */
Date.ext={};Date.ext.util={};Date.ext.util.xPad=function(a,c,b){if(typeof(b)=="undefined"){b=10}for(;parseInt(a,10)<b&&b>1;b/=10){a=c.toString()+a}return a.toString()};Date.prototype.locale="en-GB";if(document.getElementsByTagName("html")&&document.getElementsByTagName("html")[0].lang){Date.prototype.locale=document.getElementsByTagName("html")[0].lang}Date.ext.locales={};Date.ext.locales.en={a:["Sun","Mon","Tue","Wed","Thu","Fri","Sat"],A:["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"],b:["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"],B:["January","February","March","April","May","June","July","August","September","October","November","December"],c:"%a %d %b %Y %T %Z",p:["AM","PM"],P:["am","pm"],x:"%d/%m/%y",X:"%T"};Date.ext.locales["en-US"]=Date.ext.locales.en;Date.ext.locales["en-US"].c="%a %d %b %Y %r %Z";Date.ext.locales["en-US"].x="%D";Date.ext.locales["en-US"].X="%r";Date.ext.locales["en-GB"]=Date.ext.locales.en;Date.ext.locales["en-AU"]=Date.ext.locales["en-GB"];Date.ext.formats={a:function(a){return Date.ext.locales[a.locale].a[a.getDay()]},A:function(a){return Date.ext.locales[a.locale].A[a.getDay()]},b:function(a){return Date.ext.locales[a.locale].b[a.getMonth()]},B:function(a){return Date.ext.locales[a.locale].B[a.getMonth()]},c:"toLocaleString",C:function(a){return Date.ext.util.xPad(parseInt(a.getFullYear()/100,10),0)},d:["getDate","0"],e:["getDate"," "],g:function(a){return Date.ext.util.xPad(parseInt(Date.ext.util.G(a)/100,10),0)},G:function(c){var e=c.getFullYear();var b=parseInt(Date.ext.formats.V(c),10);var a=parseInt(Date.ext.formats.W(c),10);if(a>b){e++}else{if(a===0&&b>=52){e--}}return e},H:["getHours","0"],I:function(b){var a=b.getHours()%12;return Date.ext.util.xPad(a===0?12:a,0)},j:function(c){var a=c-new Date(""+c.getFullYear()+"/1/1 GMT");a+=c.getTimezoneOffset()*60000;var b=parseInt(a/60000/60/24,10)+1;return Date.ext.util.xPad(b,0,100)},m:function(a){return Date.ext.util.xPad(a.getMonth()+1,0)},M:["getMinutes","0"],p:function(a){return Date.ext.locales[a.locale].p[a.getHours()>=12?1:0]},P:function(a){return Date.ext.locales[a.locale].P[a.getHours()>=12?1:0]},S:["getSeconds","0"],u:function(a){var b=a.getDay();return b===0?7:b},U:function(e){var a=parseInt(Date.ext.formats.j(e),10);var c=6-e.getDay();var b=parseInt((a+c)/7,10);return Date.ext.util.xPad(b,0)},V:function(e){var c=parseInt(Date.ext.formats.W(e),10);var a=(new Date(""+e.getFullYear()+"/1/1")).getDay();var b=c+(a>4||a<=1?0:1);if(b==53&&(new Date(""+e.getFullYear()+"/12/31")).getDay()<4){b=1}else{if(b===0){b=Date.ext.formats.V(new Date(""+(e.getFullYear()-1)+"/12/31"))}}return Date.ext.util.xPad(b,0)},w:"getDay",W:function(e){var a=parseInt(Date.ext.formats.j(e),10);var c=7-Date.ext.formats.u(e);var b=parseInt((a+c)/7,10);return Date.ext.util.xPad(b,0,10)},y:function(a){return Date.ext.util.xPad(a.getFullYear()%100,0)},Y:"getFullYear",z:function(c){var b=c.getTimezoneOffset();var a=Date.ext.util.xPad(parseInt(Math.abs(b/60),10),0);var e=Date.ext.util.xPad(b%60,0);return(b>0?"-":"+")+a+e},Z:function(a){return a.toString().replace(/^.*\(([^)]+)\)$/,"$1")},"%":function(a){return"%"}};Date.ext.aggregates={c:"locale",D:"%m/%d/%y",h:"%b",n:"\n",r:"%I:%M:%S %p",R:"%H:%M",t:"\t",T:"%H:%M:%S",x:"locale",X:"locale"};Date.ext.aggregates.z=Date.ext.formats.z(new Date());Date.ext.aggregates.Z=Date.ext.formats.Z(new Date());Date.ext.unsupported={};Date.prototype.strftime=function(a){if(!(this.locale in Date.ext.locales)){if(this.locale.replace(/-[a-zA-Z]+$/,"") in Date.ext.locales){this.locale=this.locale.replace(/-[a-zA-Z]+$/,"")}else{this.locale="en-GB"}}var c=this;while(a.match(/%[cDhnrRtTxXzZ]/)){a=a.replace(/%([cDhnrRtTxXzZ])/g,function(e,d){var g=Date.ext.aggregates[d];return(g=="locale"?Date.ext.locales[c.locale][d]:g)})}var b=a.replace(/%([aAbBCdegGHIjmMpPSuUVwWyY%])/g,function(e,d){var g=Date.ext.formats[d];if(typeof(g)=="string"){return c[g]()}else{if(typeof(g)=="function"){return g.call(c,c)}else{if(typeof(g)=="object"&&typeof(g[0])=="string"){return Date.ext.util.xPad(c[g[0]](),g[1])}else{return d}}}});c=null;return b};"use strict";function RGBColorParser(f){this.ok=false;if(f.charAt(0)=="#"){f=f.substr(1,6)}f=f.replace(/ /g,"");f=f.toLowerCase();var b={aliceblue:"f0f8ff",antiquewhite:"faebd7",aqua:"00ffff",aquamarine:"7fffd4",azure:"f0ffff",beige:"f5f5dc",bisque:"ffe4c4",black:"000000",blanchedalmond:"ffebcd",blue:"0000ff",blueviolet:"8a2be2",brown:"a52a2a",burlywood:"deb887",cadetblue:"5f9ea0",chartreuse:"7fff00",chocolate:"d2691e",coral:"ff7f50",cornflowerblue:"6495ed",cornsilk:"fff8dc",crimson:"dc143c",cyan:"00ffff",darkblue:"00008b",darkcyan:"008b8b",darkgoldenrod:"b8860b",darkgray:"a9a9a9",darkgreen:"006400",darkkhaki:"bdb76b",darkmagenta:"8b008b",darkolivegreen:"556b2f",darkorange:"ff8c00",darkorchid:"9932cc",darkred:"8b0000",darksalmon:"e9967a",darkseagreen:"8fbc8f",darkslateblue:"483d8b",darkslategray:"2f4f4f",darkturquoise:"00ced1",darkviolet:"9400d3",deeppink:"ff1493",deepskyblue:"00bfff",dimgray:"696969",dodgerblue:"1e90ff",feldspar:"d19275",firebrick:"b22222",floralwhite:"fffaf0",forestgreen:"228b22",fuchsia:"ff00ff",gainsboro:"dcdcdc",ghostwhite:"f8f8ff",gold:"ffd700",goldenrod:"daa520",gray:"808080",green:"008000",greenyellow:"adff2f",honeydew:"f0fff0",hotpink:"ff69b4",indianred:"cd5c5c",indigo:"4b0082",ivory:"fffff0",khaki:"f0e68c",lavender:"e6e6fa",lavenderblush:"fff0f5",lawngreen:"7cfc00",lemonchiffon:"fffacd",lightblue:"add8e6",lightcoral:"f08080",lightcyan:"e0ffff",lightgoldenrodyellow:"fafad2",lightgrey:"d3d3d3",lightgreen:"90ee90",lightpink:"ffb6c1",lightsalmon:"ffa07a",lightseagreen:"20b2aa",lightskyblue:"87cefa",lightslateblue:"8470ff",lightslategray:"778899",lightsteelblue:"b0c4de",lightyellow:"ffffe0",lime:"00ff00",limegreen:"32cd32",linen:"faf0e6",magenta:"ff00ff",maroon:"800000",mediumaquamarine:"66cdaa",mediumblue:"0000cd",mediumorchid:"ba55d3",mediumpurple:"9370d8",mediumseagreen:"3cb371",mediumslateblue:"7b68ee",mediumspringgreen:"00fa9a",mediumturquoise:"48d1cc",mediumvioletred:"c71585",midnightblue:"191970",mintcream:"f5fffa",mistyrose:"ffe4e1",moccasin:"ffe4b5",navajowhite:"ffdead",navy:"000080",oldlace:"fdf5e6",olive:"808000",olivedrab:"6b8e23",orange:"ffa500",orangered:"ff4500",orchid:"da70d6",palegoldenrod:"eee8aa",palegreen:"98fb98",paleturquoise:"afeeee",palevioletred:"d87093",papayawhip:"ffefd5",peachpuff:"ffdab9",peru:"cd853f",pink:"ffc0cb",plum:"dda0dd",powderblue:"b0e0e6",purple:"800080",red:"ff0000",rosybrown:"bc8f8f",royalblue:"4169e1",saddlebrown:"8b4513",salmon:"fa8072",sandybrown:"f4a460",seagreen:"2e8b57",seashell:"fff5ee",sienna:"a0522d",silver:"c0c0c0",skyblue:"87ceeb",slateblue:"6a5acd",slategray:"708090",snow:"fffafa",springgreen:"00ff7f",steelblue:"4682b4",tan:"d2b48c",teal:"008080",thistle:"d8bfd8",tomato:"ff6347",turquoise:"40e0d0",violet:"ee82ee",violetred:"d02090",wheat:"f5deb3",white:"ffffff",whitesmoke:"f5f5f5",yellow:"ffff00",yellowgreen:"9acd32"};for(var g in b){if(f==g){f=b[g]}}var e=[{re:/^rgb\((\d{1,3}),\s*(\d{1,3}),\s*(\d{1,3})\)$/,example:["rgb(123, 234, 45)","rgb(255,234,245)"],process:function(i){return[parseInt(i[1]),parseInt(i[2]),parseInt(i[3])]}},{re:/^(\w{2})(\w{2})(\w{2})$/,example:["#00ff00","336699"],process:function(i){return[parseInt(i[1],16),parseInt(i[2],16),parseInt(i[3],16)]}},{re:/^(\w{1})(\w{1})(\w{1})$/,example:["#fb0","f0f"],process:function(i){return[parseInt(i[1]+i[1],16),parseInt(i[2]+i[2],16),parseInt(i[3]+i[3],16)]}}];for(var c=0;c<e.length;c++){var j=e[c].re;var a=e[c].process;var h=j.exec(f);if(h){var d=a(h);this.r=d[0];this.g=d[1];this.b=d[2];this.ok=true}}this.r=(this.r<0||isNaN(this.r))?0:((this.r>255)?255:this.r);this.g=(this.g<0||isNaN(this.g))?0:((this.g>255)?255:this.g);this.b=(this.b<0||isNaN(this.b))?0:((this.b>255)?255:this.b);this.toRGB=function(){return"rgb("+this.r+", "+this.g+", "+this.b+")"};this.toHex=function(){var l=this.r.toString(16);var k=this.g.toString(16);var i=this.b.toString(16);if(l.length==1){l="0"+l}if(k.length==1){k="0"+k}if(i.length==1){i="0"+i}return"#"+l+k+i}}function printStackTrace(b){b=b||{guess:true};var c=b.e||null,e=!!b.guess;var d=new printStackTrace.implementation(),a=d.run(c);return(e)?d.guessAnonymousFunctions(a):a}printStackTrace.implementation=function(){};printStackTrace.implementation.prototype={run:function(a,b){a=a||this.createException();b=b||this.mode(a);if(b==="other"){return this.other(arguments.callee)}else{return this[b](a)}},createException:function(){try{this.undef()}catch(a){return a}},mode:function(a){if(a["arguments"]&&a.stack){return"chrome"}else{if(typeof a.message==="string"&&typeof window!=="undefined"&&window.opera){if(!a.stacktrace){return"opera9"}if(a.message.indexOf("\n")>-1&&a.message.split("\n").length>a.stacktrace.split("\n").length){return"opera9"}if(!a.stack){return"opera10a"}if(a.stacktrace.indexOf("called from line")<0){return"opera10b"}return"opera11"}else{if(a.stack){return"firefox"}}}return"other"},instrumentFunction:function(b,d,e){b=b||window;var a=b[d];b[d]=function c(){e.call(this,printStackTrace().slice(4));return b[d]._instrumented.apply(this,arguments)};b[d]._instrumented=a},deinstrumentFunction:function(a,b){if(a[b].constructor===Function&&a[b]._instrumented&&a[b]._instrumented.constructor===Function){a[b]=a[b]._instrumented}},chrome:function(b){var a=(b.stack+"\n").replace(/^\S[^\(]+?[\n$]/gm,"").replace(/^\s+at\s+/gm,"").replace(/^([^\(]+?)([\n$])/gm,"{anonymous}()@$1$2").replace(/^Object.<anonymous>\s*\(([^\)]+)\)/gm,"{anonymous}()@$1").split("\n");a.pop();return a},firefox:function(a){return a.stack.replace(/(?:\n@:0)?\s+$/m,"").replace(/^\(/gm,"{anonymous}(").split("\n")},opera11:function(g){var a="{anonymous}",h=/^.*line (\d+), column (\d+)(?: in (.+))? in (\S+):$/;var k=g.stacktrace.split("\n"),l=[];for(var c=0,f=k.length;c<f;c+=2){var d=h.exec(k[c]);if(d){var j=d[4]+":"+d[1]+":"+d[2];var b=d[3]||"global code";b=b.replace(/<anonymous function: (\S+)>/,"$1").replace(/<anonymous function>/,a);l.push(b+"@"+j+" -- "+k[c+1].replace(/^\s+/,""))}}return l},opera10b:function(g){var a="{anonymous}",h=/^(.*)@(.+):(\d+)$/;var j=g.stacktrace.split("\n"),k=[];for(var c=0,f=j.length;c<f;c++){var d=h.exec(j[c]);if(d){var b=d[1]?(d[1]+"()"):"global code";k.push(b+"@"+d[2]+":"+d[3])}}return k},opera10a:function(g){var a="{anonymous}",h=/Line (\d+).*script (?:in )?(\S+)(?:: In function (\S+))?$/i;var j=g.stacktrace.split("\n"),k=[];for(var c=0,f=j.length;c<f;c+=2){var d=h.exec(j[c]);if(d){var b=d[3]||a;k.push(b+"()@"+d[2]+":"+d[1]+" -- "+j[c+1].replace(/^\s+/,""))}}return k},opera9:function(j){var d="{anonymous}",h=/Line (\d+).*script (?:in )?(\S+)/i;var c=j.message.split("\n"),b=[];for(var g=2,a=c.length;g<a;g+=2){var f=h.exec(c[g]);if(f){b.push(d+"()@"+f[2]+":"+f[1]+" -- "+c[g+1].replace(/^\s+/,""))}}return b},other:function(g){var b="{anonymous}",f=/function\s*([\w\-$]+)?\s*\(/i,a=[],d,c,e=10;while(g&&a.length<e){d=f.test(g.toString())?RegExp.$1||b:b;c=Array.prototype.slice.call(g["arguments"]||[]);a[a.length]=d+"("+this.stringifyArguments(c)+")";g=g.caller}return a},stringifyArguments:function(c){var b=[];var e=Array.prototype.slice;for(var d=0;d<c.length;++d){var a=c[d];if(a===undefined){b[d]="undefined"}else{if(a===null){b[d]="null"}else{if(a.constructor){if(a.constructor===Array){if(a.length<3){b[d]="["+this.stringifyArguments(a)+"]"}else{b[d]="["+this.stringifyArguments(e.call(a,0,1))+"..."+this.stringifyArguments(e.call(a,-1))+"]"}}else{if(a.constructor===Object){b[d]="#object"}else{if(a.constructor===Function){b[d]="#function"}else{if(a.constructor===String){b[d]='"'+a+'"'}else{if(a.constructor===Number){b[d]=a}}}}}}}}}return b.join(",")},sourceCache:{},ajax:function(a){var b=this.createXMLHTTPObject();if(b){try{b.open("GET",a,false);b.send(null);return b.responseText}catch(c){}}return""},createXMLHTTPObject:function(){var c,a=[function(){return new XMLHttpRequest()},function(){return new ActiveXObject("Msxml2.XMLHTTP")},function(){return new ActiveXObject("Msxml3.XMLHTTP")},function(){return new ActiveXObject("Microsoft.XMLHTTP")}];for(var b=0;b<a.length;b++){try{c=a[b]();this.createXMLHTTPObject=a[b];return c}catch(d){}}},isSameDomain:function(a){return a.indexOf(location.hostname)!==-1},getSource:function(a){if(!(a in this.sourceCache)){this.sourceCache[a]=this.ajax(a).split("\n")}return this.sourceCache[a]},guessAnonymousFunctions:function(k){for(var g=0;g<k.length;++g){var f=/\{anonymous\}\(.*\)@(.*)/,l=/^(.*?)(?::(\d+))(?::(\d+))?(?: -- .+)?$/,b=k[g],c=f.exec(b);if(c){var e=l.exec(c[1]),d=e[1],a=e[2],j=e[3]||0;if(d&&this.isSameDomain(d)&&a){var h=this.guessAnonymousFunction(d,a,j);k[g]=b.replace("{anonymous}",h)}}}return k},guessAnonymousFunction:function(c,f,a){var b;try{b=this.findFunctionName(this.getSource(c),f)}catch(d){b="getSource failed with url: "+c+", exception: "+d.toString()}return b},findFunctionName:function(a,e){var g=/function\s+([^(]*?)\s*\(([^)]*)\)/;var k=/['"]?([0-9A-Za-z_]+)['"]?\s*[:=]\s*function\b/;var h=/['"]?([0-9A-Za-z_]+)['"]?\s*[:=]\s*(?:eval|new Function)\b/;var b="",l,j=Math.min(e,20),d,c;for(var f=0;f<j;++f){l=a[e-f-1];c=l.indexOf("//");if(c>=0){l=l.substr(0,c)}if(l){b=l+b;d=k.exec(b);if(d&&d[1]){return d[1]}d=g.exec(b);if(d&&d[1]){return d[1]}d=h.exec(b);if(d&&d[1]){return d[1]}}}return"(?)"}};CanvasRenderingContext2D.prototype.installPattern=function(e){if(typeof(this.isPatternInstalled)!=="undefined"){throw"Must un-install old line pattern before installing a new one."}this.isPatternInstalled=true;var g=[0,0];var b=[];var f=this.beginPath;var d=this.lineTo;var c=this.moveTo;var a=this.stroke;this.uninstallPattern=function(){this.beginPath=f;this.lineTo=d;this.moveTo=c;this.stroke=a;this.uninstallPattern=undefined;this.isPatternInstalled=undefined};this.beginPath=function(){b=[];f.call(this)};this.moveTo=function(h,i){b.push([[h,i]]);c.call(this,h,i)};this.lineTo=function(h,j){var i=b[b.length-1];i.push([h,j])};this.stroke=function(){if(b.length===0){a.call(this);return}for(var p=0;p<b.length;p++){var o=b[p];var l=o[0][0],u=o[0][1];for(var n=1;n<o.length;n++){var h=o[n][0],s=o[n][1];this.save();var w=(h-l);var v=(s-u);var r=Math.sqrt(w*w+v*v);var k=Math.atan2(v,w);this.translate(l,u);c.call(this,0,0);this.rotate(k);var m=g[0];var t=0;while(r>t){var q=e[m];if(g[1]){t+=g[1]}else{t+=q}if(t>r){g=[m,t-r];t=r}else{g=[(m+1)%e.length,0]}if(m%2===0){d.call(this,t,0)}else{c.call(this,t,0)}m=(m+1)%e.length}this.restore();l=h,u=s}}a.call(this);b=[]}};CanvasRenderingContext2D.prototype.uninstallPattern=function(){throw"Must install a line pattern before uninstalling it."};var DygraphOptions=(function(){var a=function(b){this.dygraph_=b;this.yAxes_=[];this.xAxis_={};this.series_={};this.global_=this.dygraph_.attrs_;this.user_=this.dygraph_.user_attrs_||{};this.labels_=[];this.highlightSeries_=this.get("highlightSeriesOpts")||{};this.reparseSeries()};a.AXIS_STRING_MAPPINGS_={y:0,Y:0,y1:0,Y1:0,y2:1,Y2:1};a.axisToIndex_=function(b){if(typeof(b)=="string"){if(a.AXIS_STRING_MAPPINGS_.hasOwnProperty(b)){return a.AXIS_STRING_MAPPINGS_[b]}throw"Unknown axis : "+b}if(typeof(b)=="number"){if(b===0||b===1){return b}throw"Dygraphs only supports two y-axes, indexed from 0-1."}if(b){throw"Unknown axis : "+b}return 0};a.prototype.reparseSeries=function(){var g=this.get("labels");if(!g){return}this.labels_=g.slice(1);this.yAxes_=[{series:[],options:{}}];this.xAxis_={options:{}};this.series_={};var h=!this.user_.series;if(h){var c=0;for(var j=0;j<this.labels_.length;j++){var i=this.labels_[j];var e=this.user_[i]||{};var b=0;var d=e.axis;if(typeof(d)=="object"){b=++c;this.yAxes_[b]={series:[i],options:d}}if(!d){this.yAxes_[0].series.push(i)}this.series_[i]={idx:j,yAxis:b,options:e}}for(var j=0;j<this.labels_.length;j++){var i=this.labels_[j];var e=this.series_[i]["options"];var d=e.axis;if(typeof(d)=="string"){if(!this.series_.hasOwnProperty(d)){Dygraph.error("Series "+i+" wants to share a y-axis with series "+d+", which does not define its own axis.");return}var b=this.series_[d].yAxis;this.series_[i].yAxis=b;this.yAxes_[b].series.push(i)}}}else{for(var j=0;j<this.labels_.length;j++){var i=this.labels_[j];var e=this.user_.series[i]||{};var b=a.axisToIndex_(e.axis);this.series_[i]={idx:j,yAxis:b,options:e};if(!this.yAxes_[b]){this.yAxes_[b]={series:[i],options:{}}}else{this.yAxes_[b].series.push(i)}}}var f=this.user_.axes||{};Dygraph.update(this.yAxes_[0].options,f.y||{});if(this.yAxes_.length>1){Dygraph.update(this.yAxes_[1].options,f.y2||{})}Dygraph.update(this.xAxis_.options,f.x||{})};a.prototype.get=function(c){var b=this.getGlobalUser_(c);if(b!==null){return b}return this.getGlobalDefault_(c)};a.prototype.getGlobalUser_=function(b){if(this.user_.hasOwnProperty(b)){return this.user_[b]}return null};a.prototype.getGlobalDefault_=function(b){if(this.global_.hasOwnProperty(b)){return this.global_[b]}if(Dygraph.DEFAULT_ATTRS.hasOwnProperty(b)){return Dygraph.DEFAULT_ATTRS[b]}return null};a.prototype.getForAxis=function(d,e){var f;var i;if(typeof(e)=="number"){f=e;i=f===0?"y":"y2"}else{if(e=="y1"){e="y"}if(e=="y"){f=0}else{if(e=="y2"){f=1}else{if(e=="x"){f=-1}else{throw"Unknown axis "+e}}}i=e}var g=(f==-1)?this.xAxis_:this.yAxes_[f];if(g){var h=g.options;if(h.hasOwnProperty(d)){return h[d]}}var c=this.getGlobalUser_(d);if(c!==null){return c}var b=Dygraph.DEFAULT_ATTRS.axes[i];if(b.hasOwnProperty(d)){return b[d]}return this.getGlobalDefault_(d)};a.prototype.getForSeries=function(c,e){if(e===this.dygraph_.getHighlightSeries()){if(this.highlightSeries_.hasOwnProperty(c)){return this.highlightSeries_[c]}}if(!this.series_.hasOwnProperty(e)){throw"Unknown series: "+e}var d=this.series_[e];var b=d.options;if(b.hasOwnProperty(c)){return b[c]}return this.getForAxis(c,d.yAxis)};a.prototype.numAxes=function(){return this.yAxes_.length};a.prototype.axisForSeries=function(b){return this.series_[b].yAxis};a.prototype.axisOptions=function(b){return this.yAxes_[b].options};a.prototype.seriesForAxis=function(b){return this.yAxes_[b].series};a.prototype.seriesNames=function(){return this.labels_};return a})();"use strict";var DygraphLayout=function(a){this.dygraph_=a;this.points=[];this.setNames=[];this.annotations=[];this.yAxes_=null;this.xTicks_=null;this.yTicks_=null};DygraphLayout.prototype.attr_=function(a){return this.dygraph_.attr_(a)};DygraphLayout.prototype.addDataset=function(a,b){this.points.push(b);this.setNames.push(a)};DygraphLayout.prototype.getPlotArea=function(){return this.area_};DygraphLayout.prototype.computePlotArea=function(){var a={x:0,y:0};a.w=this.dygraph_.width_-a.x-this.attr_("rightGap");a.h=this.dygraph_.height_;var b={chart_div:this.dygraph_.graphDiv,reserveSpaceLeft:function(c){var d={x:a.x,y:a.y,w:c,h:a.h};a.x+=c;a.w-=c;return d},reserveSpaceRight:function(c){var d={x:a.x+a.w-c,y:a.y,w:c,h:a.h};a.w-=c;return d},reserveSpaceTop:function(c){var d={x:a.x,y:a.y,w:a.w,h:c};a.y+=c;a.h-=c;return d},reserveSpaceBottom:function(c){var d={x:a.x,y:a.y+a.h-c,w:a.w,h:c};a.h-=c;return d},chartRect:function(){return{x:a.x,y:a.y,w:a.w,h:a.h}}};this.dygraph_.cascadeEvents_("layout",b);this.area_=a};DygraphLayout.prototype.setAnnotations=function(d){this.annotations=[];var e=this.attr_("xValueParser")||function(a){return a};for(var c=0;c<d.length;c++){var b={};if(!d[c].xval&&d[c].x===undefined){this.dygraph_.error("Annotations must have an 'x' property");return}if(d[c].icon&&!(d[c].hasOwnProperty("width")&&d[c].hasOwnProperty("height"))){this.dygraph_.error("Must set width and height when setting annotation.icon property");return}Dygraph.update(b,d[c]);if(!b.xval){b.xval=e(b.x)}this.annotations.push(b)}};DygraphLayout.prototype.setXTicks=function(a){this.xTicks_=a};DygraphLayout.prototype.setYAxes=function(a){this.yAxes_=a};DygraphLayout.prototype.evaluate=function(){this._evaluateLimits();this._evaluateLineCharts();this._evaluateLineTicks();this._evaluateAnnotations()};DygraphLayout.prototype._evaluateLimits=function(){var a=this.dygraph_.xAxisRange();this.minxval=a[0];this.maxxval=a[1];var d=a[1]-a[0];this.xscale=(d!==0?1/d:1);for(var b=0;b<this.yAxes_.length;b++){var c=this.yAxes_[b];c.minyval=c.computedValueRange[0];c.maxyval=c.computedValueRange[1];c.yrange=c.maxyval-c.minyval;c.yscale=(c.yrange!==0?1/c.yrange:1);if(c.g.attr_("logscale")){c.ylogrange=Dygraph.log10(c.maxyval)-Dygraph.log10(c.minyval);c.ylogscale=(c.ylogrange!==0?1/c.ylogrange:1);if(!isFinite(c.ylogrange)||isNaN(c.ylogrange)){c.g.error("axis "+b+" of graph at "+c.g+" can't be displayed in log scale for range ["+c.minyval+" - "+c.maxyval+"]")}}}};DygraphLayout._calcYNormal=function(b,c,a){if(a){return 1-((Dygraph.log10(c)-Dygraph.log10(b.minyval))*b.ylogscale)}else{return 1-((c-b.minyval)*b.yscale)}};DygraphLayout.prototype._evaluateLineCharts=function(){var c=this.attr_("connectSeparatedPoints");var k=this.attr_("stackedGraph");var e=this.attr_("errorBars")||this.attr_("customBars");for(var a=0;a<this.points.length;a++){var l=this.points[a];var f=this.setNames[a];var b=this.dygraph_.axisPropertiesForSeries(f);var g=this.dygraph_.attributes_.getForSeries("logscale",f);for(var d=0;d<l.length;d++){var i=l[d];i.x=(i.xval-this.minxval)*this.xscale;var h=i.yval;if(k){i.y_stacked=DygraphLayout._calcYNormal(b,i.yval_stacked,g);if(h!==null&&!isNaN(h)){h=i.yval_stacked}}if(h===null){h=NaN;if(!c){i.yval=NaN}}i.y=DygraphLayout._calcYNormal(b,h,g);if(e){i.y_top=DygraphLayout._calcYNormal(b,h-i.yval_minus,g);i.y_bottom=DygraphLayout._calcYNormal(b,h+i.yval_plus,g)}}}};DygraphLayout.parseFloat_=function(a){if(a===null){return NaN}return a};DygraphLayout.prototype._evaluateLineTicks=function(){var d,c,b,f;this.xticks=[];for(d=0;d<this.xTicks_.length;d++){c=this.xTicks_[d];b=c.label;f=this.xscale*(c.v-this.minxval);if((f>=0)&&(f<=1)){this.xticks.push([f,b])}}this.yticks=[];for(d=0;d<this.yAxes_.length;d++){var e=this.yAxes_[d];for(var a=0;a<e.ticks.length;a++){c=e.ticks[a];b=c.label;f=this.dygraph_.toPercentYCoord(c.v,d);if((f>=0)&&(f<=1)){this.yticks.push([d,f,b])}}}};DygraphLayout.prototype._evaluateAnnotations=function(){var d;var g={};for(d=0;d<this.annotations.length;d++){var b=this.annotations[d];g[b.xval+","+b.series]=b}this.annotated_points=[];if(!this.annotations||!this.annotations.length){return}for(var h=0;h<this.points.length;h++){var e=this.points[h];for(d=0;d<e.length;d++){var f=e[d];var c=f.xval+","+f.name;if(c in g){f.annotation=g[c];this.annotated_points.push(f)}}}};DygraphLayout.prototype.removeAllDatasets=function(){delete this.points;delete this.setNames;delete this.setPointsLengths;delete this.setPointsOffsets;this.points=[];this.setNames=[];this.setPointsLengths=[];this.setPointsOffsets=[]};"use strict";var DygraphCanvasRenderer=function(d,c,b,e){this.dygraph_=d;this.layout=e;this.element=c;this.elementContext=b;this.container=this.element.parentNode;this.height=this.element.height;this.width=this.element.width;if(!this.isIE&&!(DygraphCanvasRenderer.isSupported(this.element))){throw"Canvas is not supported."}this.area=e.getPlotArea();this.container.style.position="relative";this.container.style.width=this.width+"px";if(this.dygraph_.isUsingExcanvas_){this._createIEClipArea()}else{if(!Dygraph.isAndroid()){var a=this.dygraph_.canvas_ctx_;a.beginPath();a.rect(this.area.x,this.area.y,this.area.w,this.area.h);a.clip();a=this.dygraph_.hidden_ctx_;a.beginPath();a.rect(this.area.x,this.area.y,this.area.w,this.area.h);a.clip()}}};DygraphCanvasRenderer.prototype.attr_=function(a,b){return this.dygraph_.attr_(a,b)};DygraphCanvasRenderer.prototype.clear=function(){var a;if(this.isIE){try{if(this.clearDelay){this.clearDelay.cancel();this.clearDelay=null}a=this.elementContext}catch(b){return}}a=this.elementContext;a.clearRect(0,0,this.width,this.height)};DygraphCanvasRenderer.isSupported=function(f){var b=null;try{if(typeof(f)=="undefined"||f===null){b=document.createElement("canvas")}else{b=f}b.getContext("2d")}catch(c){var d=navigator.appVersion.match(/MSIE (\d\.\d)/);var a=(navigator.userAgent.toLowerCase().indexOf("opera")!=-1);if((!d)||(d[1]<6)||(a)){return false}return true}return true};DygraphCanvasRenderer.prototype.render=function(){this._updatePoints();this._renderLineChart()};DygraphCanvasRenderer.prototype._createIEClipArea=function(){var g="dygraph-clip-div";var f=this.dygraph_.graphDiv;for(var e=f.childNodes.length-1;e>=0;e--){if(f.childNodes[e].className==g){f.removeChild(f.childNodes[e])}}var c=document.bgColor;var d=this.dygraph_.graphDiv;while(d!=document){var a=d.currentStyle.backgroundColor;if(a&&a!="transparent"){c=a;break}d=d.parentNode}function b(j){if(j.w===0||j.h===0){return}var i=document.createElement("div");i.className=g;i.style.backgroundColor=c;i.style.position="absolute";i.style.left=j.x+"px";i.style.top=j.y+"px";i.style.width=j.w+"px";i.style.height=j.h+"px";f.appendChild(i)}var h=this.area;b({x:0,y:0,w:h.x,h:this.height});b({x:h.x,y:0,w:this.width-h.x,h:h.y});b({x:h.x+h.w,y:0,w:this.width-h.x-h.w,h:this.height});b({x:h.x,y:h.y+h.h,w:this.width-h.x,h:this.height-h.h-h.y})};DygraphCanvasRenderer._getIteratorPredicate=function(a){return a?DygraphCanvasRenderer._predicateThatSkipsEmptyPoints:null};DygraphCanvasRenderer._predicateThatSkipsEmptyPoints=function(b,a){return b[a].yval!==null};DygraphCanvasRenderer._drawStyledLine=function(i,a,m,q,f,n,d){var h=i.dygraph;var c=h.getOption("stepPlot",i.setName);if(!Dygraph.isArrayLike(q)){q=null}var l=h.getOption("drawGapEdgePoints",i.setName);var o=i.points;var k=Dygraph.createIterator(o,0,o.length,DygraphCanvasRenderer._getIteratorPredicate(h.getOption("connectSeparatedPoints")));var j=q&&(q.length>=2);var p=i.drawingContext;p.save();if(j){p.installPattern(q)}var b=DygraphCanvasRenderer._drawSeries(i,k,m,d,f,l,c,a);DygraphCanvasRenderer._drawPointsOnLine(i,b,n,a,d);if(j){p.uninstallPattern()}p.restore()};DygraphCanvasRenderer._drawSeries=function(v,t,m,h,p,s,g,q){var b=null;var w=null;var k=null;var j;var o;var l=[];var f=true;var n=v.drawingContext;n.beginPath();n.strokeStyle=q;n.lineWidth=m;var c=t.array_;var u=t.end_;var a=t.predicate_;for(var r=t.start_;r<u;r++){o=c[r];if(a){while(r<u&&!a(c,r)){r++}if(r==u){break}o=c[r]}if(o.canvasy===null||o.canvasy!=o.canvasy){if(g&&b!==null){n.moveTo(b,w);n.lineTo(o.canvasx,w)}b=w=null}else{j=false;if(s||!b){t.nextIdx_=r;t.next();k=t.hasNext?t.peek.canvasy:null;var d=k===null||k!=k;j=(!b&&d);if(s){if((!f&&!b)||(t.hasNext&&d)){j=true}}}if(b!==null){if(m){if(g){n.moveTo(b,w);n.lineTo(o.canvasx,w)}n.lineTo(o.canvasx,o.canvasy)}}else{n.moveTo(o.canvasx,o.canvasy)}if(p||j){l.push([o.canvasx,o.canvasy,o.idx])}b=o.canvasx;w=o.canvasy}f=false}n.stroke();return l};DygraphCanvasRenderer._drawPointsOnLine=function(h,i,f,d,g){var c=h.drawingContext;for(var b=0;b<i.length;b++){var a=i[b];c.save();f(h.dygraph,h.setName,c,a[0],a[1],d,g,a[2]);c.restore()}};DygraphCanvasRenderer.prototype._updatePoints=function(){var e=this.layout.points;for(var c=e.length;c--;){var d=e[c];for(var b=d.length;b--;){var a=d[b];a.canvasx=this.area.w*a.x+this.area.x;a.canvasy=this.area.h*a.y+this.area.y}}};DygraphCanvasRenderer.prototype._renderLineChart=function(g,u){var h=u||this.elementContext;var n;var a=this.layout.points;var s=this.layout.setNames;var b;this.colors=this.dygraph_.colorsMap_;var o=this.attr_("plotter");var f=o;if(!Dygraph.isArrayLike(f)){f=[f]}var c={};for(n=0;n<s.length;n++){b=s[n];var t=this.attr_("plotter",b);if(t==o){continue}c[b]=t}for(n=0;n<f.length;n++){var r=f[n];var q=(n==f.length-1);for(var l=0;l<a.length;l++){b=s[l];if(g&&b!=g){continue}var m=a[l];var e=r;if(b in c){if(q){e=c[b]}else{continue}}var k=this.colors[b];var d=this.dygraph_.getOption("strokeWidth",b);h.save();h.strokeStyle=k;h.lineWidth=d;e({points:m,setName:b,drawingContext:h,color:k,strokeWidth:d,dygraph:this.dygraph_,axis:this.dygraph_.axisPropertiesForSeries(b),plotArea:this.area,seriesIndex:l,seriesCount:a.length,singleSeriesName:g,allSeriesPoints:a});h.restore()}}};DygraphCanvasRenderer._Plotters={linePlotter:function(a){DygraphCanvasRenderer._linePlotter(a)},fillPlotter:function(a){DygraphCanvasRenderer._fillPlotter(a)},errorPlotter:function(a){DygraphCanvasRenderer._errorPlotter(a)}};DygraphCanvasRenderer._linePlotter=function(f){var d=f.dygraph;var h=f.setName;var i=f.strokeWidth;var a=d.getOption("strokeBorderWidth",h);var j=d.getOption("drawPointCallback",h)||Dygraph.Circles.DEFAULT;var k=d.getOption("strokePattern",h);var c=d.getOption("drawPoints",h);var b=d.getOption("pointSize",h);if(a&&i){DygraphCanvasRenderer._drawStyledLine(f,d.getOption("strokeBorderColor",h),i+2*a,k,c,j,b)}DygraphCanvasRenderer._drawStyledLine(f,f.color,i,k,c,j,b)};DygraphCanvasRenderer._errorPlotter=function(s){var r=s.dygraph;var f=s.setName;var t=r.getOption("errorBars")||r.getOption("customBars");if(!t){return}var k=r.getOption("fillGraph",f);if(k){r.warn("Can't use fillGraph option with error bars")}var m=s.drawingContext;var n=s.color;var o=r.getOption("fillAlpha",f);var i=r.getOption("stepPlot",f);var p=s.points;var q=Dygraph.createIterator(p,0,p.length,DygraphCanvasRenderer._getIteratorPredicate(r.getOption("connectSeparatedPoints")));var j;var h=NaN;var c=NaN;var d=[-1,-1];var a=new RGBColorParser(n);var u="rgba("+a.r+","+a.g+","+a.b+","+o+")";m.fillStyle=u;m.beginPath();var b=function(e){return(e===null||e===undefined||isNaN(e))};while(q.hasNext){var l=q.next();if((!i&&b(l.y))||(i&&!isNaN(c)&&b(c))){h=NaN;continue}if(i){j=[l.y_bottom,l.y_top];c=l.y}else{j=[l.y_bottom,l.y_top]}j[0]=s.plotArea.h*j[0]+s.plotArea.y;j[1]=s.plotArea.h*j[1]+s.plotArea.y;if(!isNaN(h)){if(i){m.moveTo(h,d[0]);m.lineTo(l.canvasx,d[0]);m.lineTo(l.canvasx,d[1])}else{m.moveTo(h,d[0]);m.lineTo(l.canvasx,j[0]);m.lineTo(l.canvasx,j[1])}m.lineTo(h,d[1]);m.closePath()}d=j;h=l.canvasx}m.fill()};DygraphCanvasRenderer._fillPlotter=function(F){if(F.singleSeriesName){return}if(F.seriesIndex!==0){return}var D=F.dygraph;var I=D.getLabels().slice(1);for(var C=I.length;C>=0;C--){if(!D.visibility()[C]){I.splice(C,1)}}var h=(function(){for(var e=0;e<I.length;e++){if(D.getOption("fillGraph",I[e])){return true}}return false})();if(!h){return}var w=F.drawingContext;var E=F.plotArea;var c=F.allSeriesPoints;var z=c.length;var y=D.getOption("fillAlpha");var k=D.getOption("stackedGraph");var q=D.getColors();var s={};var a;var r;for(var u=z-1;u>=0;u--){var n=I[u];if(!D.getOption("fillGraph",n)){continue}var p=D.getOption("stepPlot",n);var x=q[u];var f=D.axisPropertiesForSeries(n);var d=1+f.minyval*f.yscale;if(d<0){d=0}else{if(d>1){d=1}}d=E.h*d+E.y;var B=c[u];var A=Dygraph.createIterator(B,0,B.length,DygraphCanvasRenderer._getIteratorPredicate(D.getOption("connectSeparatedPoints")));var m=NaN;var l=[-1,-1];var t;var b=new RGBColorParser(x);var H="rgba("+b.r+","+b.g+","+b.b+","+y+")";w.fillStyle=H;w.beginPath();var j,G=true;while(A.hasNext){var v=A.next();if(!Dygraph.isOK(v.y)){m=NaN;if(v.y_stacked!==null&&!isNaN(v.y_stacked)){s[v.canvasx]=E.h*v.y_stacked+E.y}continue}if(k){if(!G&&j==v.xval){continue}else{G=false;j=v.xval}a=s[v.canvasx];var o;if(a===undefined){o=d}else{if(r){o=a[0]}else{o=a}}t=[v.canvasy,o];if(p){if(l[0]===-1){s[v.canvasx]=[v.canvasy,d]}else{s[v.canvasx]=[v.canvasy,l[0]]}}else{s[v.canvasx]=v.canvasy}}else{t=[v.canvasy,d]}if(!isNaN(m)){w.moveTo(m,l[0]);if(p){w.lineTo(v.canvasx,l[0])}else{w.lineTo(v.canvasx,t[0])}if(r&&a){w.lineTo(v.canvasx,a[1])}else{w.lineTo(v.canvasx,t[1])}w.lineTo(m,l[1]);w.closePath()}l=t;m=v.canvasx}r=p;w.fill()}};"use strict";var Dygraph=function(d,c,b,a){this.is_initial_draw_=true;this.readyFns_=[];if(a!==undefined){this.warn("Using deprecated four-argument dygraph constructor");this.__old_init__(d,c,b,a)}else{this.__init__(d,c,b)}};Dygraph.NAME="Dygraph";Dygraph.VERSION="1.0.1";Dygraph.__repr__=function(){return"["+this.NAME+" "+this.VERSION+"]"};Dygraph.toString=function(){return this.__repr__()};Dygraph.DEFAULT_ROLL_PERIOD=1;Dygraph.DEFAULT_WIDTH=480;Dygraph.DEFAULT_HEIGHT=320;Dygraph.ANIMATION_STEPS=12;Dygraph.ANIMATION_DURATION=200;Dygraph.KMB_LABELS=["K","M","B","T","Q"];Dygraph.KMG2_BIG_LABELS=["k","M","G","T","P","E","Z","Y"];Dygraph.KMG2_SMALL_LABELS=["m","u","n","p","f","a","z","y"];Dygraph.numberValueFormatter=function(s,a,u,l){var e=a("sigFigs");if(e!==null){return Dygraph.floatFormat(s,e)}var c=a("digitsAfterDecimal");var o=a("maxNumberWidth");var b=a("labelsKMB");var r=a("labelsKMG2");var q;if(s!==0&&(Math.abs(s)>=Math.pow(10,o)||Math.abs(s)<Math.pow(10,-c))){q=s.toExponential(c)}else{q=""+Dygraph.round_(s,c)}if(b||r){var f;var t=[];var m=[];if(b){f=1000;t=Dygraph.KMB_LABELS}if(r){if(b){Dygraph.warn("Setting both labelsKMB and labelsKMG2. Pick one!")}f=1024;t=Dygraph.KMG2_BIG_LABELS;m=Dygraph.KMG2_SMALL_LABELS}var p=Math.abs(s);var d=Dygraph.pow(f,t.length);for(var h=t.length-1;h>=0;h--,d/=f){if(p>=d){q=Dygraph.round_(s/d,c)+t[h];break}}if(r){var i=String(s.toExponential()).split("e-");if(i.length===2&&i[1]>=3&&i[1]<=24){if(i[1]%3>0){q=Dygraph.round_(i[0]/Dygraph.pow(10,(i[1]%3)),c)}else{q=Number(i[0]).toFixed(2)}q+=m[Math.floor(i[1]/3)-1]}}}return q};Dygraph.numberAxisLabelFormatter=function(a,d,c,b){return Dygraph.numberValueFormatter(a,c,b)};Dygraph.dateString_=function(e){var i=Dygraph.zeropad;var h=new Date(e);var f=""+h.getFullYear();var g=i(h.getMonth()+1);var a=i(h.getDate());var c="";var b=h.getHours()*3600+h.getMinutes()*60+h.getSeconds();if(b){c=" "+Dygraph.hmsString_(e)}return f+"/"+g+"/"+a+c};Dygraph.dateAxisFormatter=function(b,c){if(c>=Dygraph.DECADAL){return b.strftime("%Y")}else{if(c>=Dygraph.MONTHLY){return b.strftime("%b %y")}else{var a=b.getHours()*3600+b.getMinutes()*60+b.getSeconds()+b.getMilliseconds();if(a===0||c>=Dygraph.DAILY){return new Date(b.getTime()+3600*1000).strftime("%d%b")}else{return Dygraph.hmsString_(b.getTime())}}}};Dygraph.Plotters=DygraphCanvasRenderer._Plotters;Dygraph.DEFAULT_ATTRS={highlightCircleSize:3,highlightSeriesOpts:null,highlightSeriesBackgroundAlpha:0.5,labelsDivWidth:250,labelsDivStyles:{},labelsSeparateLines:false,labelsShowZeroValues:true,labelsKMB:false,labelsKMG2:false,showLabelsOnHighlight:true,digitsAfterDecimal:2,maxNumberWidth:6,sigFigs:null,strokeWidth:1,strokeBorderWidth:0,strokeBorderColor:"white",axisTickSize:3,axisLabelFontSize:14,xAxisLabelWidth:50,yAxisLabelWidth:50,rightGap:5,showRoller:false,xValueParser:Dygraph.dateParser,delimiter:",",sigma:2,errorBars:false,fractions:false,wilsonInterval:true,customBars:false,fillGraph:false,fillAlpha:0.15,connectSeparatedPoints:false,stackedGraph:false,stackedGraphNaNFill:"all",hideOverlayOnMouseOut:true,legend:"onmouseover",stepPlot:false,avoidMinZero:false,xRangePad:0,yRangePad:null,drawAxesAtZero:false,titleHeight:28,xLabelHeight:18,yLabelWidth:18,drawXAxis:true,drawYAxis:true,axisLineColor:"black",axisLineWidth:0.3,gridLineWidth:0.3,axisLabelColor:"black",axisLabelFont:"Arial",axisLabelWidth:50,drawYGrid:true,drawXGrid:true,gridLineColor:"rgb(128,128,128)",interactionModel:null,animatedZooms:false,showRangeSelector:false,rangeSelectorHeight:40,rangeSelectorPlotStrokeColor:"#808FAB",rangeSelectorPlotFillColor:"#A7B1C4",plotter:[Dygraph.Plotters.fillPlotter,Dygraph.Plotters.errorPlotter,Dygraph.Plotters.linePlotter],plugins:[],axes:{x:{pixelsPerLabel:60,axisLabelFormatter:Dygraph.dateAxisFormatter,valueFormatter:Dygraph.dateString_,drawGrid:true,independentTicks:true,ticker:null},y:{pixelsPerLabel:30,valueFormatter:Dygraph.numberValueFormatter,axisLabelFormatter:Dygraph.numberAxisLabelFormatter,drawGrid:true,independentTicks:true,ticker:null},y2:{pixelsPerLabel:30,valueFormatter:Dygraph.numberValueFormatter,axisLabelFormatter:Dygraph.numberAxisLabelFormatter,drawGrid:false,independentTicks:false,ticker:null}}};Dygraph.HORIZONTAL=1;Dygraph.VERTICAL=2;Dygraph.PLUGINS=[];Dygraph.addedAnnotationCSS=false;Dygraph.prototype.__old_init__=function(f,d,e,b){if(e!==null){var a=["Date"];for(var c=0;c<e.length;c++){a.push(e[c])}Dygraph.update(b,{labels:a})}this.__init__(f,d,b)};Dygraph.prototype.__init__=function(a,c,l){if(/MSIE/.test(navigator.userAgent)&&!window.opera&&typeof(G_vmlCanvasManager)!="undefined"&&document.readyState!="complete"){var o=this;setTimeout(function(){o.__init__(a,c,l)},100);return}if(l===null||l===undefined){l={}}l=Dygraph.mapLegacyOptions_(l);if(typeof(a)=="string"){a=document.getElementById(a)}if(!a){Dygraph.error("Constructing dygraph with a non-existent div!");return}this.isUsingExcanvas_=typeof(G_vmlCanvasManager)!="undefined";this.maindiv_=a;this.file_=c;this.rollPeriod_=l.rollPeriod||Dygraph.DEFAULT_ROLL_PERIOD;this.previousVerticalX_=-1;this.fractions_=l.fractions||false;this.dateWindow_=l.dateWindow||null;this.annotations_=[];this.zoomed_x_=false;this.zoomed_y_=false;a.innerHTML="";if(a.style.width===""&&l.width){a.style.width=l.width+"px"}if(a.style.height===""&&l.height){a.style.height=l.height+"px"}if(a.style.height===""&&a.clientHeight===0){a.style.height=Dygraph.DEFAULT_HEIGHT+"px";if(a.style.width===""){a.style.width=Dygraph.DEFAULT_WIDTH+"px"}}this.width_=a.clientWidth||l.width||0;this.height_=a.clientHeight||l.height||0;if(l.stackedGraph){l.fillGraph=true}this.user_attrs_={};Dygraph.update(this.user_attrs_,l);this.attrs_={};Dygraph.updateDeep(this.attrs_,Dygraph.DEFAULT_ATTRS);this.boundaryIds_=[];this.setIndexByName_={};this.datasetIndex_=[];this.registeredEvents_=[];this.eventListeners_={};this.attributes_=new DygraphOptions(this);this.createInterface_();this.plugins_=[];var d=Dygraph.PLUGINS.concat(this.getOption("plugins"));for(var g=0;g<d.length;g++){var k=d[g];var f=new k();var j={plugin:f,events:{},options:{},pluginOptions:{}};var b=f.activate(this);for(var h in b){j.events[h]=b[h]}this.plugins_.push(j)}for(var g=0;g<this.plugins_.length;g++){var n=this.plugins_[g];for(var h in n.events){if(!n.events.hasOwnProperty(h)){continue}var m=n.events[h];var e=[n.plugin,m];if(!(h in this.eventListeners_)){this.eventListeners_[h]=[e]}else{this.eventListeners_[h].push(e)}}}this.createDragInterface_();this.start_()};Dygraph.prototype.cascadeEvents_=function(c,b){if(!(c in this.eventListeners_)){return true}var g={dygraph:this,cancelable:false,defaultPrevented:false,preventDefault:function(){if(!g.cancelable){throw"Cannot call preventDefault on non-cancelable event."}g.defaultPrevented=true},propagationStopped:false,stopPropagation:function(){g.propagationStopped=true}};Dygraph.update(g,b);var a=this.eventListeners_[c];if(a){for(var d=a.length-1;d>=0;d--){var f=a[d][0];var h=a[d][1];h.call(f,g);if(g.propagationStopped){break}}}return g.defaultPrevented};Dygraph.prototype.isZoomed=function(a){if(a===null||a===undefined){return this.zoomed_x_||this.zoomed_y_}if(a==="x"){return this.zoomed_x_}if(a==="y"){return this.zoomed_y_}throw"axis parameter is ["+a+"] must be null, 'x' or 'y'."};Dygraph.prototype.toString=function(){var a=this.maindiv_;var b=(a&&a.id)?a.id:a;return"[Dygraph "+b+"]"};Dygraph.prototype.attr_=function(b,a){return a?this.attributes_.getForSeries(b,a):this.attributes_.get(b)};Dygraph.prototype.getOption=function(a,b){return this.attr_(a,b)};Dygraph.prototype.getOptionForAxis=function(a,b){return this.attributes_.getForAxis(a,b)};Dygraph.prototype.optionsViewForAxis_=function(b){var a=this;return function(c){var d=a.user_attrs_.axes;if(d&&d[b]&&d[b].hasOwnProperty(c)){return d[b][c]}if(typeof(a.user_attrs_[c])!="undefined"){return a.user_attrs_[c]}d=a.attrs_.axes;if(d&&d[b]&&d[b].hasOwnProperty(c)){return d[b][c]}if(b=="y"&&a.axes_[0].hasOwnProperty(c)){return a.axes_[0][c]}else{if(b=="y2"&&a.axes_[1].hasOwnProperty(c)){return a.axes_[1][c]}}return a.attr_(c)}};Dygraph.prototype.rollPeriod=function(){return this.rollPeriod_};Dygraph.prototype.xAxisRange=function(){return this.dateWindow_?this.dateWindow_:this.xAxisExtremes()};Dygraph.prototype.xAxisExtremes=function(){var d=this.attr_("xRangePad")/this.plotter_.area.w;if(this.numRows()===0){return[0-d,1+d]}var c=this.rawData_[0][0];var b=this.rawData_[this.rawData_.length-1][0];if(d){var a=b-c;c-=a*d;b+=a*d}return[c,b]};Dygraph.prototype.yAxisRange=function(a){if(typeof(a)=="undefined"){a=0}if(a<0||a>=this.axes_.length){return null}var b=this.axes_[a];return[b.computedValueRange[0],b.computedValueRange[1]]};Dygraph.prototype.yAxisRanges=function(){var a=[];for(var b=0;b<this.axes_.length;b++){a.push(this.yAxisRange(b))}return a};Dygraph.prototype.toDomCoords=function(a,c,b){return[this.toDomXCoord(a),this.toDomYCoord(c,b)]};Dygraph.prototype.toDomXCoord=function(b){if(b===null){return null}var c=this.plotter_.area;var a=this.xAxisRange();return c.x+(b-a[0])/(a[1]-a[0])*c.w};Dygraph.prototype.toDomYCoord=function(d,a){var c=this.toPercentYCoord(d,a);if(c===null){return null}var b=this.plotter_.area;return b.y+c*b.h};Dygraph.prototype.toDataCoords=function(a,c,b){return[this.toDataXCoord(a),this.toDataYCoord(c,b)]};Dygraph.prototype.toDataXCoord=function(b){if(b===null){return null}var c=this.plotter_.area;var a=this.xAxisRange();return a[0]+(b-c.x)/c.w*(a[1]-a[0])};Dygraph.prototype.toDataYCoord=function(h,b){if(h===null){return null}var c=this.plotter_.area;var g=this.yAxisRange(b);if(typeof(b)=="undefined"){b=0}if(!this.attributes_.getForAxis("logscale",b)){return g[0]+(c.y+c.h-h)/c.h*(g[1]-g[0])}else{var f=(h-c.y)/c.h;var a=Dygraph.log10(g[1]);var e=a-(f*(a-Dygraph.log10(g[0])));var d=Math.pow(Dygraph.LOG_SCALE,e);return d}};Dygraph.prototype.toPercentYCoord=function(f,c){if(f===null){return null}if(typeof(c)=="undefined"){c=0}var e=this.yAxisRange(c);var d;var b=this.attributes_.getForAxis("logscale",c);if(!b){d=(e[1]-f)/(e[1]-e[0])}else{var a=Dygraph.log10(e[1]);d=(a-Dygraph.log10(f))/(a-Dygraph.log10(e[0]))}return d};Dygraph.prototype.toPercentXCoord=function(b){if(b===null){return null}var a=this.xAxisRange();return(b-a[0])/(a[1]-a[0])};Dygraph.prototype.numColumns=function(){if(!this.rawData_){return 0}return this.rawData_[0]?this.rawData_[0].length:this.attr_("labels").length};Dygraph.prototype.numRows=function(){if(!this.rawData_){return 0}return this.rawData_.length};Dygraph.prototype.getValue=function(b,a){if(b<0||b>this.rawData_.length){return null}if(a<0||a>this.rawData_[b].length){return null}return this.rawData_[b][a]};Dygraph.prototype.createInterface_=function(){var a=this.maindiv_;this.graphDiv=document.createElement("div");this.graphDiv.style.textAlign="left";a.appendChild(this.graphDiv);this.canvas_=Dygraph.createCanvas();this.canvas_.style.position="absolute";this.hidden_=this.createPlotKitCanvas_(this.canvas_);this.resizeElements_();this.canvas_ctx_=Dygraph.getContext(this.canvas_);this.hidden_ctx_=Dygraph.getContext(this.hidden_);this.graphDiv.appendChild(this.hidden_);this.graphDiv.appendChild(this.canvas_);this.mouseEventElement_=this.createMouseEventElement_();this.layout_=new DygraphLayout(this);var b=this;this.mouseMoveHandler_=function(c){b.mouseMove_(c)};this.mouseOutHandler_=function(f){var d=f.target||f.fromElement;var c=f.relatedTarget||f.toElement;if(Dygraph.isNodeContainedBy(d,b.graphDiv)&&!Dygraph.isNodeContainedBy(c,b.graphDiv)){b.mouseOut_(f)}};this.addAndTrackEvent(window,"mouseout",this.mouseOutHandler_);this.addAndTrackEvent(this.mouseEventElement_,"mousemove",this.mouseMoveHandler_);if(!this.resizeHandler_){this.resizeHandler_=function(c){b.resize()};this.addAndTrackEvent(window,"resize",this.resizeHandler_)}};Dygraph.prototype.resizeElements_=function(){this.graphDiv.style.width=this.width_+"px";this.graphDiv.style.height=this.height_+"px";this.canvas_.width=this.width_;this.canvas_.height=this.height_;this.canvas_.style.width=this.width_+"px";this.canvas_.style.height=this.height_+"px";this.hidden_.width=this.width_;this.hidden_.height=this.height_;this.hidden_.style.width=this.width_+"px";this.hidden_.style.height=this.height_+"px"};Dygraph.prototype.destroy=function(){this.canvas_ctx_.restore();this.hidden_ctx_.restore();var a=function(c){while(c.hasChildNodes()){a(c.firstChild);c.removeChild(c.firstChild)}};this.removeTrackedEvents_();Dygraph.removeEvent(window,"mouseout",this.mouseOutHandler_);Dygraph.removeEvent(this.mouseEventElement_,"mousemove",this.mouseMoveHandler_);Dygraph.removeEvent(window,"resize",this.resizeHandler_);this.resizeHandler_=null;a(this.maindiv_);var b=function(c){for(var d in c){if(typeof(c[d])==="object"){c[d]=null}}};b(this.layout_);b(this.plotter_);b(this)};Dygraph.prototype.createPlotKitCanvas_=function(a){var b=Dygraph.createCanvas();b.style.position="absolute";b.style.top=a.style.top;b.style.left=a.style.left;b.width=this.width_;b.height=this.height_;b.style.width=this.width_+"px";b.style.height=this.height_+"px";return b};Dygraph.prototype.createMouseEventElement_=function(){if(this.isUsingExcanvas_){var a=document.createElement("div");a.style.position="absolute";a.style.backgroundColor="white";a.style.filter="alpha(opacity=0)";a.style.width=this.width_+"px";a.style.height=this.height_+"px";this.graphDiv.appendChild(a);return a}else{return this.canvas_}};Dygraph.prototype.setColors_=function(){var g=this.getLabels();var e=g.length-1;this.colors_=[];this.colorsMap_={};var a=this.attr_("colors");var d;if(!a){var c=this.attr_("colorSaturation")||1;var b=this.attr_("colorValue")||0.5;var k=Math.ceil(e/2);for(d=1;d<=e;d++){if(!this.visibility()[d-1]){continue}var h=d%2?Math.ceil(d/2):(k+d/2);var f=(1*h/(1+e));var j=Dygraph.hsvToRGB(f,c,b);this.colors_.push(j);this.colorsMap_[g[d]]=j}}else{for(d=0;d<e;d++){if(!this.visibility()[d]){continue}var j=a[d%a.length];this.colors_.push(j);this.colorsMap_[g[1+d]]=j}}};Dygraph.prototype.getColors=function(){return this.colors_};Dygraph.prototype.getPropertiesForSeries=function(c){var a=-1;var d=this.getLabels();for(var b=1;b<d.length;b++){if(d[b]==c){a=b;break}}if(a==-1){return null}return{name:c,column:a,visible:this.visibility()[a-1],color:this.colorsMap_[c],axis:1+this.attributes_.axisForSeries(c)}};Dygraph.prototype.createRollInterface_=function(){if(!this.roller_){this.roller_=document.createElement("input");this.roller_.type="text";this.roller_.style.display="none";this.graphDiv.appendChild(this.roller_)}var e=this.attr_("showRoller")?"block":"none";var d=this.plotter_.area;var b={position:"absolute",zIndex:10,top:(d.y+d.h-25)+"px",left:(d.x+1)+"px",display:e};this.roller_.size="2";this.roller_.value=this.rollPeriod_;for(var a in b){if(b.hasOwnProperty(a)){this.roller_.style[a]=b[a]}}var c=this;this.roller_.onchange=function(){c.adjustRoll(c.roller_.value)}};Dygraph.prototype.dragGetX_=function(b,a){return Dygraph.pageX(b)-a.px};Dygraph.prototype.dragGetY_=function(b,a){return Dygraph.pageY(b)-a.py};Dygraph.prototype.createDragInterface_=function(){var c={isZooming:false,isPanning:false,is2DPan:false,dragStartX:null,dragStartY:null,dragEndX:null,dragEndY:null,dragDirection:null,prevEndX:null,prevEndY:null,prevDragDirection:null,cancelNextDblclick:false,initialLeftmostDate:null,xUnitsPerPixel:null,dateRange:null,px:0,py:0,boundedDates:null,boundedValues:null,tarp:new Dygraph.IFrameTarp(),initializeMouseDown:function(j,i,h){if(j.preventDefault){j.preventDefault()}else{j.returnValue=false;j.cancelBubble=true}h.px=Dygraph.findPosX(i.canvas_);h.py=Dygraph.findPosY(i.canvas_);h.dragStartX=i.dragGetX_(j,h);h.dragStartY=i.dragGetY_(j,h);h.cancelNextDblclick=false;h.tarp.cover()}};var f=this.attr_("interactionModel");var b=this;var e=function(g){return function(h){g(h,b,c)}};for(var a in f){if(!f.hasOwnProperty(a)){continue}this.addAndTrackEvent(this.mouseEventElement_,a,e(f[a]))}var d=function(h){if(c.isZooming||c.isPanning){c.isZooming=false;c.dragStartX=null;c.dragStartY=null}if(c.isPanning){c.isPanning=false;c.draggingDate=null;c.dateRange=null;for(var g=0;g<b.axes_.length;g++){delete b.axes_[g].draggingValue;delete b.axes_[g].dragValueRange}}c.tarp.uncover()};this.addAndTrackEvent(document,"mouseup",d)};Dygraph.prototype.drawZoomRect_=function(e,c,i,b,g,a,f,d){var h=this.canvas_ctx_;if(a==Dygraph.HORIZONTAL){h.clearRect(Math.min(c,f),this.layout_.getPlotArea().y,Math.abs(c-f),this.layout_.getPlotArea().h)}else{if(a==Dygraph.VERTICAL){h.clearRect(this.layout_.getPlotArea().x,Math.min(b,d),this.layout_.getPlotArea().w,Math.abs(b-d))}}if(e==Dygraph.HORIZONTAL){if(i&&c){h.fillStyle="rgba(128,128,128,0.33)";h.fillRect(Math.min(c,i),this.layout_.getPlotArea().y,Math.abs(i-c),this.layout_.getPlotArea().h)}}else{if(e==Dygraph.VERTICAL){if(g&&b){h.fillStyle="rgba(128,128,128,0.33)";h.fillRect(this.layout_.getPlotArea().x,Math.min(b,g),this.layout_.getPlotArea().w,Math.abs(g-b))}}}if(this.isUsingExcanvas_){this.currentZoomRectArgs_=[e,c,i,b,g,0,0,0]}};Dygraph.prototype.clearZoomRect_=function(){this.currentZoomRectArgs_=null;this.canvas_ctx_.clearRect(0,0,this.canvas_.width,this.canvas_.height)};Dygraph.prototype.doZoomX_=function(c,a){this.currentZoomRectArgs_=null;var b=this.toDataXCoord(c);var d=this.toDataXCoord(a);this.doZoomXDates_(b,d)};Dygraph.zoomAnimationFunction=function(c,b){var a=1.5;return(1-Math.pow(a,-c))/(1-Math.pow(a,-b))};Dygraph.prototype.doZoomXDates_=function(c,e){var a=this.xAxisRange();var d=[c,e];this.zoomed_x_=true;var b=this;this.doAnimatedZoom(a,d,null,null,function(){if(b.attr_("zoomCallback")){b.attr_("zoomCallback")(c,e,b.yAxisRanges())}})};Dygraph.prototype.doZoomY_=function(h,f){this.currentZoomRectArgs_=null;var c=this.yAxisRanges();var b=[];for(var e=0;e<this.axes_.length;e++){var d=this.toDataYCoord(h,e);var a=this.toDataYCoord(f,e);b.push([a,d])}this.zoomed_y_=true;var g=this;this.doAnimatedZoom(null,null,c,b,function(){if(g.attr_("zoomCallback")){var i=g.xAxisRange();g.attr_("zoomCallback")(i[0],i[1],g.yAxisRanges())}})};Dygraph.prototype.resetZoom=function(){var c=false,d=false,a=false;if(this.dateWindow_!==null){c=true;d=true}for(var g=0;g<this.axes_.length;g++){if(typeof(this.axes_[g].valueWindow)!=="undefined"&&this.axes_[g].valueWindow!==null){c=true;a=true}}this.clearSelection();if(c){this.zoomed_x_=false;this.zoomed_y_=false;var f=this.rawData_[0][0];var b=this.rawData_[this.rawData_.length-1][0];if(!this.attr_("animatedZooms")){this.dateWindow_=null;for(g=0;g<this.axes_.length;g++){if(this.axes_[g].valueWindow!==null){delete this.axes_[g].valueWindow}}this.drawGraph_();if(this.attr_("zoomCallback")){this.attr_("zoomCallback")(f,b,this.yAxisRanges())}return}var l=null,m=null,k=null,h=null;if(d){l=this.xAxisRange();m=[f,b]}if(a){k=this.yAxisRanges();var n=this.gatherDatasets_(this.rolledSeries_,null);var o=n.extremes;this.computeYAxisRanges_(o);h=[];for(g=0;g<this.axes_.length;g++){var e=this.axes_[g];h.push((e.valueRange!==null&&e.valueRange!==undefined)?e.valueRange:e.extremeRange)}}var j=this;this.doAnimatedZoom(l,m,k,h,function(){j.dateWindow_=null;for(var p=0;p<j.axes_.length;p++){if(j.axes_[p].valueWindow!==null){delete j.axes_[p].valueWindow}}if(j.attr_("zoomCallback")){j.attr_("zoomCallback")(f,b,j.yAxisRanges())}})}};Dygraph.prototype.doAnimatedZoom=function(a,e,b,c,m){var i=this.attr_("animatedZooms")?Dygraph.ANIMATION_STEPS:1;var l=[];var k=[];var f,d;if(a!==null&&e!==null){for(f=1;f<=i;f++){d=Dygraph.zoomAnimationFunction(f,i);l[f-1]=[a[0]*(1-d)+d*e[0],a[1]*(1-d)+d*e[1]]}}if(b!==null&&c!==null){for(f=1;f<=i;f++){d=Dygraph.zoomAnimationFunction(f,i);var n=[];for(var g=0;g<this.axes_.length;g++){n.push([b[g][0]*(1-d)+d*c[g][0],b[g][1]*(1-d)+d*c[g][1]])}k[f-1]=n}}var h=this;Dygraph.repeatAndCleanup(function(p){if(k.length){for(var o=0;o<h.axes_.length;o++){var j=k[p][o];h.axes_[o].valueWindow=[j[0],j[1]]}}if(l.length){h.dateWindow_=l[p]}h.drawGraph_()},i,Dygraph.ANIMATION_DURATION/i,m)};Dygraph.prototype.getArea=function(){return this.plotter_.area};Dygraph.prototype.eventToDomCoords=function(c){if(c.offsetX&&c.offsetY){return[c.offsetX,c.offsetY]}else{var b=Dygraph.pageX(c)-Dygraph.findPosX(this.mouseEventElement_);var a=Dygraph.pageY(c)-Dygraph.findPosY(this.mouseEventElement_);return[b,a]}};Dygraph.prototype.findClosestRow=function(a){var g=Infinity;var h=-1;var e=this.layout_.points;for(var c=0;c<e.length;c++){var l=e[c];var d=l.length;for(var b=0;b<d;b++){var k=l[b];if(!Dygraph.isValidPoint(k,true)){continue}var f=Math.abs(k.canvasx-a);if(f<g){g=f;h=k.idx}}}return h};Dygraph.prototype.findClosestPoint=function(f,e){var k=Infinity;var h,o,n,l,d,c,j;for(var b=this.layout_.points.length-1;b>=0;--b){var m=this.layout_.points[b];for(var g=0;g<m.length;++g){l=m[g];if(!Dygraph.isValidPoint(l)){continue}o=l.canvasx-f;n=l.canvasy-e;h=o*o+n*n;if(h<k){k=h;d=l;c=b;j=l.idx}}}var a=this.layout_.setNames[c];return{row:j,seriesName:a,point:d}};Dygraph.prototype.findStackedPoint=function(i,h){var p=this.findClosestRow(i);var f,c;for(var d=0;d<this.layout_.points.length;++d){var g=this.getLeftBoundary_(d);var e=p-g;var l=this.layout_.points[d];if(e>=l.length){continue}var m=l[e];if(!Dygraph.isValidPoint(m)){continue}var j=m.canvasy;if(i>m.canvasx&&e+1<l.length){var k=l[e+1];if(Dygraph.isValidPoint(k)){var o=k.canvasx-m.canvasx;if(o>0){var a=(i-m.canvasx)/o;j+=a*(k.canvasy-m.canvasy)}}}else{if(i<m.canvasx&&e>0){var n=l[e-1];if(Dygraph.isValidPoint(n)){var o=m.canvasx-n.canvasx;if(o>0){var a=(m.canvasx-i)/o;j+=a*(n.canvasy-m.canvasy)}}}}if(d===0||j<h){f=m;c=d}}var b=this.layout_.setNames[c];return{row:p,seriesName:b,point:f}};Dygraph.prototype.mouseMove_=function(b){var h=this.layout_.points;if(h===undefined||h===null){return}var e=this.eventToDomCoords(b);var a=e[0];var j=e[1];var f=this.attr_("highlightSeriesOpts");var d=false;if(f&&!this.isSeriesLocked()){var c;if(this.attr_("stackedGraph")){c=this.findStackedPoint(a,j)}else{c=this.findClosestPoint(a,j)}d=this.setSelection(c.row,c.seriesName)}else{var g=this.findClosestRow(a);d=this.setSelection(g)}var i=this.attr_("highlightCallback");if(i&&d){i(b,this.lastx_,this.selPoints_,this.lastRow_,this.highlightSet_)}};Dygraph.prototype.getLeftBoundary_=function(b){if(this.boundaryIds_[b]){return this.boundaryIds_[b][0]}else{for(var a=0;a<this.boundaryIds_.length;a++){if(this.boundaryIds_[a]!==undefined){return this.boundaryIds_[a][0]}}return 0}};Dygraph.prototype.animateSelection_=function(f){var e=10;var c=30;if(this.fadeLevel===undefined){this.fadeLevel=0}if(this.animateId===undefined){this.animateId=0}var g=this.fadeLevel;var b=f<0?g:e-g;if(b<=0){if(this.fadeLevel){this.updateSelection_(1)}return}var a=++this.animateId;var d=this;Dygraph.repeatAndCleanup(function(h){if(d.animateId!=a){return}d.fadeLevel+=f;if(d.fadeLevel===0){d.clearSelection()}else{d.updateSelection_(d.fadeLevel/e)}},b,c,function(){})};Dygraph.prototype.updateSelection_=function(d){this.cascadeEvents_("select",{selectedX:this.lastx_,selectedPoints:this.selPoints_});var h;var n=this.canvas_ctx_;if(this.attr_("highlightSeriesOpts")){n.clearRect(0,0,this.width_,this.height_);var f=1-this.attr_("highlightSeriesBackgroundAlpha");if(f){var g=true;if(g){if(d===undefined){this.animateSelection_(1);return}f*=d}n.fillStyle="rgba(255,255,255,"+f+")";n.fillRect(0,0,this.width_,this.height_)}this.plotter_._renderLineChart(this.highlightSet_,n)}else{if(this.previousVerticalX_>=0){var j=0;var k=this.attr_("labels");for(h=1;h<k.length;h++){var c=this.attr_("highlightCircleSize",k[h]);if(c>j){j=c}}var l=this.previousVerticalX_;n.clearRect(l-j-1,0,2*j+2,this.height_)}}if(this.isUsingExcanvas_&&this.currentZoomRectArgs_){Dygraph.prototype.drawZoomRect_.apply(this,this.currentZoomRectArgs_)}if(this.selPoints_.length>0){var b=this.selPoints_[0].canvasx;n.save();for(h=0;h<this.selPoints_.length;h++){var o=this.selPoints_[h];if(!Dygraph.isOK(o.canvasy)){continue}var a=this.attr_("highlightCircleSize",o.name);var m=this.attr_("drawHighlightPointCallback",o.name);var e=this.plotter_.colors[o.name];if(!m){m=Dygraph.Circles.DEFAULT}n.lineWidth=this.attr_("strokeWidth",o.name);n.strokeStyle=e;n.fillStyle=e;m(this.g,o.name,n,b,o.canvasy,e,a,o.idx)}n.restore();this.previousVerticalX_=b}};Dygraph.prototype.setSelection=function(f,h,g){this.selPoints_=[];var e=false;if(f!==false&&f>=0){if(f!=this.lastRow_){e=true}this.lastRow_=f;for(var d=0;d<this.layout_.points.length;++d){var b=this.layout_.points[d];var c=f-this.getLeftBoundary_(d);if(c<b.length){var a=b[c];if(a.yval!==null){this.selPoints_.push(a)}}}}else{if(this.lastRow_>=0){e=true}this.lastRow_=-1}if(this.selPoints_.length){this.lastx_=this.selPoints_[0].xval}else{this.lastx_=-1}if(h!==undefined){if(this.highlightSet_!==h){e=true}this.highlightSet_=h}if(g!==undefined){this.lockedSet_=g}if(e){this.updateSelection_(undefined)}return e};Dygraph.prototype.mouseOut_=function(a){if(this.attr_("unhighlightCallback")){this.attr_("unhighlightCallback")(a)}if(this.attr_("hideOverlayOnMouseOut")&&!this.lockedSet_){this.clearSelection()}};Dygraph.prototype.clearSelection=function(){this.cascadeEvents_("deselect",{});this.lockedSet_=false;if(this.fadeLevel){this.animateSelection_(-1);return}this.canvas_ctx_.clearRect(0,0,this.width_,this.height_);this.fadeLevel=0;this.selPoints_=[];this.lastx_=-1;this.lastRow_=-1;this.highlightSet_=null};Dygraph.prototype.getSelection=function(){if(!this.selPoints_||this.selPoints_.length<1){return -1}for(var c=0;c<this.layout_.points.length;c++){var a=this.layout_.points[c];for(var b=0;b<a.length;b++){if(a[b].x==this.selPoints_[0].x){return a[b].idx}}}return -1};Dygraph.prototype.getHighlightSeries=function(){return this.highlightSet_};Dygraph.prototype.isSeriesLocked=function(){return this.lockedSet_};Dygraph.prototype.loadedEvent_=function(a){this.rawData_=this.parseCSV_(a);this.predraw_()};Dygraph.prototype.addXTicks_=function(){var a;if(this.dateWindow_){a=[this.dateWindow_[0],this.dateWindow_[1]]}else{a=this.xAxisExtremes()}var c=this.optionsViewForAxis_("x");var b=c("ticker")(a[0],a[1],this.width_,c,this);this.layout_.setXTicks(b)};Dygraph.prototype.extremeValues_=function(d){var h=null,f=null,c,g;var b=this.attr_("errorBars")||this.attr_("customBars");if(b){for(c=0;c<d.length;c++){g=d[c][1][0];if(g===null||isNaN(g)){continue}var a=g-d[c][1][1];var e=g+d[c][1][2];if(a>g){a=g}if(e<g){e=g}if(f===null||e>f){f=e}if(h===null||a<h){h=a}}}else{for(c=0;c<d.length;c++){g=d[c][1];if(g===null||isNaN(g)){continue}if(f===null||g>f){f=g}if(h===null||g<h){h=g}}}return[h,f]};Dygraph.prototype.predraw_=function(){var e=new Date();this.layout_.computePlotArea();this.computeYAxes_();if(this.plotter_){this.cascadeEvents_("clearChart");this.plotter_.clear()}if(!this.is_initial_draw_){this.canvas_ctx_.restore();this.hidden_ctx_.restore()}this.canvas_ctx_.save();this.hidden_ctx_.save();this.plotter_=new DygraphCanvasRenderer(this,this.hidden_,this.hidden_ctx_,this.layout_);this.createRollInterface_();this.cascadeEvents_("predraw");this.rolledSeries_=[null];for(var c=1;c<this.numColumns();c++){var d=this.attr_("logscale");var b=this.extractSeries_(this.rawData_,c,d);b=this.rollingAverage(b,this.rollPeriod_);this.rolledSeries_.push(b)}this.drawGraph_();var a=new Date();this.drawingTimeMs_=(a-e)};Dygraph.PointType=undefined;Dygraph.seriesToPoints_=function(b,j,d,f){var h=[];for(var c=0;c<b.length;++c){var k=b[c];var a=j?k[1][0]:k[1];var e=a===null?null:DygraphLayout.parseFloat_(a);var g={x:NaN,y:NaN,xval:DygraphLayout.parseFloat_(k[0]),yval:e,name:d,idx:c+f};if(j){g.y_top=NaN;g.y_bottom=NaN;g.yval_minus=DygraphLayout.parseFloat_(k[1][1]);g.yval_plus=DygraphLayout.parseFloat_(k[1][2])}h.push(g)}return h};Dygraph.stackPoints_=function(m,l,o,f){var h=null;var d=null;var g=null;var c=-1;var k=function(i){if(c>=i){return}for(var p=i;p<m.length;++p){g=null;if(!isNaN(m[p].yval)&&m[p].yval!==null){c=p;g=m[p];break}}};for(var b=0;b<m.length;++b){var j=m[b];var n=j.xval;if(l[n]===undefined){l[n]=0}var e=j.yval;if(isNaN(e)||e===null){k(b);if(d&&g&&f!="none"){e=d.yval+(g.yval-d.yval)*((n-d.xval)/(g.xval-d.xval))}else{if(d&&f=="all"){e=d.yval}else{if(g&&f=="all"){e=g.yval}else{e=0}}}}else{d=j}var a=l[n];if(h!=n){a+=e;l[n]=a}h=n;j.yval_stacked=a;if(a>o[1]){o[1]=a}if(a<o[0]){o[0]=a}}};Dygraph.prototype.gatherDatasets_=function(w,f){var s=[];var t=[];var q=[];var a={};var u,r;var x=this.attr_("errorBars");var h=this.attr_("customBars");var p=x||h;var b=function(i){if(!p){return i[1]===null}else{return h?i[1][1]===null:x?i[1][0]===null:false}};var n=w.length-1;var l;for(u=n;u>=1;u--){if(!this.visibility()[u-1]){continue}if(f){l=w[u];var z=f[0];var j=f[1];var g=null,y=null;for(r=0;r<l.length;r++){if(l[r][0]>=z&&g===null){g=r}if(l[r][0]<=j){y=r}}if(g===null){g=0}var e=g;var d=true;while(d&&e>0){e--;d=b(l[e])}if(y===null){y=l.length-1}var c=y;d=true;while(d&&c<l.length-1){c++;d=b(l[c])}if(e!==g){g=e}if(c!==y){y=c}s[u-1]=[g,y];l=l.slice(g,y+1)}else{l=w[u];s[u-1]=[0,l.length-1]}var v=this.attr_("labels")[u];var o=this.extremeValues_(l);var m=Dygraph.seriesToPoints_(l,p,v,s[u-1][0]);if(this.attr_("stackedGraph")){Dygraph.stackPoints_(m,q,o,this.attr_("stackedGraphNaNFill"))}a[v]=o;t[u]=m}return{points:t,extremes:a,boundaryIds:s}};Dygraph.prototype.drawGraph_=function(){var a=new Date();var d=this.is_initial_draw_;this.is_initial_draw_=false;this.layout_.removeAllDatasets();this.setColors_();this.attrs_.pointSize=0.5*this.attr_("highlightCircleSize");var j=this.gatherDatasets_(this.rolledSeries_,this.dateWindow_);var h=j.points;var k=j.extremes;this.boundaryIds_=j.boundaryIds;this.setIndexByName_={};var g=this.attr_("labels");if(g.length>0){this.setIndexByName_[g[0]]=0}var e=0;for(var f=1;f<h.length;f++){this.setIndexByName_[g[f]]=f;if(!this.visibility()[f-1]){continue}this.layout_.addDataset(g[f],h[f]);this.datasetIndex_[f]=e++}this.computeYAxisRanges_(k);this.layout_.setYAxes(this.axes_);this.addXTicks_();var b=this.zoomed_x_;this.zoomed_x_=b;this.layout_.evaluate();this.renderGraph_(d);if(this.attr_("timingName")){var c=new Date();Dygraph.info(this.attr_("timingName")+" - drawGraph: "+(c-a)+"ms")}};Dygraph.prototype.renderGraph_=function(b){this.cascadeEvents_("clearChart");this.plotter_.clear();if(this.attr_("underlayCallback")){this.attr_("underlayCallback")(this.hidden_ctx_,this.layout_.getPlotArea(),this,this)}var c={canvas:this.hidden_,drawingContext:this.hidden_ctx_};this.cascadeEvents_("willDrawChart",c);this.plotter_.render();this.cascadeEvents_("didDrawChart",c);this.lastRow_=-1;this.canvas_.getContext("2d").clearRect(0,0,this.canvas_.width,this.canvas_.height);if(this.attr_("drawCallback")!==null){this.attr_("drawCallback")(this,b)}if(b){this.readyFired_=true;while(this.readyFns_.length>0){var a=this.readyFns_.pop();a(this)}}};Dygraph.prototype.computeYAxes_=function(){var b,d,c,f,a;if(this.axes_!==undefined&&this.user_attrs_.hasOwnProperty("valueRange")===false){b=[];for(c=0;c<this.axes_.length;c++){b.push(this.axes_[c].valueWindow)}}this.axes_=[];for(d=0;d<this.attributes_.numAxes();d++){f={g:this};Dygraph.update(f,this.attributes_.axisOptions(d));this.axes_[d]=f}a=this.attr_("valueRange");if(a){this.axes_[0].valueRange=a}if(b!==undefined){var e=Math.min(b.length,this.axes_.length);for(c=0;c<e;c++){this.axes_[c].valueWindow=b[c]}}for(d=0;d<this.axes_.length;d++){if(d===0){f=this.optionsViewForAxis_("y"+(d?"2":""));a=f("valueRange");if(a){this.axes_[d].valueRange=a}}else{var g=this.user_attrs_.axes;if(g&&g.y2){a=g.y2.valueRange;if(a){this.axes_[d].valueRange=a}}}}};Dygraph.prototype.numAxes=function(){return this.attributes_.numAxes()};Dygraph.prototype.axisPropertiesForSeries=function(a){return this.axes_[this.attributes_.axisForSeries(a)]};Dygraph.prototype.computeYAxisRanges_=function(a){var g=function(i){return isNaN(parseFloat(i))};var q=this.attributes_.numAxes();var b,x,o,B;var p;for(var y=0;y<q;y++){var c=this.axes_[y];var C=this.attributes_.getForAxis("logscale",y);var G=this.attributes_.getForAxis("includeZero",y);var l=this.attributes_.getForAxis("independentTicks",y);o=this.attributes_.seriesForAxis(y);b=true;B=0.1;if(this.attr_("yRangePad")!==null){b=false;B=this.attr_("yRangePad")/this.plotter_.area.h}if(o.length===0){c.extremeRange=[0,1]}else{var D=Infinity;var A=-Infinity;var t,s;for(var w=0;w<o.length;w++){if(!a.hasOwnProperty(o[w])){continue}t=a[o[w]][0];if(t!==null){D=Math.min(t,D)}s=a[o[w]][1];if(s!==null){A=Math.max(s,A)}}if(G&&!C){if(D>0){D=0}if(A<0){A=0}}if(D==Infinity){D=0}if(A==-Infinity){A=1}x=A-D;if(x===0){if(A!==0){x=Math.abs(A)}else{A=1;x=1}}var h,H;if(C){if(b){h=A+B*x;H=D}else{var E=Math.exp(Math.log(x)*B);h=A*E;H=D/E}}else{h=A+B*x;H=D-B*x;if(b&&!this.attr_("avoidMinZero")){if(H<0&&D>=0){H=0}if(h>0&&A<=0){h=0}}}c.extremeRange=[H,h]}if(c.valueWindow){c.computedValueRange=[c.valueWindow[0],c.valueWindow[1]]}else{if(c.valueRange){var e=g(c.valueRange[0])?c.extremeRange[0]:c.valueRange[0];var d=g(c.valueRange[1])?c.extremeRange[1]:c.valueRange[1];if(!b){if(c.logscale){var E=Math.exp(Math.log(x)*B);e*=E;d/=E}else{x=d-e;e-=x*B;d+=x*B}}c.computedValueRange=[e,d]}else{c.computedValueRange=c.extremeRange}}if(l){c.independentTicks=l;var r=this.optionsViewForAxis_("y"+(y?"2":""));var F=r("ticker");c.ticks=F(c.computedValueRange[0],c.computedValueRange[1],this.height_,r,this);if(!p){p=c}}}if(p===undefined){throw ('Configuration Error: At least one axis has to have the "independentTicks" option activated.')}for(var y=0;y<q;y++){var c=this.axes_[y];if(!c.independentTicks){var r=this.optionsViewForAxis_("y"+(y?"2":""));var F=r("ticker");var m=p.ticks;var n=p.computedValueRange[1]-p.computedValueRange[0];var I=c.computedValueRange[1]-c.computedValueRange[0];var f=[];for(var v=0;v<m.length;v++){var u=(m[v].v-p.computedValueRange[0])/n;var z=c.computedValueRange[0]+u*I;f.push(z)}c.ticks=F(c.computedValueRange[0],c.computedValueRange[1],this.height_,r,this,f)}}};Dygraph.prototype.extractSeries_=function(a,f,c){var g=[];var h=this.attr_("errorBars");var e=this.attr_("customBars");for(var d=0;d<a.length;d++){var l=a[d][0];var m=a[d][f];if(c){if(h||e){for(var b=0;b<m.length;b++){if(m[b]<=0){m=null;break}}}else{if(m<=0){m=null}}}if(m!==null){g.push([l,m])}else{g.push([l,h?[null,null]:e?[null,null,null]:m])}}return g};Dygraph.prototype.rollingAverage=function(l,d){d=Math.min(d,l.length);var b=[];var t=this.attr_("sigma");var G,o,z,x,m,c,F,A;if(this.fractions_){var k=0;var h=0;var e=100;for(z=0;z<l.length;z++){k+=l[z][1][0];h+=l[z][1][1];if(z-d>=0){k-=l[z-d][1][0];h-=l[z-d][1][1]}var C=l[z][0];var w=h?k/h:0;if(this.attr_("errorBars")){if(this.attr_("wilsonInterval")){if(h){var s=w<0?0:w,u=h;var B=t*Math.sqrt(s*(1-s)/u+t*t/(4*u*u));var a=1+t*t/h;G=(s+t*t/(2*h)-B)/a;o=(s+t*t/(2*h)+B)/a;b[z]=[C,[s*e,(s-G)*e,(o-s)*e]]}else{b[z]=[C,[0,0,0]]}}else{A=h?t*Math.sqrt(w*(1-w)/h):1;b[z]=[C,[e*w,e*A,e*A]]}}else{b[z]=[C,e*w]}}}else{if(this.attr_("customBars")){G=0;var D=0;o=0;var g=0;for(z=0;z<l.length;z++){var E=l[z][1];m=E[1];b[z]=[l[z][0],[m,m-E[0],E[2]-m]];if(m!==null&&!isNaN(m)){G+=E[0];D+=m;o+=E[2];g+=1}if(z-d>=0){var r=l[z-d];if(r[1][1]!==null&&!isNaN(r[1][1])){G-=r[1][0];D-=r[1][1];o-=r[1][2];g-=1}}if(g){b[z]=[l[z][0],[1*D/g,1*(D-G)/g,1*(o-D)/g]]}else{b[z]=[l[z][0],[null,null,null]]}}}else{if(!this.attr_("errorBars")){if(d==1){return l}for(z=0;z<l.length;z++){c=0;F=0;for(x=Math.max(0,z-d+1);x<z+1;x++){m=l[x][1];if(m===null||isNaN(m)){continue}F++;c+=l[x][1]}if(F){b[z]=[l[z][0],c/F]}else{b[z]=[l[z][0],null]}}}else{for(z=0;z<l.length;z++){c=0;var f=0;F=0;for(x=Math.max(0,z-d+1);x<z+1;x++){m=l[x][1][0];if(m===null||isNaN(m)){continue}F++;c+=l[x][1][0];f+=Math.pow(l[x][1][1],2)}if(F){A=Math.sqrt(f)/F;b[z]=[l[z][0],[c/F,t*A,t*A]]}else{var q=(d==1)?l[z][1][0]:null;b[z]=[l[z][0],[q,q,q]]}}}}}return b};Dygraph.prototype.detectTypeFromString_=function(b){var a=false;var c=b.indexOf("-");if((c>0&&(b[c-1]!="e"&&b[c-1]!="E"))||b.indexOf("/")>=0||isNaN(parseFloat(b))){a=true}else{if(b.length==8&&b>"19700101"&&b<"20371231"){a=true}}this.setXAxisOptions_(a)};Dygraph.prototype.setXAxisOptions_=function(a){if(a){this.attrs_.xValueParser=Dygraph.dateParser;this.attrs_.axes.x.valueFormatter=Dygraph.dateString_;this.attrs_.axes.x.ticker=Dygraph.dateTicker;this.attrs_.axes.x.axisLabelFormatter=Dygraph.dateAxisFormatter}else{this.attrs_.xValueParser=function(b){return parseFloat(b)};this.attrs_.axes.x.valueFormatter=function(b){return b};this.attrs_.axes.x.ticker=Dygraph.numericLinearTicks;this.attrs_.axes.x.axisLabelFormatter=this.attrs_.axes.x.valueFormatter}};Dygraph.prototype.parseFloat_=function(a,c,b){var e=parseFloat(a);if(!isNaN(e)){return e}if(/^ *$/.test(a)){return null}if(/^ *nan *$/i.test(a)){return NaN}var d="Unable to parse '"+a+"' as a number";if(b!==null&&c!==null){d+=" on line "+(1+c)+" ('"+b+"') of CSV."}this.error(d);return null};Dygraph.prototype.parseCSV_=function(t){var r=[];var s=Dygraph.detectLineDelimiter(t);var a=t.split(s||"\n");var g,k;var p=this.attr_("delimiter");if(a[0].indexOf(p)==-1&&a[0].indexOf("\t")>=0){p="\t"}var b=0;if(!("labels" in this.user_attrs_)){b=1;this.attrs_.labels=a[0].split(p);this.attributes_.reparseSeries()}var o=0;var m;var q=false;var c=this.attr_("labels").length;var f=false;for(var l=b;l<a.length;l++){var e=a[l];o=l;if(e.length===0){continue}if(e[0]=="#"){continue}var d=e.split(p);if(d.length<2){continue}var h=[];if(!q){this.detectTypeFromString_(d[0]);m=this.attr_("xValueParser");q=true}h[0]=m(d[0],this);if(this.fractions_){for(k=1;k<d.length;k++){g=d[k].split("/");if(g.length!=2){this.error('Expected fractional "num/den" values in CSV data but found a value \''+d[k]+"' on line "+(1+l)+" ('"+e+"') which is not of this form.");h[k]=[0,0]}else{h[k]=[this.parseFloat_(g[0],l,e),this.parseFloat_(g[1],l,e)]}}}else{if(this.attr_("errorBars")){if(d.length%2!=1){this.error("Expected alternating (value, stdev.) pairs in CSV data but line "+(1+l)+" has an odd number of values ("+(d.length-1)+"): '"+e+"'")}for(k=1;k<d.length;k+=2){h[(k+1)/2]=[this.parseFloat_(d[k],l,e),this.parseFloat_(d[k+1],l,e)]}}else{if(this.attr_("customBars")){for(k=1;k<d.length;k++){var u=d[k];if(/^ *$/.test(u)){h[k]=[null,null,null]}else{g=u.split(";");if(g.length==3){h[k]=[this.parseFloat_(g[0],l,e),this.parseFloat_(g[1],l,e),this.parseFloat_(g[2],l,e)]}else{this.warn('When using customBars, values must be either blank or "low;center;high" tuples (got "'+u+'" on line '+(1+l))}}}}else{for(k=1;k<d.length;k++){h[k]=this.parseFloat_(d[k],l,e)}}}}if(r.length>0&&h[0]<r[r.length-1][0]){f=true}if(h.length!=c){this.error("Number of columns in line "+l+" ("+h.length+") does not agree with number of labels ("+c+") "+e)}if(l===0&&this.attr_("labels")){var n=true;for(k=0;n&&k<h.length;k++){if(h[k]){n=false}}if(n){this.warn("The dygraphs 'labels' option is set, but the first row of CSV data ('"+e+"') appears to also contain labels. Will drop the CSV labels and use the option labels.");continue}}r.push(h)}if(f){this.warn("CSV is out of order; order it correctly to speed loading.");r.sort(function(j,i){return j[0]-i[0]})}return r};Dygraph.prototype.parseArray_=function(c){if(c.length===0){this.error("Can't plot empty data set");return null}if(c[0].length===0){this.error("Data set cannot contain an empty row");return null}var a;if(this.attr_("labels")===null){this.warn("Using default labels. Set labels explicitly via 'labels' in the options parameter");this.attrs_.labels=["X"];for(a=1;a<c[0].length;a++){this.attrs_.labels.push("Y"+a)}this.attributes_.reparseSeries()}else{var b=this.attr_("labels");if(b.length!=c[0].length){this.error("Mismatch between number of labels ("+b+") and number of columns in array ("+c[0].length+")");return null}}if(Dygraph.isDateLike(c[0][0])){this.attrs_.axes.x.valueFormatter=Dygraph.dateString_;this.attrs_.axes.x.ticker=Dygraph.dateTicker;this.attrs_.axes.x.axisLabelFormatter=Dygraph.dateAxisFormatter;var d=Dygraph.clone(c);for(a=0;a<c.length;a++){if(d[a].length===0){this.error("Row "+(1+a)+" of data is empty");return null}if(d[a][0]===null||typeof(d[a][0].getTime)!="function"||isNaN(d[a][0].getTime())){this.error("x value in row "+(1+a)+" is not a Date");return null}d[a][0]=d[a][0].getTime()}return d}else{this.attrs_.axes.x.valueFormatter=function(e){return e};this.attrs_.axes.x.ticker=Dygraph.numericLinearTicks;this.attrs_.axes.x.axisLabelFormatter=Dygraph.numberAxisLabelFormatter;return c}};Dygraph.prototype.parseDataTable_=function(w){var d=function(i){var j=String.fromCharCode(65+i%26);i=Math.floor(i/26);while(i>0){j=String.fromCharCode(65+(i-1)%26)+j.toLowerCase();i=Math.floor((i-1)/26)}return j};var h=w.getNumberOfColumns();var g=w.getNumberOfRows();var f=w.getColumnType(0);if(f=="date"||f=="datetime"){this.attrs_.xValueParser=Dygraph.dateParser;this.attrs_.axes.x.valueFormatter=Dygraph.dateString_;this.attrs_.axes.x.ticker=Dygraph.dateTicker;this.attrs_.axes.x.axisLabelFormatter=Dygraph.dateAxisFormatter}else{if(f=="number"){this.attrs_.xValueParser=function(i){return parseFloat(i)};this.attrs_.axes.x.valueFormatter=function(i){return i};this.attrs_.axes.x.ticker=Dygraph.numericLinearTicks;this.attrs_.axes.x.axisLabelFormatter=this.attrs_.axes.x.valueFormatter}else{this.error("only 'date', 'datetime' and 'number' types are supported for column 1 of DataTable input (Got '"+f+"')");return null}}var m=[];var t={};var s=false;var q,o;for(q=1;q<h;q++){var b=w.getColumnType(q);if(b=="number"){m.push(q)}else{if(b=="string"&&this.attr_("displayAnnotations")){var r=m[m.length-1];if(!t.hasOwnProperty(r)){t[r]=[q]}else{t[r].push(q)}s=true}else{this.error("Only 'number' is supported as a dependent type with Gviz. 'string' is only supported if displayAnnotations is true")}}}var u=[w.getColumnLabel(0)];for(q=0;q<m.length;q++){u.push(w.getColumnLabel(m[q]));if(this.attr_("errorBars")){q+=1}}this.attrs_.labels=u;h=u.length;var v=[];var l=false;var a=[];for(q=0;q<g;q++){var e=[];if(typeof(w.getValue(q,0))==="undefined"||w.getValue(q,0)===null){this.warn("Ignoring row "+q+" of DataTable because of undefined or null first column.");continue}if(f=="date"||f=="datetime"){e.push(w.getValue(q,0).getTime())}else{e.push(w.getValue(q,0))}if(!this.attr_("errorBars")){for(o=0;o<m.length;o++){var c=m[o];e.push(w.getValue(q,c));if(s&&t.hasOwnProperty(c)&&w.getValue(q,t[c][0])!==null){var p={};p.series=w.getColumnLabel(c);p.xval=e[0];p.shortText=d(a.length);p.text="";for(var n=0;n<t[c].length;n++){if(n){p.text+="\n"}p.text+=w.getValue(q,t[c][n])}a.push(p)}}for(o=0;o<e.length;o++){if(!isFinite(e[o])){e[o]=null}}}else{for(o=0;o<h-1;o++){e.push([w.getValue(q,1+2*o),w.getValue(q,2+2*o)])}}if(v.length>0&&e[0]<v[v.length-1][0]){l=true}v.push(e)}if(l){this.warn("DataTable is out of order; order it correctly to speed loading.");v.sort(function(j,i){return j[0]-i[0]})}this.rawData_=v;if(a.length>0){this.setAnnotations(a,true)}this.attributes_.reparseSeries()};Dygraph.prototype.start_=function(){var d=this.file_;if(typeof d=="function"){d=d()}if(Dygraph.isArrayLike(d)){this.rawData_=this.parseArray_(d);this.predraw_()}else{if(typeof d=="object"&&typeof d.getColumnRange=="function"){this.parseDataTable_(d);this.predraw_()}else{if(typeof d=="string"){var c=Dygraph.detectLineDelimiter(d);if(c){this.loadedEvent_(d)}else{var b;if(window.XMLHttpRequest){b=new XMLHttpRequest()}else{b=new ActiveXObject("Microsoft.XMLHTTP")}var a=this;b.onreadystatechange=function(){if(b.readyState==4){if(b.status===200||b.status===0){a.loadedEvent_(b.responseText)}}};b.open("GET",d,true);b.send(null)}}else{this.error("Unknown data format: "+(typeof d))}}}};Dygraph.prototype.updateOptions=function(e,b){if(typeof(b)=="undefined"){b=false}var d=e.file;var c=Dygraph.mapLegacyOptions_(e);if("rollPeriod" in c){this.rollPeriod_=c.rollPeriod}if("dateWindow" in c){this.dateWindow_=c.dateWindow;if(!("isZoomedIgnoreProgrammaticZoom" in c)){this.zoomed_x_=(c.dateWindow!==null)}}if("valueRange" in c&&!("isZoomedIgnoreProgrammaticZoom" in c)){this.zoomed_y_=(c.valueRange!==null)}var a=Dygraph.isPixelChangingOptionList(this.attr_("labels"),c);Dygraph.updateDeep(this.user_attrs_,c);this.attributes_.reparseSeries();if(d){this.file_=d;if(!b){this.start_()}}else{if(!b){if(a){this.predraw_()}else{this.renderGraph_(false)}}}};Dygraph.mapLegacyOptions_=function(c){var a={};for(var b in c){if(b=="file"){continue}if(c.hasOwnProperty(b)){a[b]=c[b]}}var e=function(g,f,h){if(!a.axes){a.axes={}}if(!a.axes[g]){a.axes[g]={}}a.axes[g][f]=h};var d=function(f,g,h){if(typeof(c[f])!="undefined"){Dygraph.warn("Option "+f+" is deprecated. Use the "+h+" option for the "+g+" axis instead. (e.g. { axes : { "+g+" : { "+h+" : ... } } } (see http://dygraphs.com/per-axis.html for more information.");e(g,h,c[f]);delete a[f]}};d("xValueFormatter","x","valueFormatter");d("pixelsPerXLabel","x","pixelsPerLabel");d("xAxisLabelFormatter","x","axisLabelFormatter");d("xTicker","x","ticker");d("yValueFormatter","y","valueFormatter");d("pixelsPerYLabel","y","pixelsPerLabel");d("yAxisLabelFormatter","y","axisLabelFormatter");d("yTicker","y","ticker");return a};Dygraph.prototype.resize=function(d,b){if(this.resize_lock){return}this.resize_lock=true;if((d===null)!=(b===null)){this.warn("Dygraph.resize() should be called with zero parameters or two non-NULL parameters. Pretending it was zero.");d=b=null}var a=this.width_;var c=this.height_;if(d){this.maindiv_.style.width=d+"px";this.maindiv_.style.height=b+"px";this.width_=d;this.height_=b}else{this.width_=this.maindiv_.clientWidth;this.height_=this.maindiv_.clientHeight}if(a!=this.width_||c!=this.height_){this.resizeElements_();this.predraw_()}this.resize_lock=false};Dygraph.prototype.adjustRoll=function(a){this.rollPeriod_=a;this.predraw_()};Dygraph.prototype.visibility=function(){if(!this.attr_("visibility")){this.attrs_.visibility=[]}while(this.attr_("visibility").length<this.numColumns()-1){this.attrs_.visibility.push(true)}return this.attr_("visibility")};Dygraph.prototype.setVisibility=function(b,c){var a=this.visibility();if(b<0||b>=a.length){this.warn("invalid series number in setVisibility: "+b)}else{a[b]=c;this.predraw_()}};Dygraph.prototype.size=function(){return{width:this.width_,height:this.height_}};Dygraph.prototype.setAnnotations=function(b,a){Dygraph.addAnnotationRule();this.annotations_=b;if(!this.layout_){this.warn("Tried to setAnnotations before dygraph was ready. Try setting them in a ready() block. See dygraphs.com/tests/annotation.html");return}this.layout_.setAnnotations(this.annotations_);if(!a){this.predraw_()}};Dygraph.prototype.annotations=function(){return this.annotations_};Dygraph.prototype.getLabels=function(){var a=this.attr_("labels");return a?a.slice():null};Dygraph.prototype.indexFromSetName=function(a){return this.setIndexByName_[a]};Dygraph.prototype.ready=function(a){if(this.is_initial_draw_){this.readyFns_.push(a)}else{a(this)}};Dygraph.addAnnotationRule=function(){if(Dygraph.addedAnnotationCSS){return}var f="border: 1px solid black; background-color: white; text-align: center;";var e=document.createElement("style");e.type="text/css";document.getElementsByTagName("head")[0].appendChild(e);for(var b=0;b<document.styleSheets.length;b++){if(document.styleSheets[b].disabled){continue}var d=document.styleSheets[b];try{if(d.insertRule){var a=d.cssRules?d.cssRules.length:0;d.insertRule(".dygraphDefaultAnnotation { "+f+" }",a)}else{if(d.addRule){d.addRule(".dygraphDefaultAnnotation",f)}}Dygraph.addedAnnotationCSS=true;return}catch(c){}}this.warn("Unable to add default annotation CSS rule; display may be off.")};var DateGraph=Dygraph;"use strict";Dygraph.LOG_SCALE=10;Dygraph.LN_TEN=Math.log(Dygraph.LOG_SCALE);Dygraph.log10=function(a){return Math.log(a)/Dygraph.LN_TEN};Dygraph.DEBUG=1;Dygraph.INFO=2;Dygraph.WARNING=3;Dygraph.ERROR=3;Dygraph.LOG_STACK_TRACES=false;Dygraph.DOTTED_LINE=[2,2];Dygraph.DASHED_LINE=[7,3];Dygraph.DOT_DASH_LINE=[7,2,2,2];Dygraph.log=function(c,g){var b;if(typeof(printStackTrace)!="undefined"){try{b=printStackTrace({guess:false});while(b[0].indexOf("stacktrace")!=-1){b.splice(0,1)}b.splice(0,2);for(var d=0;d<b.length;d++){b[d]=b[d].replace(/\([^)]*\/(.*)\)/,"@$1").replace(/\@.*\/([^\/]*)/,"@$1").replace("[object Object].","")}var j=b.splice(0,1)[0];g+=" ("+j.replace(/^.*@ ?/,"")+")"}catch(h){}}if(typeof(window.console)!="undefined"){var a=window.console;var f=function(e,k,i){if(k&&typeof(k)=="function"){k.call(e,i)}else{e.log(i)}};switch(c){case Dygraph.DEBUG:f(a,a.debug,"dygraphs: "+g);break;case Dygraph.INFO:f(a,a.info,"dygraphs: "+g);break;case Dygraph.WARNING:f(a,a.warn,"dygraphs: "+g);break;case Dygraph.ERROR:f(a,a.error,"dygraphs: "+g);break}}if(Dygraph.LOG_STACK_TRACES){window.console.log(b.join("\n"))}};Dygraph.info=function(a){Dygraph.log(Dygraph.INFO,a)};Dygraph.prototype.info=Dygraph.info;Dygraph.warn=function(a){Dygraph.log(Dygraph.WARNING,a)};Dygraph.prototype.warn=Dygraph.warn;Dygraph.error=function(a){Dygraph.log(Dygraph.ERROR,a)};Dygraph.prototype.error=Dygraph.error;Dygraph.getContext=function(a){return(a.getContext("2d"))};Dygraph.addEvent=function addEvent(c,b,a){if(c.addEventListener){c.addEventListener(b,a,false)}else{c[b+a]=function(){a(window.event)};c.attachEvent("on"+b,c[b+a])}};Dygraph.prototype.addAndTrackEvent=function(c,b,a){Dygraph.addEvent(c,b,a);this.registeredEvents_.push({elem:c,type:b,fn:a})};Dygraph.removeEvent=function(c,b,a){if(c.removeEventListener){c.removeEventListener(b,a,false)}else{try{c.detachEvent("on"+b,c[b+a])}catch(d){}c[b+a]=null}};Dygraph.prototype.removeTrackedEvents_=function(){if(this.registeredEvents_){for(var a=0;a<this.registeredEvents_.length;a++){var b=this.registeredEvents_[a];Dygraph.removeEvent(b.elem,b.type,b.fn)}}this.registeredEvents_=[]};Dygraph.cancelEvent=function(a){a=a?a:window.event;if(a.stopPropagation){a.stopPropagation()}if(a.preventDefault){a.preventDefault()}a.cancelBubble=true;a.cancel=true;a.returnValue=false;return false};Dygraph.hsvToRGB=function(h,g,k){var c;var d;var l;if(g===0){c=k;d=k;l=k}else{var e=Math.floor(h*6);var j=(h*6)-e;var b=k*(1-g);var a=k*(1-(g*j));var m=k*(1-(g*(1-j)));switch(e){case 1:c=a;d=k;l=b;break;case 2:c=b;d=k;l=m;break;case 3:c=b;d=a;l=k;break;case 4:c=m;d=b;l=k;break;case 5:c=k;d=b;l=a;break;case 6:case 0:c=k;d=m;l=b;break}}c=Math.floor(255*c+0.5);d=Math.floor(255*d+0.5);l=Math.floor(255*l+0.5);return"rgb("+c+","+d+","+l+")"};Dygraph.findPosX=function(c){var d=0;if(c.offsetParent){var a=c;while(1){var b="0";if(window.getComputedStyle){b=window.getComputedStyle(a,null).borderLeft||"0"}d+=parseInt(b,10);d+=a.offsetLeft;if(!a.offsetParent){break}a=a.offsetParent}}else{if(c.x){d+=c.x}}while(c&&c!=document.body){d-=c.scrollLeft;c=c.parentNode}return d};Dygraph.findPosY=function(c){var b=0;if(c.offsetParent){var a=c;while(1){var d="0";if(window.getComputedStyle){d=window.getComputedStyle(a,null).borderTop||"0"}b+=parseInt(d,10);b+=a.offsetTop;if(!a.offsetParent){break}a=a.offsetParent}}else{if(c.y){b+=c.y}}while(c&&c!=document.body){b-=c.scrollTop;c=c.parentNode}return b};Dygraph.pageX=function(c){if(c.pageX){return(!c.pageX||c.pageX<0)?0:c.pageX}else{var d=document.documentElement;var a=document.body;return c.clientX+(d.scrollLeft||a.scrollLeft)-(d.clientLeft||0)}};Dygraph.pageY=function(c){if(c.pageY){return(!c.pageY||c.pageY<0)?0:c.pageY}else{var d=document.documentElement;var a=document.body;return c.clientY+(d.scrollTop||a.scrollTop)-(d.clientTop||0)}};Dygraph.isOK=function(a){return !!a&&!isNaN(a)};Dygraph.isValidPoint=function(b,a){if(!b){return false}if(b.yval===null){return false}if(b.x===null||b.x===undefined){return false}if(b.y===null||b.y===undefined){return false}if(isNaN(b.x)||(!a&&isNaN(b.y))){return false}return true};Dygraph.floatFormat=function(a,b){var c=Math.min(Math.max(1,b||2),21);return(Math.abs(a)<0.001&&a!==0)?a.toExponential(c-1):a.toPrecision(c)};Dygraph.zeropad=function(a){if(a<10){return"0"+a}else{return""+a}};Dygraph.hmsString_=function(a){var c=Dygraph.zeropad;var b=new Date(a);if(b.getSeconds()){return c(b.getHours())+":"+c(b.getMinutes())+":"+c(b.getSeconds())}else{return c(b.getHours())+":"+c(b.getMinutes())}};Dygraph.round_=function(c,b){var a=Math.pow(10,b);return Math.round(c*a)/a};Dygraph.binarySearch=function(a,d,i,e,b){if(e===null||e===undefined||b===null||b===undefined){e=0;b=d.length-1}if(e>b){return -1}if(i===null||i===undefined){i=0}var h=function(j){return j>=0&&j<d.length};var g=parseInt((e+b)/2,10);var c=d[g];var f;if(c==a){return g}else{if(c>a){if(i>0){f=g-1;if(h(f)&&d[f]<a){return g}}return Dygraph.binarySearch(a,d,i,e,g-1)}else{if(c<a){if(i<0){f=g+1;if(h(f)&&d[f]>a){return g}}return Dygraph.binarySearch(a,d,i,g+1,b)}}}return -1};Dygraph.dateParser=function(a){var b;var c;if(a.search("-")==-1||a.search("T")!=-1||a.search("Z")!=-1){c=Dygraph.dateStrToMillis(a);if(c&&!isNaN(c)){return c}}if(a.search("-")!=-1){b=a.replace("-","/","g");while(b.search("-")!=-1){b=b.replace("-","/")}c=Dygraph.dateStrToMillis(b)}else{if(a.length==8){b=a.substr(0,4)+"/"+a.substr(4,2)+"/"+a.substr(6,2);c=Dygraph.dateStrToMillis(b)}else{c=Dygraph.dateStrToMillis(a)}}if(!c||isNaN(c)){Dygraph.error("Couldn't parse "+a+" as a date")}return c};Dygraph.dateStrToMillis=function(a){return new Date(a).getTime()};Dygraph.update=function(b,c){if(typeof(c)!="undefined"&&c!==null){for(var a in c){if(c.hasOwnProperty(a)){b[a]=c[a]}}}return b};Dygraph.updateDeep=function(b,d){function c(e){return(typeof Node==="object"?e instanceof Node:typeof e==="object"&&typeof e.nodeType==="number"&&typeof e.nodeName==="string")}if(typeof(d)!="undefined"&&d!==null){for(var a in d){if(d.hasOwnProperty(a)){if(d[a]===null){b[a]=null}else{if(Dygraph.isArrayLike(d[a])){b[a]=d[a].slice()}else{if(c(d[a])){b[a]=d[a]}else{if(typeof(d[a])=="object"){if(typeof(b[a])!="object"||b[a]===null){b[a]={}}Dygraph.updateDeep(b[a],d[a])}else{b[a]=d[a]}}}}}}}return b};Dygraph.isArrayLike=function(b){var a=typeof(b);if((a!="object"&&!(a=="function"&&typeof(b.item)=="function"))||b===null||typeof(b.length)!="number"||b.nodeType===3){return false}return true};Dygraph.isDateLike=function(a){if(typeof(a)!="object"||a===null||typeof(a.getTime)!="function"){return false}return true};Dygraph.clone=function(c){var b=[];for(var a=0;a<c.length;a++){if(Dygraph.isArrayLike(c[a])){b.push(Dygraph.clone(c[a]))}else{b.push(c[a])}}return b};Dygraph.createCanvas=function(){var a=document.createElement("canvas");var b=(/MSIE/.test(navigator.userAgent)&&!window.opera);if(b&&(typeof(G_vmlCanvasManager)!="undefined")){a=G_vmlCanvasManager.initElement((a))}return a};Dygraph.isAndroid=function(){return(/Android/).test(navigator.userAgent)};Dygraph.Iterator=function(d,c,b,a){c=c||0;b=b||d.length;this.hasNext=true;this.peek=null;this.start_=c;this.array_=d;this.predicate_=a;this.end_=Math.min(d.length,c+b);this.nextIdx_=c-1;this.next()};Dygraph.Iterator.prototype.next=function(){if(!this.hasNext){return null}var c=this.peek;var b=this.nextIdx_+1;var a=false;while(b<this.end_){if(!this.predicate_||this.predicate_(this.array_,b)){this.peek=this.array_[b];a=true;break}b++}this.nextIdx_=b;if(!a){this.hasNext=false;this.peek=null}return c};Dygraph.createIterator=function(d,c,b,a){return new Dygraph.Iterator(d,c,b,a)};Dygraph.requestAnimFrame=(function(){return window.requestAnimationFrame||window.webkitRequestAnimationFrame||window.mozRequestAnimationFrame||window.oRequestAnimationFrame||window.msRequestAnimationFrame||function(a){window.setTimeout(a,1000/60)}})();Dygraph.repeatAndCleanup=function(h,g,f,a){var i=0;var d;var b=new Date().getTime();h(i);if(g==1){a();return}var e=g-1;(function c(){if(i>=g){return}Dygraph.requestAnimFrame.call(window,function(){var l=new Date().getTime();var j=l-b;d=i;i=Math.floor(j/f);var k=i-d;var m=(i+k)>e;if(m||(i>=e)){h(e);a()}else{if(k!==0){h(i)}c()}})})()};Dygraph.isPixelChangingOptionList=function(h,e){var d={annotationClickHandler:true,annotationDblClickHandler:true,annotationMouseOutHandler:true,annotationMouseOverHandler:true,axisLabelColor:true,axisLineColor:true,axisLineWidth:true,clickCallback:true,digitsAfterDecimal:true,drawCallback:true,drawHighlightPointCallback:true,drawPoints:true,drawPointCallback:true,drawXGrid:true,drawYGrid:true,fillAlpha:true,gridLineColor:true,gridLineWidth:true,hideOverlayOnMouseOut:true,highlightCallback:true,highlightCircleSize:true,interactionModel:true,isZoomedIgnoreProgrammaticZoom:true,labelsDiv:true,labelsDivStyles:true,labelsDivWidth:true,labelsKMB:true,labelsKMG2:true,labelsSeparateLines:true,labelsShowZeroValues:true,legend:true,maxNumberWidth:true,panEdgeFraction:true,pixelsPerYLabel:true,pointClickCallback:true,pointSize:true,rangeSelectorPlotFillColor:true,rangeSelectorPlotStrokeColor:true,showLabelsOnHighlight:true,showRoller:true,sigFigs:true,strokeWidth:true,underlayCallback:true,unhighlightCallback:true,xAxisLabelFormatter:true,xTicker:true,xValueFormatter:true,yAxisLabelFormatter:true,yValueFormatter:true,zoomCallback:true};var a=false;var b={};if(h){for(var f=1;f<h.length;f++){b[h[f]]=true}}for(var g in e){if(a){break}if(e.hasOwnProperty(g)){if(b[g]){for(var c in e[g]){if(a){break}if(e[g].hasOwnProperty(c)&&!d[c]){a=true}}}else{if(!d[g]){a=true}}}}return a};Dygraph.compareArrays=function(c,b){if(!Dygraph.isArrayLike(c)||!Dygraph.isArrayLike(b)){return false}if(c.length!==b.length){return false}for(var a=0;a<c.length;a++){if(c[a]!==b[a]){return false}}return true};Dygraph.regularShape_=function(o,c,i,f,e,a,n){a=a||0;n=n||Math.PI*2/c;o.beginPath();var g=a;var d=g;var h=function(){var p=f+(Math.sin(d)*i);var q=e+(-Math.cos(d)*i);return[p,q]};var b=h();var l=b[0];var j=b[1];o.moveTo(l,j);for(var m=0;m<c;m++){d=(m==c-1)?g:(d+n);var k=h();o.lineTo(k[0],k[1])}o.fill();o.stroke()};Dygraph.shapeFunction_=function(b,a,c){return function(j,i,f,e,k,h,d){f.strokeStyle=h;f.fillStyle="white";Dygraph.regularShape_(f,b,d,e,k,a,c)}};Dygraph.Circles={DEFAULT:function(h,f,b,e,d,c,a){b.beginPath();b.fillStyle=c;b.arc(e,d,a,0,2*Math.PI,false);b.fill()},TRIANGLE:Dygraph.shapeFunction_(3),SQUARE:Dygraph.shapeFunction_(4,Math.PI/4),DIAMOND:Dygraph.shapeFunction_(4),PENTAGON:Dygraph.shapeFunction_(5),HEXAGON:Dygraph.shapeFunction_(6),CIRCLE:function(f,e,c,b,h,d,a){c.beginPath();c.strokeStyle=d;c.fillStyle="white";c.arc(b,h,a,0,2*Math.PI,false);c.fill();c.stroke()},STAR:Dygraph.shapeFunction_(5,0,4*Math.PI/5),PLUS:function(f,e,c,b,h,d,a){c.strokeStyle=d;c.beginPath();c.moveTo(b+a,h);c.lineTo(b-a,h);c.closePath();c.stroke();c.beginPath();c.moveTo(b,h+a);c.lineTo(b,h-a);c.closePath();c.stroke()},EX:function(f,e,c,b,h,d,a){c.strokeStyle=d;c.beginPath();c.moveTo(b+a,h+a);c.lineTo(b-a,h-a);c.closePath();c.stroke();c.beginPath();c.moveTo(b+a,h-a);c.lineTo(b-a,h+a);c.closePath();c.stroke()}};Dygraph.IFrameTarp=function(){this.tarps=[]};Dygraph.IFrameTarp.prototype.cover=function(){var f=document.getElementsByTagName("iframe");for(var c=0;c<f.length;c++){var e=f[c];var b=Dygraph.findPosX(e),h=Dygraph.findPosY(e),d=e.offsetWidth,a=e.offsetHeight;var g=document.createElement("div");g.style.position="absolute";g.style.left=b+"px";g.style.top=h+"px";g.style.width=d+"px";g.style.height=a+"px";g.style.zIndex=999;document.body.appendChild(g);this.tarps.push(g)}};Dygraph.IFrameTarp.prototype.uncover=function(){for(var a=0;a<this.tarps.length;a++){this.tarps[a].parentNode.removeChild(this.tarps[a])}this.tarps=[]};Dygraph.detectLineDelimiter=function(c){for(var a=0;a<c.length;a++){var b=c.charAt(a);if(b==="\r"){if(((a+1)<c.length)&&(c.charAt(a+1)==="\n")){return"\r\n"}return b}if(b==="\n"){if(((a+1)<c.length)&&(c.charAt(a+1)==="\r")){return"\n\r"}return b}}return null};Dygraph.isNodeContainedBy=function(b,a){if(a===null||b===null){return false}var c=(b);while(c&&c!==a){c=c.parentNode}return(c===a)};Dygraph.pow=function(a,b){if(b<0){return 1/Math.pow(a,-b)}return Math.pow(a,b)};Dygraph.dateSetters={ms:Date.prototype.setMilliseconds,s:Date.prototype.setSeconds,m:Date.prototype.setMinutes,h:Date.prototype.setHours};Dygraph.setDateSameTZ=function(c,b){var f=c.getTimezoneOffset();for(var a in b){if(!b.hasOwnProperty(a)){continue}var e=Dygraph.dateSetters[a];if(!e){throw"Invalid setter: "+a}e.call(c,b[a]);if(c.getTimezoneOffset()!=f){c.setTime(c.getTime()+(f-c.getTimezoneOffset())*60*1000)}}};"use strict";Dygraph.GVizChart=function(a){this.container=a};Dygraph.GVizChart.prototype.draw=function(b,a){this.container.innerHTML="";if(typeof(this.date_graph)!="undefined"){this.date_graph.destroy()}this.date_graph=new Dygraph(this.container,b,a)};Dygraph.GVizChart.prototype.setSelection=function(b){var a=false;if(b.length){a=b[0].row}this.date_graph.setSelection(a)};Dygraph.GVizChart.prototype.getSelection=function(){var b=[];var d=this.date_graph.getSelection();if(d<0){return b}var a=this.date_graph.layout_.points;for(var c=0;c<a.length;++c){b.push({row:d,column:c+1})}return b};"use strict";Dygraph.Interaction={};Dygraph.Interaction.startPan=function(o,t,c){var r,b;c.isPanning=true;var k=t.xAxisRange();c.dateRange=k[1]-k[0];c.initialLeftmostDate=k[0];c.xUnitsPerPixel=c.dateRange/(t.plotter_.area.w-1);if(t.attr_("panEdgeFraction")){var x=t.width_*t.attr_("panEdgeFraction");var d=t.xAxisExtremes();var j=t.toDomXCoord(d[0])-x;var l=t.toDomXCoord(d[1])+x;var u=t.toDataXCoord(j);var w=t.toDataXCoord(l);c.boundedDates=[u,w];var f=[];var a=t.height_*t.attr_("panEdgeFraction");for(r=0;r<t.axes_.length;r++){b=t.axes_[r];var p=b.extremeRange;var q=t.toDomYCoord(p[0],r)+a;var s=t.toDomYCoord(p[1],r)-a;var n=t.toDataYCoord(q,r);var e=t.toDataYCoord(s,r);f[r]=[n,e]}c.boundedValues=f}c.is2DPan=false;c.axes=[];for(r=0;r<t.axes_.length;r++){b=t.axes_[r];var h={};var m=t.yAxisRange(r);var v=t.attributes_.getForAxis("logscale",r);if(v){h.initialTopValue=Dygraph.log10(m[1]);h.dragValueRange=Dygraph.log10(m[1])-Dygraph.log10(m[0])}else{h.initialTopValue=m[1];h.dragValueRange=m[1]-m[0]}h.unitsPerPixel=h.dragValueRange/(t.plotter_.area.h-1);c.axes.push(h);if(b.valueWindow||b.valueRange){c.is2DPan=true}}};Dygraph.Interaction.movePan=function(b,k,c){c.dragEndX=k.dragGetX_(b,c);c.dragEndY=k.dragGetY_(b,c);var h=c.initialLeftmostDate-(c.dragEndX-c.dragStartX)*c.xUnitsPerPixel;if(c.boundedDates){h=Math.max(h,c.boundedDates[0])}var a=h+c.dateRange;if(c.boundedDates){if(a>c.boundedDates[1]){h=h-(a-c.boundedDates[1]);a=h+c.dateRange}}k.dateWindow_=[h,a];if(c.is2DPan){var d=c.dragEndY-c.dragStartY;for(var j=0;j<k.axes_.length;j++){var e=k.axes_[j];var o=c.axes[j];var p=d*o.unitsPerPixel;var f=c.boundedValues?c.boundedValues[j]:null;var l=o.initialTopValue+p;if(f){l=Math.min(l,f[1])}var n=l-o.dragValueRange;if(f){if(n<f[0]){l=l-(n-f[0]);n=l-o.dragValueRange}}var m=k.attributes_.getForAxis("logscale",j);if(m){e.valueWindow=[Math.pow(Dygraph.LOG_SCALE,n),Math.pow(Dygraph.LOG_SCALE,l)]}else{e.valueWindow=[n,l]}}}k.drawGraph_(false)};Dygraph.Interaction.endPan=function(c,b,a){a.dragEndX=b.dragGetX_(c,a);a.dragEndY=b.dragGetY_(c,a);var e=Math.abs(a.dragEndX-a.dragStartX);var d=Math.abs(a.dragEndY-a.dragStartY);if(e<2&&d<2&&b.lastx_!==undefined&&b.lastx_!=-1){Dygraph.Interaction.treatMouseOpAsClick(b,c,a)}a.isPanning=false;a.is2DPan=false;a.initialLeftmostDate=null;a.dateRange=null;a.valueRange=null;a.boundedDates=null;a.boundedValues=null;a.axes=null};Dygraph.Interaction.startZoom=function(c,b,a){a.isZooming=true;a.zoomMoved=false};Dygraph.Interaction.moveZoom=function(c,b,a){a.zoomMoved=true;a.dragEndX=b.dragGetX_(c,a);a.dragEndY=b.dragGetY_(c,a);var e=Math.abs(a.dragStartX-a.dragEndX);var d=Math.abs(a.dragStartY-a.dragEndY);a.dragDirection=(e<d/2)?Dygraph.VERTICAL:Dygraph.HORIZONTAL;b.drawZoomRect_(a.dragDirection,a.dragStartX,a.dragEndX,a.dragStartY,a.dragEndY,a.prevDragDirection,a.prevEndX,a.prevEndY);a.prevEndX=a.dragEndX;a.prevEndY=a.dragEndY;a.prevDragDirection=a.dragDirection};Dygraph.Interaction.treatMouseOpAsClick=function(f,b,d){var k=f.attr_("clickCallback");var n=f.attr_("pointClickCallback");var j=null;if(n){var l=-1;var m=Number.MAX_VALUE;for(var e=0;e<f.selPoints_.length;e++){var c=f.selPoints_[e];var a=Math.pow(c.canvasx-d.dragEndX,2)+Math.pow(c.canvasy-d.dragEndY,2);if(!isNaN(a)&&(l==-1||a<m)){m=a;l=e}}var h=f.attr_("highlightCircleSize")+2;if(m<=h*h){j=f.selPoints_[l]}}if(j){n(b,j)}if(k){k(b,f.lastx_,f.selPoints_)}};Dygraph.Interaction.endZoom=function(c,i,e){e.isZooming=false;e.dragEndX=i.dragGetX_(c,e);e.dragEndY=i.dragGetY_(c,e);var h=Math.abs(e.dragEndX-e.dragStartX);var d=Math.abs(e.dragEndY-e.dragStartY);if(h<2&&d<2&&i.lastx_!==undefined&&i.lastx_!=-1){Dygraph.Interaction.treatMouseOpAsClick(i,c,e)}var b=i.getArea();if(h>=10&&e.dragDirection==Dygraph.HORIZONTAL){var f=Math.min(e.dragStartX,e.dragEndX),k=Math.max(e.dragStartX,e.dragEndX);f=Math.max(f,b.x);k=Math.min(k,b.x+b.w);if(f<k){i.doZoomX_(f,k)}e.cancelNextDblclick=true}else{if(d>=10&&e.dragDirection==Dygraph.VERTICAL){var j=Math.min(e.dragStartY,e.dragEndY),a=Math.max(e.dragStartY,e.dragEndY);j=Math.max(j,b.y);a=Math.min(a,b.y+b.h);if(j<a){i.doZoomY_(j,a)}e.cancelNextDblclick=true}else{if(e.zoomMoved){i.clearZoomRect_()}}}e.dragStartX=null;e.dragStartY=null};Dygraph.Interaction.startTouch=function(f,e,d){f.preventDefault();if(f.touches.length>1){d.startTimeForDoubleTapMs=null}var h=[];for(var c=0;c<f.touches.length;c++){var b=f.touches[c];h.push({pageX:b.pageX,pageY:b.pageY,dataX:e.toDataXCoord(b.pageX),dataY:e.toDataYCoord(b.pageY)})}d.initialTouches=h;if(h.length==1){d.initialPinchCenter=h[0];d.touchDirections={x:true,y:true}}else{if(h.length>=2){d.initialPinchCenter={pageX:0.5*(h[0].pageX+h[1].pageX),pageY:0.5*(h[0].pageY+h[1].pageY),dataX:0.5*(h[0].dataX+h[1].dataX),dataY:0.5*(h[0].dataY+h[1].dataY)};var a=180/Math.PI*Math.atan2(d.initialPinchCenter.pageY-h[0].pageY,h[0].pageX-d.initialPinchCenter.pageX);a=Math.abs(a);if(a>90){a=90-a}d.touchDirections={x:(a<(90-45/2)),y:(a>45/2)}}}d.initialRange={x:e.xAxisRange(),y:e.yAxisRange()}};Dygraph.Interaction.moveTouch=function(n,q,d){d.startTimeForDoubleTapMs=null;var p,l=[];for(p=0;p<n.touches.length;p++){var k=n.touches[p];l.push({pageX:k.pageX,pageY:k.pageY})}var a=d.initialTouches;var h;var j=d.initialPinchCenter;if(l.length==1){h=l[0]}else{h={pageX:0.5*(l[0].pageX+l[1].pageX),pageY:0.5*(l[0].pageY+l[1].pageY)}}var m={pageX:h.pageX-j.pageX,pageY:h.pageY-j.pageY};var f=d.initialRange.x[1]-d.initialRange.x[0];var o=d.initialRange.y[0]-d.initialRange.y[1];m.dataX=(m.pageX/q.plotter_.area.w)*f;m.dataY=(m.pageY/q.plotter_.area.h)*o;var w,c;if(l.length==1){w=1;c=1}else{if(l.length>=2){var e=(a[1].pageX-j.pageX);w=(l[1].pageX-h.pageX)/e;var v=(a[1].pageY-j.pageY);c=(l[1].pageY-h.pageY)/v}}w=Math.min(8,Math.max(0.125,w));c=Math.min(8,Math.max(0.125,c));var u=false;if(d.touchDirections.x){q.dateWindow_=[j.dataX-m.dataX+(d.initialRange.x[0]-j.dataX)/w,j.dataX-m.dataX+(d.initialRange.x[1]-j.dataX)/w];u=true}if(d.touchDirections.y){for(p=0;p<1;p++){var b=q.axes_[p];var s=q.attributes_.getForAxis("logscale",p);if(s){}else{b.valueWindow=[j.dataY-m.dataY+(d.initialRange.y[0]-j.dataY)/c,j.dataY-m.dataY+(d.initialRange.y[1]-j.dataY)/c];u=true}}}q.drawGraph_(false);if(u&&l.length>1&&q.attr_("zoomCallback")){var r=q.xAxisRange();q.attr_("zoomCallback")(r[0],r[1],q.yAxisRanges())}};Dygraph.Interaction.endTouch=function(e,d,c){if(e.touches.length!==0){Dygraph.Interaction.startTouch(e,d,c)}else{if(e.changedTouches.length==1){var a=new Date().getTime();var b=e.changedTouches[0];if(c.startTimeForDoubleTapMs&&a-c.startTimeForDoubleTapMs<500&&c.doubleTapX&&Math.abs(c.doubleTapX-b.screenX)<50&&c.doubleTapY&&Math.abs(c.doubleTapY-b.screenY)<50){d.resetZoom()}else{c.startTimeForDoubleTapMs=a;c.doubleTapX=b.screenX;c.doubleTapY=b.screenY}}}};Dygraph.Interaction.defaultModel={mousedown:function(c,b,a){if(c.button&&c.button==2){return}a.initializeMouseDown(c,b,a);if(c.altKey||c.shiftKey){Dygraph.startPan(c,b,a)}else{Dygraph.startZoom(c,b,a)}},mousemove:function(c,b,a){if(a.isZooming){Dygraph.moveZoom(c,b,a)}else{if(a.isPanning){Dygraph.movePan(c,b,a)}}},mouseup:function(c,b,a){if(a.isZooming){Dygraph.endZoom(c,b,a)}else{if(a.isPanning){Dygraph.endPan(c,b,a)}}},touchstart:function(c,b,a){Dygraph.Interaction.startTouch(c,b,a)},touchmove:function(c,b,a){Dygraph.Interaction.moveTouch(c,b,a)},touchend:function(c,b,a){Dygraph.Interaction.endTouch(c,b,a)},mouseout:function(c,b,a){if(a.isZooming){a.dragEndX=null;a.dragEndY=null;b.clearZoomRect_()}},dblclick:function(c,b,a){if(a.cancelNextDblclick){a.cancelNextDblclick=false;return}if(c.altKey||c.shiftKey){return}b.resetZoom()}};Dygraph.DEFAULT_ATTRS.interactionModel=Dygraph.Interaction.defaultModel;Dygraph.defaultInteractionModel=Dygraph.Interaction.defaultModel;Dygraph.endZoom=Dygraph.Interaction.endZoom;Dygraph.moveZoom=Dygraph.Interaction.moveZoom;Dygraph.startZoom=Dygraph.Interaction.startZoom;Dygraph.endPan=Dygraph.Interaction.endPan;Dygraph.movePan=Dygraph.Interaction.movePan;Dygraph.startPan=Dygraph.Interaction.startPan;Dygraph.Interaction.nonInteractiveModel_={mousedown:function(c,b,a){a.initializeMouseDown(c,b,a)},mouseup:function(c,b,a){a.dragEndX=b.dragGetX_(c,a);a.dragEndY=b.dragGetY_(c,a);var e=Math.abs(a.dragEndX-a.dragStartX);var d=Math.abs(a.dragEndY-a.dragStartY);if(e<2&&d<2&&b.lastx_!==undefined&&b.lastx_!=-1){Dygraph.Interaction.treatMouseOpAsClick(b,c,a)}}};Dygraph.Interaction.dragIsPanInteractionModel={mousedown:function(c,b,a){a.initializeMouseDown(c,b,a);Dygraph.startPan(c,b,a)},mousemove:function(c,b,a){if(a.isPanning){Dygraph.movePan(c,b,a)}},mouseup:function(c,b,a){if(a.isPanning){Dygraph.endPan(c,b,a)}}};"use strict";Dygraph.TickList=undefined;Dygraph.Ticker=undefined;Dygraph.numericLinearTicks=function(d,c,i,g,f,h){var e=function(a){if(a==="logscale"){return false}return g(a)};return Dygraph.numericTicks(d,c,i,e,f,h)};Dygraph.numericTicks=function(F,E,u,p,d,q){var z=(p("pixelsPerLabel"));var G=[];var C,A,t,y;if(q){for(C=0;C<q.length;C++){G.push({v:q[C]})}}else{if(p("logscale")){y=Math.floor(u/z);var l=Dygraph.binarySearch(F,Dygraph.PREFERRED_LOG_TICK_VALUES,1);var H=Dygraph.binarySearch(E,Dygraph.PREFERRED_LOG_TICK_VALUES,-1);if(l==-1){l=0}if(H==-1){H=Dygraph.PREFERRED_LOG_TICK_VALUES.length-1}var s=null;if(H-l>=y/4){for(var r=H;r>=l;r--){var m=Dygraph.PREFERRED_LOG_TICK_VALUES[r];var k=Math.log(m/F)/Math.log(E/F)*u;var D={v:m};if(s===null){s={tickValue:m,pixel_coord:k}}else{if(Math.abs(k-s.pixel_coord)>=z){s={tickValue:m,pixel_coord:k}}else{D.label=""}}G.push(D)}G.reverse()}}if(G.length===0){var g=p("labelsKMG2");var n,h;if(g){n=[1,2,4,8,16,32,64,128,256];h=16}else{n=[1,2,5,10,20,50,100];h=10}var w=Math.ceil(u/z);var o=Math.abs(E-F)/w;var v=Math.floor(Math.log(o)/Math.log(h));var f=Math.pow(h,v);var I,x,c,e;for(A=0;A<n.length;A++){I=f*n[A];x=Math.floor(F/I)*I;c=Math.ceil(E/I)*I;y=Math.abs(c-x)/I;e=u/y;if(e>z){break}}if(x>c){I*=-1}for(C=0;C<y;C++){t=x+C*I;G.push({v:t})}}}var B=(p("axisLabelFormatter"));for(C=0;C<G.length;C++){if(G[C].label!==undefined){continue}G[C].label=B(G[C].v,0,p,d)}return G};Dygraph.dateTicker=function(e,c,i,g,f,h){var d=Dygraph.pickDateTickGranularity(e,c,i,g);if(d>=0){return Dygraph.getDateAxis(e,c,d,g,f)}else{return[]}};Dygraph.SECONDLY=0;Dygraph.TWO_SECONDLY=1;Dygraph.FIVE_SECONDLY=2;Dygraph.TEN_SECONDLY=3;Dygraph.THIRTY_SECONDLY=4;Dygraph.MINUTELY=5;Dygraph.TWO_MINUTELY=6;Dygraph.FIVE_MINUTELY=7;Dygraph.TEN_MINUTELY=8;Dygraph.THIRTY_MINUTELY=9;Dygraph.HOURLY=10;Dygraph.TWO_HOURLY=11;Dygraph.SIX_HOURLY=12;Dygraph.DAILY=13;Dygraph.WEEKLY=14;Dygraph.MONTHLY=15;Dygraph.QUARTERLY=16;Dygraph.BIANNUAL=17;Dygraph.ANNUAL=18;Dygraph.DECADAL=19;Dygraph.CENTENNIAL=20;Dygraph.NUM_GRANULARITIES=21;Dygraph.SHORT_SPACINGS=[];Dygraph.SHORT_SPACINGS[Dygraph.SECONDLY]=1000*1;Dygraph.SHORT_SPACINGS[Dygraph.TWO_SECONDLY]=1000*2;Dygraph.SHORT_SPACINGS[Dygraph.FIVE_SECONDLY]=1000*5;Dygraph.SHORT_SPACINGS[Dygraph.TEN_SECONDLY]=1000*10;Dygraph.SHORT_SPACINGS[Dygraph.THIRTY_SECONDLY]=1000*30;Dygraph.SHORT_SPACINGS[Dygraph.MINUTELY]=1000*60;Dygraph.SHORT_SPACINGS[Dygraph.TWO_MINUTELY]=1000*60*2;Dygraph.SHORT_SPACINGS[Dygraph.FIVE_MINUTELY]=1000*60*5;Dygraph.SHORT_SPACINGS[Dygraph.TEN_MINUTELY]=1000*60*10;Dygraph.SHORT_SPACINGS[Dygraph.THIRTY_MINUTELY]=1000*60*30;Dygraph.SHORT_SPACINGS[Dygraph.HOURLY]=1000*3600;Dygraph.SHORT_SPACINGS[Dygraph.TWO_HOURLY]=1000*3600*2;Dygraph.SHORT_SPACINGS[Dygraph.SIX_HOURLY]=1000*3600*6;Dygraph.SHORT_SPACINGS[Dygraph.DAILY]=1000*86400;Dygraph.SHORT_SPACINGS[Dygraph.WEEKLY]=1000*604800;Dygraph.LONG_TICK_PLACEMENTS=[];Dygraph.LONG_TICK_PLACEMENTS[Dygraph.MONTHLY]={months:[0,1,2,3,4,5,6,7,8,9,10,11],year_mod:1};Dygraph.LONG_TICK_PLACEMENTS[Dygraph.QUARTERLY]={months:[0,3,6,9],year_mod:1};Dygraph.LONG_TICK_PLACEMENTS[Dygraph.BIANNUAL]={months:[0,6],year_mod:1};Dygraph.LONG_TICK_PLACEMENTS[Dygraph.ANNUAL]={months:[0],year_mod:1};Dygraph.LONG_TICK_PLACEMENTS[Dygraph.DECADAL]={months:[0],year_mod:10};Dygraph.LONG_TICK_PLACEMENTS[Dygraph.CENTENNIAL]={months:[0],year_mod:100};Dygraph.PREFERRED_LOG_TICK_VALUES=function(){var c=[];for(var b=-39;b<=39;b++){var a=Math.pow(10,b);for(var d=1;d<=9;d++){var e=a*d;c.push(e)}}return c}();Dygraph.pickDateTickGranularity=function(d,c,j,h){var g=(h("pixelsPerLabel"));for(var f=0;f<Dygraph.NUM_GRANULARITIES;f++){var e=Dygraph.numDateTicks(d,c,f);if(j/e>=g){return f}}return -1};Dygraph.numDateTicks=function(e,b,f){if(f<Dygraph.MONTHLY){var g=Dygraph.SHORT_SPACINGS[f];return Math.floor(0.5+1*(b-e)/g)}else{var d=Dygraph.LONG_TICK_PLACEMENTS[f];var c=365.2524*24*3600*1000;var a=1*(b-e)/c;return Math.floor(0.5+1*a*d.months.length/d.year_mod)}};Dygraph.getDateAxis=function(p,l,a,n,z){var w=(n("axisLabelFormatter"));var C=[];var m;if(a<Dygraph.MONTHLY){var c=Dygraph.SHORT_SPACINGS[a];var y=c/1000;var A=new Date(p);Dygraph.setDateSameTZ(A,{ms:0});var h;if(y<=60){h=A.getSeconds();Dygraph.setDateSameTZ(A,{s:h-h%y})}else{Dygraph.setDateSameTZ(A,{s:0});y/=60;if(y<=60){h=A.getMinutes();Dygraph.setDateSameTZ(A,{m:h-h%y})}else{Dygraph.setDateSameTZ(A,{m:0});y/=60;if(y<=24){h=A.getHours();A.setHours(h-h%y)}else{A.setHours(0);y/=24;if(y==7){A.setDate(A.getDate()-A.getDay())}}}}p=A.getTime();var B=new Date(p).getTimezoneOffset();var e=(c>=Dygraph.SHORT_SPACINGS[Dygraph.TWO_HOURLY]);for(m=p;m<=l;m+=c){A=new Date(m);if(e&&A.getTimezoneOffset()!=B){var k=A.getTimezoneOffset()-B;m+=k*60*1000;A=new Date(m);B=A.getTimezoneOffset();if(new Date(m+c).getTimezoneOffset()!=B){m+=c;A=new Date(m);B=A.getTimezoneOffset()}}C.push({v:m,label:w(A,a,n,z)})}}else{var f;var r=1;if(a<Dygraph.NUM_GRANULARITIES){f=Dygraph.LONG_TICK_PLACEMENTS[a].months;r=Dygraph.LONG_TICK_PLACEMENTS[a].year_mod}else{Dygraph.warn("Span of dates is too long")}var v=new Date(p).getFullYear();var q=new Date(l).getFullYear();var b=Dygraph.zeropad;for(var u=v;u<=q;u++){if(u%r!==0){continue}for(var s=0;s<f.length;s++){var o=u+"/"+b(1+f[s])+"/01";m=Dygraph.dateStrToMillis(o);if(m<p||m>l){continue}C.push({v:m,label:w(new Date(m),a,n,z)})}}}return C};if(Dygraph&&Dygraph.DEFAULT_ATTRS&&Dygraph.DEFAULT_ATTRS.axes&&Dygraph.DEFAULT_ATTRS.axes["x"]&&Dygraph.DEFAULT_ATTRS.axes["y"]&&Dygraph.DEFAULT_ATTRS.axes["y2"]){Dygraph.DEFAULT_ATTRS.axes["x"]["ticker"]=Dygraph.dateTicker;Dygraph.DEFAULT_ATTRS.axes["y"]["ticker"]=Dygraph.numericTicks;Dygraph.DEFAULT_ATTRS.axes["y2"]["ticker"]=Dygraph.numericTicks}Dygraph.Plugins={};Dygraph.Plugins.Annotations=(function(){var a=function(){this.annotations_=[]};a.prototype.toString=function(){return"Annotations Plugin"};a.prototype.activate=function(b){return{clearChart:this.clearChart,didDrawChart:this.didDrawChart}};a.prototype.detachLabels=function(){for(var c=0;c<this.annotations_.length;c++){var b=this.annotations_[c];if(b.parentNode){b.parentNode.removeChild(b)}this.annotations_[c]=null}this.annotations_=[]};a.prototype.clearChart=function(b){this.detachLabels()};a.prototype.didDrawChart=function(v){var t=v.dygraph;var r=t.layout_.annotated_points;if(!r||r.length===0){return}var h=v.canvas.parentNode;var x={position:"absolute",fontSize:t.getOption("axisLabelFontSize")+"px",zIndex:10,overflow:"hidden"};var b=function(e,g,i){return function(y){var p=i.annotation;if(p.hasOwnProperty(e)){p[e](p,i,t,y)}else{if(t.getOption(g)){t.getOption(g)(p,i,t,y)}}}};var u=v.dygraph.plotter_.area;var q={};for(var s=0;s<r.length;s++){var l=r[s];if(l.canvasx<u.x||l.canvasx>u.x+u.w||l.canvasy<u.y||l.canvasy>u.y+u.h){continue}var w=l.annotation;var n=6;if(w.hasOwnProperty("tickHeight")){n=w.tickHeight}var j=document.createElement("div");for(var A in x){if(x.hasOwnProperty(A)){j.style[A]=x[A]}}if(!w.hasOwnProperty("icon")){j.className="dygraphDefaultAnnotation"}if(w.hasOwnProperty("cssClass")){j.className+=" "+w.cssClass}var m=w.hasOwnProperty("width")?w.width:16;var k=w.hasOwnProperty("height")?w.height:16;if(w.hasOwnProperty("icon")){var z=document.createElement("img");z.src=w.icon;z.width=m;z.height=k;j.appendChild(z)}else{if(l.annotation.hasOwnProperty("shortText")){j.appendChild(document.createTextNode(l.annotation.shortText))}}var c=l.canvasx-m/2;j.style.left=c+"px";var f=0;if(w.attachAtBottom){var d=(u.y+u.h-k-n);if(q[c]){d-=q[c]}else{q[c]=0}q[c]+=(n+k);f=d}else{f=l.canvasy-k-n}j.style.top=f+"px";j.style.width=m+"px";j.style.height=k+"px";j.title=l.annotation.text;j.style.color=t.colorsMap_[l.name];j.style.borderColor=t.colorsMap_[l.name];w.div=j;t.addAndTrackEvent(j,"click",b("clickHandler","annotationClickHandler",l,this));t.addAndTrackEvent(j,"mouseover",b("mouseOverHandler","annotationMouseOverHandler",l,this));t.addAndTrackEvent(j,"mouseout",b("mouseOutHandler","annotationMouseOutHandler",l,this));t.addAndTrackEvent(j,"dblclick",b("dblClickHandler","annotationDblClickHandler",l,this));h.appendChild(j);this.annotations_.push(j);var o=v.drawingContext;o.save();o.strokeStyle=t.colorsMap_[l.name];o.beginPath();if(!w.attachAtBottom){o.moveTo(l.canvasx,l.canvasy);o.lineTo(l.canvasx,l.canvasy-2-n)}else{var d=f+k;o.moveTo(l.canvasx,d);o.lineTo(l.canvasx,d+n)}o.closePath();o.stroke();o.restore()}};a.prototype.destroy=function(){this.detachLabels()};return a})();Dygraph.Plugins.Axes=(function(){var a=function(){this.xlabels_=[];this.ylabels_=[]};a.prototype.toString=function(){return"Axes Plugin"};a.prototype.activate=function(b){return{layout:this.layout,clearChart:this.clearChart,willDrawChart:this.willDrawChart}};a.prototype.layout=function(f){var d=f.dygraph;if(d.getOption("drawYAxis")){var b=d.getOption("yAxisLabelWidth")+2*d.getOption("axisTickSize");f.reserveSpaceLeft(b)}if(d.getOption("drawXAxis")){var c;if(d.getOption("xAxisHeight")){c=d.getOption("xAxisHeight")}else{c=d.getOptionForAxis("axisLabelFontSize","x")+2*d.getOption("axisTickSize")}f.reserveSpaceBottom(c)}if(d.numAxes()==2){if(d.getOption("drawYAxis")){var b=d.getOption("yAxisLabelWidth")+2*d.getOption("axisTickSize");f.reserveSpaceRight(b)}}else{if(d.numAxes()>2){d.error("Only two y-axes are supported at this time. (Trying to use "+d.numAxes()+")")}}};a.prototype.detachLabels=function(){function b(d){for(var c=0;c<d.length;c++){var e=d[c];if(e.parentNode){e.parentNode.removeChild(e)}}}b(this.xlabels_);b(this.ylabels_);this.xlabels_=[];this.ylabels_=[]};a.prototype.clearChart=function(b){this.detachLabels()};a.prototype.willDrawChart=function(H){var F=H.dygraph;if(!F.getOption("drawXAxis")&&!F.getOption("drawYAxis")){return}function B(e){return Math.round(e)+0.5}function A(e){return Math.round(e)-0.5}var j=H.drawingContext;var v=H.canvas.parentNode;var J=H.canvas.width;var d=H.canvas.height;var s,u,t,E,D;var C=function(e){return{position:"absolute",fontSize:F.getOptionForAxis("axisLabelFontSize",e)+"px",zIndex:10,color:F.getOptionForAxis("axisLabelColor",e),width:F.getOption("axisLabelWidth")+"px",lineHeight:"normal",overflow:"hidden"}};var p={x:C("x"),y:C("y"),y2:C("y2")};var m=function(g,x,y){var K=document.createElement("div");var e=p[y=="y2"?"y2":x];for(var r in e){if(e.hasOwnProperty(r)){K.style[r]=e[r]}}var i=document.createElement("div");i.className="dygraph-axis-label dygraph-axis-label-"+x+(y?" dygraph-axis-label-"+y:"");i.innerHTML=g;K.appendChild(i);return K};j.save();var I=F.layout_;var G=H.dygraph.plotter_.area;if(F.getOption("drawYAxis")){if(I.yticks&&I.yticks.length>0){var h=F.numAxes();for(D=0;D<I.yticks.length;D++){E=I.yticks[D];if(typeof(E)=="function"){return}u=G.x;var o=1;var f="y1";if(E[0]==1){u=G.x+G.w;o=-1;f="y2"}var k=F.getOptionForAxis("axisLabelFontSize",f);t=G.y+E[1]*G.h;s=m(E[2],"y",h==2?f:null);var z=(t-k/2);if(z<0){z=0}if(z+k+3>d){s.style.bottom="0px"}else{s.style.top=z+"px"}if(E[0]===0){s.style.left=(G.x-F.getOption("yAxisLabelWidth")-F.getOption("axisTickSize"))+"px";s.style.textAlign="right"}else{if(E[0]==1){s.style.left=(G.x+G.w+F.getOption("axisTickSize"))+"px";s.style.textAlign="left"}}s.style.width=F.getOption("yAxisLabelWidth")+"px";v.appendChild(s);this.ylabels_.push(s)}var n=this.ylabels_[0];var k=F.getOptionForAxis("axisLabelFontSize","y");var q=parseInt(n.style.top,10)+k;if(q>d-k){n.style.top=(parseInt(n.style.top,10)-k/2)+"px"}}var c;if(F.getOption("drawAxesAtZero")){var w=F.toPercentXCoord(0);if(w>1||w<0||isNaN(w)){w=0}c=B(G.x+w*G.w)}else{c=B(G.x)}j.strokeStyle=F.getOptionForAxis("axisLineColor","y");j.lineWidth=F.getOptionForAxis("axisLineWidth","y");j.beginPath();j.moveTo(c,A(G.y));j.lineTo(c,A(G.y+G.h));j.closePath();j.stroke();if(F.numAxes()==2){j.strokeStyle=F.getOptionForAxis("axisLineColor","y2");j.lineWidth=F.getOptionForAxis("axisLineWidth","y2");j.beginPath();j.moveTo(A(G.x+G.w),A(G.y));j.lineTo(A(G.x+G.w),A(G.y+G.h));j.closePath();j.stroke()}}if(F.getOption("drawXAxis")){if(I.xticks){for(D=0;D<I.xticks.length;D++){E=I.xticks[D];u=G.x+E[0]*G.w;t=G.y+G.h;s=m(E[1],"x");s.style.textAlign="center";s.style.top=(t+F.getOption("axisTickSize"))+"px";var l=(u-F.getOption("axisLabelWidth")/2);if(l+F.getOption("axisLabelWidth")>J){l=J-F.getOption("xAxisLabelWidth");s.style.textAlign="right"}if(l<0){l=0;s.style.textAlign="left"}s.style.left=l+"px";s.style.width=F.getOption("xAxisLabelWidth")+"px";v.appendChild(s);this.xlabels_.push(s)}}j.strokeStyle=F.getOptionForAxis("axisLineColor","x");j.lineWidth=F.getOptionForAxis("axisLineWidth","x");j.beginPath();var b;if(F.getOption("drawAxesAtZero")){var w=F.toPercentYCoord(0,0);if(w>1||w<0){w=1}b=A(G.y+w*G.h)}else{b=A(G.y+G.h)}j.moveTo(B(G.x),b);j.lineTo(B(G.x+G.w),b);j.closePath();j.stroke()}j.restore()};return a})();Dygraph.Plugins.ChartLabels=(function(){var c=function(){this.title_div_=null;this.xlabel_div_=null;this.ylabel_div_=null;this.y2label_div_=null};c.prototype.toString=function(){return"ChartLabels Plugin"};c.prototype.activate=function(d){return{layout:this.layout,didDrawChart:this.didDrawChart}};var b=function(d){var e=document.createElement("div");e.style.position="absolute";e.style.left=d.x+"px";e.style.top=d.y+"px";e.style.width=d.w+"px";e.style.height=d.h+"px";return e};c.prototype.detachLabels_=function(){var e=[this.title_div_,this.xlabel_div_,this.ylabel_div_,this.y2label_div_];for(var d=0;d<e.length;d++){var f=e[d];if(!f){continue}if(f.parentNode){f.parentNode.removeChild(f)}}this.title_div_=null;this.xlabel_div_=null;this.ylabel_div_=null;this.y2label_div_=null};var a=function(l,i,f,h,j){var d=document.createElement("div");d.style.position="absolute";if(f==1){d.style.left="0px"}else{d.style.left=i.x+"px"}d.style.top=i.y+"px";d.style.width=i.w+"px";d.style.height=i.h+"px";d.style.fontSize=(l.getOption("yLabelWidth")-2)+"px";var m=document.createElement("div");m.style.position="absolute";m.style.width=i.h+"px";m.style.height=i.w+"px";m.style.top=(i.h/2-i.w/2)+"px";m.style.left=(i.w/2-i.h/2)+"px";m.style.textAlign="center";var e="rotate("+(f==1?"-":"")+"90deg)";m.style.transform=e;m.style.WebkitTransform=e;m.style.MozTransform=e;m.style.OTransform=e;m.style.msTransform=e;if(typeof(document.documentMode)!=="undefined"&&document.documentMode<9){m.style.filter="progid:DXImageTransform.Microsoft.BasicImage(rotation="+(f==1?"3":"1")+")";m.style.left="0px";m.style.top="0px"}var k=document.createElement("div");k.className=h;k.innerHTML=j;m.appendChild(k);d.appendChild(m);return d};c.prototype.layout=function(k){this.detachLabels_();var i=k.dygraph;var m=k.chart_div;if(i.getOption("title")){var d=k.reserveSpaceTop(i.getOption("titleHeight"));this.title_div_=b(d);this.title_div_.style.textAlign="center";this.title_div_.style.fontSize=(i.getOption("titleHeight")-8)+"px";this.title_div_.style.fontWeight="bold";this.title_div_.style.zIndex=10;var f=document.createElement("div");f.className="dygraph-label dygraph-title";f.innerHTML=i.getOption("title");this.title_div_.appendChild(f);m.appendChild(this.title_div_)}if(i.getOption("xlabel")){var j=k.reserveSpaceBottom(i.getOption("xLabelHeight"));this.xlabel_div_=b(j);this.xlabel_div_.style.textAlign="center";this.xlabel_div_.style.fontSize=(i.getOption("xLabelHeight")-2)+"px";var f=document.createElement("div");f.className="dygraph-label dygraph-xlabel";f.innerHTML=i.getOption("xlabel");this.xlabel_div_.appendChild(f);m.appendChild(this.xlabel_div_)}if(i.getOption("ylabel")){var h=k.reserveSpaceLeft(0);this.ylabel_div_=a(i,h,1,"dygraph-label dygraph-ylabel",i.getOption("ylabel"));m.appendChild(this.ylabel_div_)}if(i.getOption("y2label")&&i.numAxes()==2){var l=k.reserveSpaceRight(0);this.y2label_div_=a(i,l,2,"dygraph-label dygraph-y2label",i.getOption("y2label"));m.appendChild(this.y2label_div_)}};c.prototype.didDrawChart=function(f){var d=f.dygraph;if(this.title_div_){this.title_div_.children[0].innerHTML=d.getOption("title")}if(this.xlabel_div_){this.xlabel_div_.children[0].innerHTML=d.getOption("xlabel")}if(this.ylabel_div_){this.ylabel_div_.children[0].children[0].innerHTML=d.getOption("ylabel")}if(this.y2label_div_){this.y2label_div_.children[0].children[0].innerHTML=d.getOption("y2label")}};c.prototype.clearChart=function(){};c.prototype.destroy=function(){this.detachLabels_()};return c})();Dygraph.Plugins.Grid=(function(){var a=function(){};a.prototype.toString=function(){return"Gridline Plugin"};a.prototype.activate=function(b){return{willDrawChart:this.willDrawChart}};a.prototype.willDrawChart=function(s){var q=s.dygraph;var l=s.drawingContext;var t=q.layout_;var r=s.dygraph.plotter_.area;function k(e){return Math.round(e)+0.5}function j(e){return Math.round(e)-0.5}var h,f,p,u;if(q.getOption("drawYGrid")){var o=["y","y2"];var m=[],v=[],b=[],n=[],d=[];for(var p=0;p<o.length;p++){b[p]=q.getOptionForAxis("drawGrid",o[p]);if(b[p]){m[p]=q.getOptionForAxis("gridLineColor",o[p]);v[p]=q.getOptionForAxis("gridLineWidth",o[p]);d[p]=q.getOptionForAxis("gridLinePattern",o[p]);n[p]=d[p]&&(d[p].length>=2)}}u=t.yticks;l.save();for(p=0;p<u.length;p++){var c=u[p][0];if(b[c]){if(n[c]){l.installPattern(d[c])}l.strokeStyle=m[c];l.lineWidth=v[c];h=k(r.x);f=j(r.y+u[p][1]*r.h);l.beginPath();l.moveTo(h,f);l.lineTo(h+r.w,f);l.closePath();l.stroke();if(n[c]){l.uninstallPattern()}}}l.restore()}if(q.getOption("drawXGrid")&&q.getOptionForAxis("drawGrid","x")){u=t.xticks;l.save();var d=q.getOptionForAxis("gridLinePattern","x");var n=d&&(d.length>=2);if(n){l.installPattern(d)}l.strokeStyle=q.getOptionForAxis("gridLineColor","x");l.lineWidth=q.getOptionForAxis("gridLineWidth","x");for(p=0;p<u.length;p++){h=k(r.x+u[p][0]*r.w);f=j(r.y+r.h);l.beginPath();l.moveTo(h,f);l.lineTo(h,r.y);l.closePath();l.stroke()}if(n){l.uninstallPattern()}l.restore()}};a.prototype.destroy=function(){};return a})();Dygraph.Plugins.Legend=(function(){var c=function(){this.legend_div_=null;this.is_generated_div_=false};c.prototype.toString=function(){return"Legend Plugin"};var a,d;c.prototype.activate=function(j){var m;var f=j.getOption("labelsDivWidth");var l=j.getOption("labelsDiv");if(l&&null!==l){if(typeof(l)=="string"||l instanceof String){m=document.getElementById(l)}else{m=l}}else{var i={position:"absolute",fontSize:"14px",zIndex:10,width:f+"px",top:"0px",left:(j.size().width-f-2)+"px",background:"white",lineHeight:"normal",textAlign:"left",overflow:"hidden"};Dygraph.update(i,j.getOption("labelsDivStyles"));m=document.createElement("div");m.className="dygraph-legend";for(var h in i){if(!i.hasOwnProperty(h)){continue}try{m.style[h]=i[h]}catch(k){this.warn("You are using unsupported css properties for your browser in labelsDivStyles")}}j.graphDiv.appendChild(m);this.is_generated_div_=true}this.legend_div_=m;this.one_em_width_=10;return{select:this.select,deselect:this.deselect,predraw:this.predraw,didDrawChart:this.didDrawChart}};var b=function(g){var f=document.createElement("span");f.setAttribute("style","margin: 0; padding: 0 0 0 1em; border: 0;");g.appendChild(f);var e=f.offsetWidth;g.removeChild(f);return e};c.prototype.select=function(i){var h=i.selectedX;var g=i.selectedPoints;var f=a(i.dygraph,h,g,this.one_em_width_);this.legend_div_.innerHTML=f};c.prototype.deselect=function(h){var f=b(this.legend_div_);this.one_em_width_=f;var g=a(h.dygraph,undefined,undefined,f);this.legend_div_.innerHTML=g};c.prototype.didDrawChart=function(f){this.deselect(f)};c.prototype.predraw=function(h){if(!this.is_generated_div_){return}h.dygraph.graphDiv.appendChild(this.legend_div_);var g=h.dygraph.plotter_.area;var f=h.dygraph.getOption("labelsDivWidth");this.legend_div_.style.left=g.x+g.w-f-1+"px";this.legend_div_.style.top=g.y+"px";this.legend_div_.style.width=f+"px"};c.prototype.destroy=function(){this.legend_div_=null};a=function(w,p,l,f){if(w.getOption("showLabelsOnHighlight")!==true){return""}var r,C,u,s,m;var z=w.getLabels();if(typeof(p)==="undefined"){if(w.getOption("legend")!="always"){return""}C=w.getOption("labelsSeparateLines");r="";for(u=1;u<z.length;u++){var q=w.getPropertiesForSeries(z[u]);if(!q.visible){continue}if(r!==""){r+=(C?"<br/>":" ")}m=w.getOption("strokePattern",z[u]);s=d(m,q.color,f);r+="<span style='color: "+q.color+";'>"+s+" "+z[u]+"</span>"}return r}var A=w.optionsViewForAxis_("x");var o=A("valueFormatter");r=o(p,A,z[0],w);if(r!==""){r+=":"}var v=[];var j=w.numAxes();for(u=0;u<j;u++){v[u]=w.optionsViewForAxis_("y"+(u?1+u:""))}var k=w.getOption("labelsShowZeroValues");C=w.getOption("labelsSeparateLines");var B=w.getHighlightSeries();for(u=0;u<l.length;u++){var t=l[u];if(t.yval===0&&!k){continue}if(!Dygraph.isOK(t.canvasy)){continue}if(C){r+="<br/>"}var q=w.getPropertiesForSeries(t.name);var n=v[q.axis-1];var y=n("valueFormatter");var e=y(t.yval,n,t.name,w);var h=(t.name==B)?" class='highlight'":"";r+="<span"+h+"> <span style='color: "+q.color+";'>"+t.name+"</span>:&nbsp;"+e+"</span>"}return r};d=function(s,h,r){var e=(/MSIE/.test(navigator.userAgent)&&!window.opera);if(e){return"&mdash;"}if(!s||s.length<=1){return'<div style="display: inline-block; position: relative; bottom: .5ex; padding-left: 1em; height: 1px; border-bottom: 2px solid '+h+';"></div>'}var l,k,f,o;var g=0,q=0;var p=[];var n;for(l=0;l<=s.length;l++){g+=s[l%s.length]}n=Math.floor(r/(g-s[0]));if(n>1){for(l=0;l<s.length;l++){p[l]=s[l]/r}q=p.length}else{n=1;for(l=0;l<s.length;l++){p[l]=s[l]/g}q=p.length+1}var m="";for(k=0;k<n;k++){for(l=0;l<q;l+=2){f=p[l%p.length];if(l<s.length){o=p[(l+1)%p.length]}else{o=0}m+='<div style="display: inline-block; position: relative; bottom: .5ex; margin-right: '+o+"em; padding-left: "+f+"em; height: 1px; border-bottom: 2px solid "+h+';"></div>'}}return m};return c})();Dygraph.Plugins.RangeSelector=(function(){var a=function(){this.isIE_=/MSIE/.test(navigator.userAgent)&&!window.opera;this.hasTouchInterface_=typeof(TouchEvent)!="undefined";this.isMobileDevice_=/mobile|android/gi.test(navigator.appVersion);this.interfaceCreated_=false};a.prototype.toString=function(){return"RangeSelector Plugin"};a.prototype.activate=function(b){this.dygraph_=b;this.isUsingExcanvas_=b.isUsingExcanvas_;if(this.getOption_("showRangeSelector")){this.createInterface_()}return{layout:this.reserveSpace_,predraw:this.renderStaticLayer_,didDrawChart:this.renderInteractiveLayer_}};a.prototype.destroy=function(){this.bgcanvas_=null;this.fgcanvas_=null;this.leftZoomHandle_=null;this.rightZoomHandle_=null;this.iePanOverlay_=null};a.prototype.getOption_=function(b){return this.dygraph_.getOption(b)};a.prototype.setDefaultOption_=function(b,c){return this.dygraph_.attrs_[b]=c};a.prototype.createInterface_=function(){this.createCanvases_();if(this.isUsingExcanvas_){this.createIEPanOverlay_()}this.createZoomHandles_();this.initInteraction_();if(this.getOption_("animatedZooms")){this.dygraph_.warn("Animated zooms and range selector are not compatible; disabling animatedZooms.");this.dygraph_.updateOptions({animatedZooms:false},true)}this.interfaceCreated_=true;this.addToGraph_()};a.prototype.addToGraph_=function(){var b=this.graphDiv_=this.dygraph_.graphDiv;b.appendChild(this.bgcanvas_);b.appendChild(this.fgcanvas_);b.appendChild(this.leftZoomHandle_);b.appendChild(this.rightZoomHandle_)};a.prototype.removeFromGraph_=function(){var b=this.graphDiv_;b.removeChild(this.bgcanvas_);b.removeChild(this.fgcanvas_);b.removeChild(this.leftZoomHandle_);b.removeChild(this.rightZoomHandle_);this.graphDiv_=null};a.prototype.reserveSpace_=function(b){if(this.getOption_("showRangeSelector")){b.reserveSpaceBottom(this.getOption_("rangeSelectorHeight")+4)}};a.prototype.renderStaticLayer_=function(){if(!this.updateVisibility_()){return}this.resize_();this.drawStaticLayer_()};a.prototype.renderInteractiveLayer_=function(){if(!this.updateVisibility_()||this.isChangingRange_){return}this.placeZoomHandles_();this.drawInteractiveLayer_()};a.prototype.updateVisibility_=function(){var b=this.getOption_("showRangeSelector");if(b){if(!this.interfaceCreated_){this.createInterface_()}else{if(!this.graphDiv_||!this.graphDiv_.parentNode){this.addToGraph_()}}}else{if(this.graphDiv_){this.removeFromGraph_();var c=this.dygraph_;setTimeout(function(){c.width_=0;c.resize()},1)}}return b};a.prototype.resize_=function(){function d(e,f){e.style.top=f.y+"px";e.style.left=f.x+"px";e.width=f.w;e.height=f.h;e.style.width=e.width+"px";e.style.height=e.height+"px"}var c=this.dygraph_.layout_.getPlotArea();var b=0;if(this.getOption_("drawXAxis")){b=this.getOption_("xAxisHeight")||(this.getOption_("axisLabelFontSize")+2*this.getOption_("axisTickSize"))}this.canvasRect_={x:c.x,y:c.y+c.h+b+4,w:c.w,h:this.getOption_("rangeSelectorHeight")};d(this.bgcanvas_,this.canvasRect_);d(this.fgcanvas_,this.canvasRect_)};a.prototype.createCanvases_=function(){this.bgcanvas_=Dygraph.createCanvas();this.bgcanvas_.className="dygraph-rangesel-bgcanvas";this.bgcanvas_.style.position="absolute";this.bgcanvas_.style.zIndex=9;this.bgcanvas_ctx_=Dygraph.getContext(this.bgcanvas_);this.fgcanvas_=Dygraph.createCanvas();this.fgcanvas_.className="dygraph-rangesel-fgcanvas";this.fgcanvas_.style.position="absolute";this.fgcanvas_.style.zIndex=9;this.fgcanvas_.style.cursor="default";this.fgcanvas_ctx_=Dygraph.getContext(this.fgcanvas_)};a.prototype.createIEPanOverlay_=function(){this.iePanOverlay_=document.createElement("div");this.iePanOverlay_.style.position="absolute";this.iePanOverlay_.style.backgroundColor="white";this.iePanOverlay_.style.filter="alpha(opacity=0)";this.iePanOverlay_.style.display="none";this.iePanOverlay_.style.cursor="move";this.fgcanvas_.appendChild(this.iePanOverlay_)};a.prototype.createZoomHandles_=function(){var b=new Image();b.className="dygraph-rangesel-zoomhandle";b.style.position="absolute";b.style.zIndex=10;b.style.visibility="hidden";b.style.cursor="col-resize";if(/MSIE 7/.test(navigator.userAgent)){b.width=7;b.height=14;b.style.backgroundColor="white";b.style.border="1px solid #333333"}else{b.width=9;b.height=16;b.src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAkAAAAQCAYAAADESFVDAAAAAXNSR0IArs4c6QAAAAZiS0dEANAAzwDP4Z7KegAAAAlwSFlzAAAOxAAADsQBlSsOGwAAAAd0SU1FB9sHGw0cMqdt1UwAAAAZdEVYdENvbW1lbnQAQ3JlYXRlZCB3aXRoIEdJTVBXgQ4XAAAAaElEQVQoz+3SsRFAQBCF4Z9WJM8KCDVwownl6YXsTmCUsyKGkZzcl7zkz3YLkypgAnreFmDEpHkIwVOMfpdi9CEEN2nGpFdwD03yEqDtOgCaun7sqSTDH32I1pQA2Pb9sZecAxc5r3IAb21d6878xsAAAAAASUVORK5CYII="}if(this.isMobileDevice_){b.width*=2;b.height*=2}this.leftZoomHandle_=b;this.rightZoomHandle_=b.cloneNode(false)};a.prototype.initInteraction_=function(){var o=this;var i=this.isIE_?document:window;var u=0;var v=null;var s=false;var d=false;var g=!this.isMobileDevice_&&!this.isUsingExcanvas_;var k=new Dygraph.IFrameTarp();var p,f,r,j,w,h,x,t,q,c,l;var e,n,m;p=function(C){var B=o.dygraph_.xAxisExtremes();var z=(B[1]-B[0])/o.canvasRect_.w;var A=B[0]+(C.leftHandlePos-o.canvasRect_.x)*z;var y=B[0]+(C.rightHandlePos-o.canvasRect_.x)*z;return[A,y]};f=function(y){Dygraph.cancelEvent(y);s=true;u=y.clientX;v=y.target?y.target:y.srcElement;if(y.type==="mousedown"||y.type==="dragstart"){Dygraph.addEvent(i,"mousemove",r);Dygraph.addEvent(i,"mouseup",j)}o.fgcanvas_.style.cursor="col-resize";k.cover();return true};r=function(C){if(!s){return false}Dygraph.cancelEvent(C);var z=C.clientX-u;if(Math.abs(z)<4){return true}u=C.clientX;var B=o.getZoomHandleStatus_();var y;if(v==o.leftZoomHandle_){y=B.leftHandlePos+z;y=Math.min(y,B.rightHandlePos-v.width-3);y=Math.max(y,o.canvasRect_.x)}else{y=B.rightHandlePos+z;y=Math.min(y,o.canvasRect_.x+o.canvasRect_.w);y=Math.max(y,B.leftHandlePos+v.width+3)}var A=v.width/2;v.style.left=(y-A)+"px";o.drawInteractiveLayer_();if(g){w()}return true};j=function(y){if(!s){return false}s=false;k.uncover();Dygraph.removeEvent(i,"mousemove",r);Dygraph.removeEvent(i,"mouseup",j);o.fgcanvas_.style.cursor="default";if(!g){w()}return true};w=function(){try{var z=o.getZoomHandleStatus_();o.isChangingRange_=true;if(!z.isZoomed){o.dygraph_.resetZoom()}else{var y=p(z);o.dygraph_.doZoomXDates_(y[0],y[1])}}finally{o.isChangingRange_=false}};h=function(A){if(o.isUsingExcanvas_){return A.srcElement==o.iePanOverlay_}else{var z=o.leftZoomHandle_.getBoundingClientRect();var y=z.left+z.width/2;z=o.rightZoomHandle_.getBoundingClientRect();var B=z.left+z.width/2;return(A.clientX>y&&A.clientX<B)}};x=function(y){if(!d&&h(y)&&o.getZoomHandleStatus_().isZoomed){Dygraph.cancelEvent(y);d=true;u=y.clientX;if(y.type==="mousedown"){Dygraph.addEvent(i,"mousemove",t);Dygraph.addEvent(i,"mouseup",q)}return true}return false};t=function(C){if(!d){return false}Dygraph.cancelEvent(C);var z=C.clientX-u;if(Math.abs(z)<4){return true}u=C.clientX;var B=o.getZoomHandleStatus_();var E=B.leftHandlePos;var y=B.rightHandlePos;var D=y-E;if(E+z<=o.canvasRect_.x){E=o.canvasRect_.x;y=E+D}else{if(y+z>=o.canvasRect_.x+o.canvasRect_.w){y=o.canvasRect_.x+o.canvasRect_.w;E=y-D}else{E+=z;y+=z}}var A=o.leftZoomHandle_.width/2;o.leftZoomHandle_.style.left=(E-A)+"px";o.rightZoomHandle_.style.left=(y-A)+"px";o.drawInteractiveLayer_();if(g){c()}return true};q=function(y){if(!d){return false}d=false;Dygraph.removeEvent(i,"mousemove",t);Dygraph.removeEvent(i,"mouseup",q);if(!g){c()}return true};c=function(){try{o.isChangingRange_=true;o.dygraph_.dateWindow_=p(o.getZoomHandleStatus_());o.dygraph_.drawGraph_(false)}finally{o.isChangingRange_=false}};l=function(y){if(s||d){return}var z=h(y)?"move":"default";if(z!=o.fgcanvas_.style.cursor){o.fgcanvas_.style.cursor=z}};e=function(y){if(y.type=="touchstart"&&y.targetTouches.length==1){if(f(y.targetTouches[0])){Dygraph.cancelEvent(y)}}else{if(y.type=="touchmove"&&y.targetTouches.length==1){if(r(y.targetTouches[0])){Dygraph.cancelEvent(y)}}else{j(y)}}};n=function(y){if(y.type=="touchstart"&&y.targetTouches.length==1){if(x(y.targetTouches[0])){Dygraph.cancelEvent(y)}}else{if(y.type=="touchmove"&&y.targetTouches.length==1){if(t(y.targetTouches[0])){Dygraph.cancelEvent(y)}}else{q(y)}}};m=function(B,A){var z=["touchstart","touchend","touchmove","touchcancel"];for(var y=0;y<z.length;y++){o.dygraph_.addAndTrackEvent(B,z[y],A)}};this.setDefaultOption_("interactionModel",Dygraph.Interaction.dragIsPanInteractionModel);this.setDefaultOption_("panEdgeFraction",0.0001);var b=window.opera?"mousedown":"dragstart";this.dygraph_.addAndTrackEvent(this.leftZoomHandle_,b,f);this.dygraph_.addAndTrackEvent(this.rightZoomHandle_,b,f);if(this.isUsingExcanvas_){this.dygraph_.addAndTrackEvent(this.iePanOverlay_,"mousedown",x)}else{this.dygraph_.addAndTrackEvent(this.fgcanvas_,"mousedown",x);this.dygraph_.addAndTrackEvent(this.fgcanvas_,"mousemove",l)}if(this.hasTouchInterface_){m(this.leftZoomHandle_,e);m(this.rightZoomHandle_,e);m(this.fgcanvas_,n)}};a.prototype.drawStaticLayer_=function(){var b=this.bgcanvas_ctx_;b.clearRect(0,0,this.canvasRect_.w,this.canvasRect_.h);try{this.drawMiniPlot_()}catch(c){Dygraph.warn(c)}var d=0.5;this.bgcanvas_ctx_.lineWidth=1;b.strokeStyle="gray";b.beginPath();b.moveTo(d,d);b.lineTo(d,this.canvasRect_.h-d);b.lineTo(this.canvasRect_.w-d,this.canvasRect_.h-d);b.lineTo(this.canvasRect_.w-d,d);b.stroke()};a.prototype.drawMiniPlot_=function(){var f=this.getOption_("rangeSelectorPlotFillColor");var r=this.getOption_("rangeSelectorPlotStrokeColor");if(!f&&!r){return}var j=this.getOption_("stepPlot");var v=this.computeCombinedSeriesAndLimits_();var q=v.yMax-v.yMin;var p=this.bgcanvas_ctx_;var n=0.5;var e=this.dygraph_.xAxisExtremes();var o=Math.max(e[1]-e[0],1e-30);var g=(this.canvasRect_.w-n)/o;var u=(this.canvasRect_.h-n)/q;var t=this.canvasRect_.w-n;var b=this.canvasRect_.h-n;var d=null,c=null;p.beginPath();p.moveTo(n,b);for(var s=0;s<v.data.length;s++){var h=v.data[s];var l=((h[0]!==null)?((h[0]-e[0])*g):NaN);var k=((h[1]!==null)?(b-(h[1]-v.yMin)*u):NaN);if(isFinite(l)&&isFinite(k)){if(d===null){p.lineTo(l,b)}else{if(j){p.lineTo(l,c)}}p.lineTo(l,k);d=l;c=k}else{if(d!==null){if(j){p.lineTo(l,c);p.lineTo(l,b)}else{p.lineTo(d,b)}}d=c=null}}p.lineTo(t,b);p.closePath();if(f){var m=this.bgcanvas_ctx_.createLinearGradient(0,0,0,b);m.addColorStop(0,"white");m.addColorStop(1,f);this.bgcanvas_ctx_.fillStyle=m;p.fill()}if(r){this.bgcanvas_ctx_.strokeStyle=r;this.bgcanvas_ctx_.lineWidth=1.5;p.stroke()}};a.prototype.computeCombinedSeriesAndLimits_=function(){var v=this.dygraph_.rawData_;var u=this.getOption_("logscale");var q=[];var d;var h;var m;var t,s,r;var e,g;for(t=0;t<v.length;t++){if(v[t].length>1&&v[t][1]!==null){m=typeof v[t][1]!="number";if(m){d=[];h=[];for(r=0;r<v[t][1].length;r++){d.push(0);h.push(0)}}break}}for(t=0;t<v.length;t++){var l=v[t];e=l[0];if(m){for(r=0;r<d.length;r++){d[r]=h[r]=0}}else{d=h=0}for(s=1;s<l.length;s++){if(this.dygraph_.visibility()[s-1]){var n;if(m){for(r=0;r<d.length;r++){n=l[s][r];if(n===null||isNaN(n)){continue}d[r]+=n;h[r]++}}else{n=l[s];if(n===null||isNaN(n)){continue}d+=n;h++}}}if(m){for(r=0;r<d.length;r++){d[r]/=h[r]}g=d.slice(0)}else{g=d/h}q.push([e,g])}q=this.dygraph_.rollingAverage(q,this.dygraph_.rollPeriod_);if(typeof q[0][1]!="number"){for(t=0;t<q.length;t++){g=q[t][1];q[t][1]=g[0]}}var b=Number.MAX_VALUE;var c=-Number.MAX_VALUE;for(t=0;t<q.length;t++){g=q[t][1];if(g!==null&&isFinite(g)&&(!u||g>0)){b=Math.min(b,g);c=Math.max(c,g)}}var o=0.25;if(u){c=Dygraph.log10(c);c+=c*o;b=Dygraph.log10(b);for(t=0;t<q.length;t++){q[t][1]=Dygraph.log10(q[t][1])}}else{var f;var p=c-b;if(p<=Number.MIN_VALUE){f=c*o}else{f=p*o}c+=f;b-=f}return{data:q,yMin:b,yMax:c}};a.prototype.placeZoomHandles_=function(){var h=this.dygraph_.xAxisExtremes();var b=this.dygraph_.xAxisRange();var c=h[1]-h[0];var j=Math.max(0,(b[0]-h[0])/c);var f=Math.max(0,(h[1]-b[1])/c);var i=this.canvasRect_.x+this.canvasRect_.w*j;var e=this.canvasRect_.x+this.canvasRect_.w*(1-f);var d=Math.max(this.canvasRect_.y,this.canvasRect_.y+(this.canvasRect_.h-this.leftZoomHandle_.height)/2);var g=this.leftZoomHandle_.width/2;this.leftZoomHandle_.style.left=(i-g)+"px";this.leftZoomHandle_.style.top=d+"px";this.rightZoomHandle_.style.left=(e-g)+"px";this.rightZoomHandle_.style.top=this.leftZoomHandle_.style.top;this.leftZoomHandle_.style.visibility="visible";this.rightZoomHandle_.style.visibility="visible"};a.prototype.drawInteractiveLayer_=function(){var c=this.fgcanvas_ctx_;c.clearRect(0,0,this.canvasRect_.w,this.canvasRect_.h);var f=1;var e=this.canvasRect_.w-f;var b=this.canvasRect_.h-f;var h=this.getZoomHandleStatus_();c.strokeStyle="black";if(!h.isZoomed){c.beginPath();c.moveTo(f,f);c.lineTo(f,b);c.lineTo(e,b);c.lineTo(e,f);c.stroke();if(this.iePanOverlay_){this.iePanOverlay_.style.display="none"}}else{var g=Math.max(f,h.leftHandlePos-this.canvasRect_.x);var d=Math.min(e,h.rightHandlePos-this.canvasRect_.x);c.fillStyle="rgba(240, 240, 240, 0.6)";c.fillRect(0,0,g,this.canvasRect_.h);c.fillRect(d,0,this.canvasRect_.w-d,this.canvasRect_.h);c.beginPath();c.moveTo(f,f);c.lineTo(g,f);c.lineTo(g,b);c.lineTo(d,b);c.lineTo(d,f);c.lineTo(e,f);c.stroke();if(this.isUsingExcanvas_){this.iePanOverlay_.style.width=(d-g)+"px";this.iePanOverlay_.style.left=g+"px";this.iePanOverlay_.style.height=b+"px";this.iePanOverlay_.style.display="inline"}}};a.prototype.getZoomHandleStatus_=function(){var c=this.leftZoomHandle_.width/2;var d=parseFloat(this.leftZoomHandle_.style.left)+c;var b=parseFloat(this.rightZoomHandle_.style.left)+c;return{leftHandlePos:d,rightHandlePos:b,isZoomed:(d-1>this.canvasRect_.x||b+1<this.canvasRect_.x+this.canvasRect_.w)}};return a})();Dygraph.PLUGINS.push(Dygraph.Plugins.Legend,Dygraph.Plugins.Axes,Dygraph.Plugins.RangeSelector,Dygraph.Plugins.ChartLabels,Dygraph.Plugins.Annotations,Dygraph.Plugins.Grid);

/*global _, Dygraph, window, document */

(function () {
  "use strict";
  window.dygraphConfig = {
    defaultFrame : 20 * 60 * 1000,

    zeropad: function (x) {
      if (x < 10) {
        return "0" + x;
      }
      return x;
    },

    xAxisFormat: function (d) {
      if (d === -1) {
        return "";
      }
      var date = new Date(d);
      return this.zeropad(date.getHours()) + ":"
        + this.zeropad(date.getMinutes()) + ":"
        + this.zeropad(date.getSeconds());
    },

    mergeObjects: function (o1, o2, mergeAttribList) {
      if (!mergeAttribList) {
        mergeAttribList = [];
      }
      var vals = {}, res;
      mergeAttribList.forEach(function (a) {
        var valO1 = o1[a],
        valO2 = o2[a];
        if (valO1 === undefined) {
          valO1 = {};
        }
        if (valO2 === undefined) {
          valO2 = {};
        }
        vals[a] = _.extend(valO1, valO2);
      });
      res = _.extend(o1, o2);
      Object.keys(vals).forEach(function (k) {
        res[k] = vals[k];
      });
      return res;
    },

    mapStatToFigure : {
      residentSize : ["times", "residentSizePercent"],
      pageFaults : ["times", "majorPageFaultsPerSecond", "minorPageFaultsPerSecond"],
      systemUserTime : ["times", "systemTimePerSecond", "userTimePerSecond"],
      totalTime : ["times", "avgQueueTime", "avgRequestTime", "avgIoTime"],
      dataTransfer : ["times", "bytesSentPerSecond", "bytesReceivedPerSecond"],
      requests : ["times", "getsPerSecond", "putsPerSecond", "postsPerSecond",
                  "deletesPerSecond", "patchesPerSecond", "headsPerSecond",
                  "optionsPerSecond", "othersPerSecond"]
    },

    //colors for dygraphs
    colors: ["#617e2b", "#296e9c", "#81ccd8", "#7ca530", "#3c3c3c",
             "#aa90bd", "#e1811d", "#c7d4b2", "#d0b2d4"],


    // figure dependend options
    figureDependedOptions: {
      clusterRequestsPerSecond: {
        showLabelsOnHighlight: true,
        title: '',
        header : "Cluster Requests per Second",
        stackedGraph: true,
        div: "lineGraphLegend",
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }

              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      residentSize: {
        header: "Resident Size",
        axes: {
          y: {
            labelsKMG2: false,
            axisLabelFormatter: function (y) {
              return parseFloat(y.toPrecision(3) * 100) + "%";
            },
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3) * 100) + "%";
            }
          }
        }
      },

      pageFaults: {
        header : "Page Faults",
        visibility: [true, false],
        labels: ["datetime", "Major Page", "Minor Page"],
        div: "pageFaultsChart",
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      systemUserTime: {
        div: "systemUserTimeChart",
        header: "System and User Time",
        labels: ["datetime", "System Time", "User Time"],
        stackedGraph: true,
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      totalTime: {
        div: "totalTimeChart",
        header: "Total Time",
        labels: ["datetime", "Queue", "Computation", "I/O"],
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        },
        stackedGraph: true
      },

      dataTransfer: {
        header: "Data Transfer",
        labels: ["datetime", "Bytes sent", "Bytes received"],
        stackedGraph: true,
        div: "dataTransferChart"
      },

      requests: {
        header: "Requests",
        labels: ["datetime", "GET", "PUT", "POST", "DELETE", "PATCH", "HEAD", "OPTIONS", "OTHER"],
        stackedGraph: true,
        div: "requestsChart",
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      }
    },

    getDashBoardFigures : function (all) {
      var result = [], self = this;
      Object.keys(this.figureDependedOptions).forEach(function (k) {
        // ClusterRequestsPerSecond should not be ignored. Quick Fix
        if (k !== "clusterRequestsPerSecond" && (self.figureDependedOptions[k].div || all)) {
          result.push(k);
        }
      });
      return result;
    },

    //configuration for chart overview
    getDefaultConfig: function (figure) {
      var self = this;
      var result = {
        digitsAfterDecimal: 1,
        drawGapPoints: true,
        fillGraph: true,
        showLabelsOnHighlight: false,
        strokeWidth: 1.0,
        lineWidth: 1.0,
        strokeBorderWidth: 1.0,
        includeZero: true,
        highlightCircleSize: 2.5,
        labelsSeparateLines : true,
        strokeBorderColor: '#ffffff',
        interactionModel: {},
        maxNumberWidth : 10,
        colors: [this.colors[0]],
        xAxisLabelWidth: "50",
        rightGap: 15,
        showRangeSelector: false,
        rangeSelectorHeight: 50,
        rangeSelectorPlotStrokeColor: '#365300',
        rangeSelectorPlotFillColor: '',
        // rangeSelectorPlotFillColor: '#414a4c',
        pixelsPerLabel: 50,
        labelsKMG2: true,
        dateWindow: [
          new Date().getTime() -
            this.defaultFrame,
          new Date().getTime()
        ],
        axes: {
          x: {
            valueFormatter: function (d) {
              return self.xAxisFormat(d);
            }
          },
          y: {
            ticker: Dygraph.numericLinearTicks
          }
        }
      };
      if (this.figureDependedOptions[figure]) {
        result = this.mergeObjects(
          result, this.figureDependedOptions[figure], ["axes"]
        );
        if (result.div && result.labels) {
          result.colors = this.getColors(result.labels);
          result.labelsDiv = document.getElementById(result.div + "Legend");
          result.legend = "always";
          result.showLabelsOnHighlight = true;
        }
      }
      return result;

    },

    getDetailChartConfig: function (figure) {
      var result = _.extend(
        this.getDefaultConfig(figure),
        {
          showRangeSelector: true,
          interactionModel: null,
          showLabelsOnHighlight: true,
          highlightCircleSize: 2.5,
          legend: "always",
          labelsDiv: "div#detailLegend.dashboard-legend-inner"
        }
      );
      if (figure === "pageFaults") {
        result.visibility = [true, true];
      }
      if (!result.labels) {
        result.labels = ["datetime", result.header];
        result.colors = this.getColors(result.labels);
      }
      return result;
    },

    getColors: function (labels) {
      var colorList;
      colorList = this.colors.concat([]);
      return colorList.slice(0, labels.length - 1);
    }
  };
}());

(function(){function t(t){return t.target}function n(t){return t.source}function e(t,n){try{for(var e in n)Object.defineProperty(t.prototype,e,{value:n[e],enumerable:!1})}catch(r){t.prototype=n}}function r(t){for(var n=-1,e=t.length,r=[];e>++n;)r.push(t[n]);return r}function u(t){return Array.prototype.slice.call(t)}function i(){}function a(t){return t}function o(){return!0}function c(t){return"function"==typeof t?t:function(){return t}}function l(t,n,e){return function(){var r=e.apply(n,arguments);return arguments.length?t:r}}function s(t){return null!=t&&!isNaN(t)}function f(t){return t.length}function h(t){return t.trim().replace(/\s+/g," ")}function d(t){for(var n=1;t*n%1;)n*=10;return n}function g(t){return 1===t.length?function(n,e){t(null==n?e:null)}:t}function p(t){return t.responseText}function m(t){return JSON.parse(t.responseText)}function v(t){var n=document.createRange();return n.selectNode(document.body),n.createContextualFragment(t.responseText)}function y(t){return t.responseXML}function M(){}function b(t){function n(){for(var n,r=e,u=-1,i=r.length;i>++u;)(n=r[u].on)&&n.apply(this,arguments);return t}var e=[],r=new i;return n.on=function(n,u){var i,a=r.get(n);return 2>arguments.length?a&&a.on:(a&&(a.on=null,e=e.slice(0,i=e.indexOf(a)).concat(e.slice(i+1)),r.remove(n)),u&&e.push(r.set(n,{on:u})),t)},n}function x(t,n){return n-(t?1+Math.floor(Math.log(t+Math.pow(10,1+Math.floor(Math.log(t)/Math.LN10)-n))/Math.LN10):1)}function _(t){return t+""}function w(t,n){var e=Math.pow(10,3*Math.abs(8-n));return{scale:n>8?function(t){return t/e}:function(t){return t*e},symbol:t}}function S(t){return function(n){return 0>=n?0:n>=1?1:t(n)}}function k(t){return function(n){return 1-t(1-n)}}function E(t){return function(n){return.5*(.5>n?t(2*n):2-t(2-2*n))}}function A(t){return t*t}function N(t){return t*t*t}function T(t){if(0>=t)return 0;if(t>=1)return 1;var n=t*t,e=n*t;return 4*(.5>t?e:3*(t-n)+e-.75)}function q(t){return function(n){return Math.pow(n,t)}}function C(t){return 1-Math.cos(t*Ri/2)}function z(t){return Math.pow(2,10*(t-1))}function D(t){return 1-Math.sqrt(1-t*t)}function L(t,n){var e;return 2>arguments.length&&(n=.45),arguments.length?e=n/(2*Ri)*Math.asin(1/t):(t=1,e=n/4),function(r){return 1+t*Math.pow(2,10*-r)*Math.sin(2*(r-e)*Ri/n)}}function F(t){return t||(t=1.70158),function(n){return n*n*((t+1)*n-t)}}function H(t){return 1/2.75>t?7.5625*t*t:2/2.75>t?7.5625*(t-=1.5/2.75)*t+.75:2.5/2.75>t?7.5625*(t-=2.25/2.75)*t+.9375:7.5625*(t-=2.625/2.75)*t+.984375}function R(){d3.event.stopPropagation(),d3.event.preventDefault()}function P(){for(var t,n=d3.event;t=n.sourceEvent;)n=t;return n}function j(t){for(var n=new M,e=0,r=arguments.length;r>++e;)n[arguments[e]]=b(n);return n.of=function(e,r){return function(u){try{var i=u.sourceEvent=d3.event;u.target=t,d3.event=u,n[u.type].apply(e,r)}finally{d3.event=i}}},n}function O(t){var n=[t.a,t.b],e=[t.c,t.d],r=U(n),u=Y(n,e),i=U(I(e,n,-u))||0;n[0]*e[1]<e[0]*n[1]&&(n[0]*=-1,n[1]*=-1,r*=-1,u*=-1),this.rotate=(r?Math.atan2(n[1],n[0]):Math.atan2(-e[0],e[1]))*Oi,this.translate=[t.e,t.f],this.scale=[r,i],this.skew=i?Math.atan2(u,i)*Oi:0}function Y(t,n){return t[0]*n[0]+t[1]*n[1]}function U(t){var n=Math.sqrt(Y(t,t));return n&&(t[0]/=n,t[1]/=n),n}function I(t,n,e){return t[0]+=e*n[0],t[1]+=e*n[1],t}function V(t){return"transform"==t?d3.interpolateTransform:d3.interpolate}function X(t,n){return n=n-(t=+t)?1/(n-t):0,function(e){return(e-t)*n}}function Z(t,n){return n=n-(t=+t)?1/(n-t):0,function(e){return Math.max(0,Math.min(1,(e-t)*n))}}function B(){}function $(t,n,e){return new J(t,n,e)}function J(t,n,e){this.r=t,this.g=n,this.b=e}function G(t){return 16>t?"0"+Math.max(0,t).toString(16):Math.min(255,t).toString(16)}function K(t,n,e){var r,u,i,a=0,o=0,c=0;if(r=/([a-z]+)\((.*)\)/i.exec(t))switch(u=r[2].split(","),r[1]){case"hsl":return e(parseFloat(u[0]),parseFloat(u[1])/100,parseFloat(u[2])/100);case"rgb":return n(nn(u[0]),nn(u[1]),nn(u[2]))}return(i=aa.get(t))?n(i.r,i.g,i.b):(null!=t&&"#"===t.charAt(0)&&(4===t.length?(a=t.charAt(1),a+=a,o=t.charAt(2),o+=o,c=t.charAt(3),c+=c):7===t.length&&(a=t.substring(1,3),o=t.substring(3,5),c=t.substring(5,7)),a=parseInt(a,16),o=parseInt(o,16),c=parseInt(c,16)),n(a,o,c))}function W(t,n,e){var r,u,i=Math.min(t/=255,n/=255,e/=255),a=Math.max(t,n,e),o=a-i,c=(a+i)/2;return o?(u=.5>c?o/(a+i):o/(2-a-i),r=t==a?(n-e)/o+(e>n?6:0):n==a?(e-t)/o+2:(t-n)/o+4,r*=60):u=r=0,en(r,u,c)}function Q(t,n,e){t=tn(t),n=tn(n),e=tn(e);var r=gn((.4124564*t+.3575761*n+.1804375*e)/sa),u=gn((.2126729*t+.7151522*n+.072175*e)/fa),i=gn((.0193339*t+.119192*n+.9503041*e)/ha);return ln(116*u-16,500*(r-u),200*(u-i))}function tn(t){return.04045>=(t/=255)?t/12.92:Math.pow((t+.055)/1.055,2.4)}function nn(t){var n=parseFloat(t);return"%"===t.charAt(t.length-1)?Math.round(2.55*n):n}function en(t,n,e){return new rn(t,n,e)}function rn(t,n,e){this.h=t,this.s=n,this.l=e}function un(t,n,e){function r(t){return t>360?t-=360:0>t&&(t+=360),60>t?i+(a-i)*t/60:180>t?a:240>t?i+(a-i)*(240-t)/60:i}function u(t){return Math.round(255*r(t))}var i,a;return t%=360,0>t&&(t+=360),n=0>n?0:n>1?1:n,e=0>e?0:e>1?1:e,a=.5>=e?e*(1+n):e+n-e*n,i=2*e-a,$(u(t+120),u(t),u(t-120))}function an(t,n,e){return new on(t,n,e)}function on(t,n,e){this.h=t,this.c=n,this.l=e}function cn(t,n,e){return ln(e,Math.cos(t*=ji)*n,Math.sin(t)*n)}function ln(t,n,e){return new sn(t,n,e)}function sn(t,n,e){this.l=t,this.a=n,this.b=e}function fn(t,n,e){var r=(t+16)/116,u=r+n/500,i=r-e/200;return u=dn(u)*sa,r=dn(r)*fa,i=dn(i)*ha,$(pn(3.2404542*u-1.5371385*r-.4985314*i),pn(-.969266*u+1.8760108*r+.041556*i),pn(.0556434*u-.2040259*r+1.0572252*i))}function hn(t,n,e){return an(180*(Math.atan2(e,n)/Ri),Math.sqrt(n*n+e*e),t)}function dn(t){return t>.206893034?t*t*t:(t-4/29)/7.787037}function gn(t){return t>.008856?Math.pow(t,1/3):7.787037*t+4/29}function pn(t){return Math.round(255*(.00304>=t?12.92*t:1.055*Math.pow(t,1/2.4)-.055))}function mn(t){return Ii(t,Ma),t}function vn(t){return function(){return ga(t,this)}}function yn(t){return function(){return pa(t,this)}}function Mn(t,n){function e(){this.removeAttribute(t)}function r(){this.removeAttributeNS(t.space,t.local)}function u(){this.setAttribute(t,n)}function i(){this.setAttributeNS(t.space,t.local,n)}function a(){var e=n.apply(this,arguments);null==e?this.removeAttribute(t):this.setAttribute(t,e)}function o(){var e=n.apply(this,arguments);null==e?this.removeAttributeNS(t.space,t.local):this.setAttributeNS(t.space,t.local,e)}return t=d3.ns.qualify(t),null==n?t.local?r:e:"function"==typeof n?t.local?o:a:t.local?i:u}function bn(t){return RegExp("(?:^|\\s+)"+d3.requote(t)+"(?:\\s+|$)","g")}function xn(t,n){function e(){for(var e=-1;u>++e;)t[e](this,n)}function r(){for(var e=-1,r=n.apply(this,arguments);u>++e;)t[e](this,r)}t=t.trim().split(/\s+/).map(_n);var u=t.length;return"function"==typeof n?r:e}function _n(t){var n=bn(t);return function(e,r){if(u=e.classList)return r?u.add(t):u.remove(t);var u=e.className,i=null!=u.baseVal,a=i?u.baseVal:u;r?(n.lastIndex=0,n.test(a)||(a=h(a+" "+t),i?u.baseVal=a:e.className=a)):a&&(a=h(a.replace(n," ")),i?u.baseVal=a:e.className=a)}}function wn(t,n,e){function r(){this.style.removeProperty(t)}function u(){this.style.setProperty(t,n,e)}function i(){var r=n.apply(this,arguments);null==r?this.style.removeProperty(t):this.style.setProperty(t,r,e)}return null==n?r:"function"==typeof n?i:u}function Sn(t,n){function e(){delete this[t]}function r(){this[t]=n}function u(){var e=n.apply(this,arguments);null==e?delete this[t]:this[t]=e}return null==n?e:"function"==typeof n?u:r}function kn(t){return{__data__:t}}function En(t){return function(){return ya(this,t)}}function An(t){return arguments.length||(t=d3.ascending),function(n,e){return t(n&&n.__data__,e&&e.__data__)}}function Nn(t,n,e){function r(){var n=this[i];n&&(this.removeEventListener(t,n,n.$),delete this[i])}function u(){function u(t){var e=d3.event;d3.event=t,o[0]=a.__data__;try{n.apply(a,o)}finally{d3.event=e}}var a=this,o=Yi(arguments);r.call(this),this.addEventListener(t,this[i]=u,u.$=e),u._=n}var i="__on"+t,a=t.indexOf(".");return a>0&&(t=t.substring(0,a)),n?u:r}function Tn(t,n){for(var e=0,r=t.length;r>e;e++)for(var u,i=t[e],a=0,o=i.length;o>a;a++)(u=i[a])&&n(u,a,e);return t}function qn(t){return Ii(t,xa),t}function Cn(t,n){return Ii(t,wa),t.id=n,t}function zn(t,n,e,r){var u=t.__transition__||(t.__transition__={active:0,count:0}),a=u[e];if(!a){var o=r.time;return a=u[e]={tween:new i,event:d3.dispatch("start","end"),time:o,ease:r.ease,delay:r.delay,duration:r.duration},++u.count,d3.timer(function(r){function i(r){return u.active>e?l():(u.active=e,h.start.call(t,s,n),a.tween.forEach(function(e,r){(r=r.call(t,s,n))&&p.push(r)}),c(r)||d3.timer(c,0,o),1)}function c(r){if(u.active!==e)return l();for(var i=(r-d)/g,a=f(i),o=p.length;o>0;)p[--o].call(t,a);return i>=1?(l(),h.end.call(t,s,n),1):void 0}function l(){return--u.count?delete u[e]:delete t.__transition__,1}var s=t.__data__,f=a.ease,h=a.event,d=a.delay,g=a.duration,p=[];return r>=d?i(r):d3.timer(i,d,o),1},0,o),a}}function Dn(t){return null==t&&(t=""),function(){this.textContent=t}}function Ln(t,n,e,r){var u=t.id;return Tn(t,"function"==typeof e?function(t,i,a){t.__transition__[u].tween.set(n,r(e.call(t,t.__data__,i,a)))}:(e=r(e),function(t){t.__transition__[u].tween.set(n,e)}))}function Fn(){for(var t,n=Date.now(),e=qa;e;)t=n-e.then,t>=e.delay&&(e.flush=e.callback(t)),e=e.next;var r=Hn()-n;r>24?(isFinite(r)&&(clearTimeout(Aa),Aa=setTimeout(Fn,r)),Ea=0):(Ea=1,Ca(Fn))}function Hn(){for(var t=null,n=qa,e=1/0;n;)n.flush?(delete Ta[n.callback.id],n=t?t.next=n.next:qa=n.next):(e=Math.min(e,n.then+n.delay),n=(t=n).next);return e}function Rn(t,n){var e=t.ownerSVGElement||t;if(e.createSVGPoint){var r=e.createSVGPoint();if(0>za&&(window.scrollX||window.scrollY)){e=d3.select(document.body).append("svg").style("position","absolute").style("top",0).style("left",0);var u=e[0][0].getScreenCTM();za=!(u.f||u.e),e.remove()}return za?(r.x=n.pageX,r.y=n.pageY):(r.x=n.clientX,r.y=n.clientY),r=r.matrixTransform(t.getScreenCTM().inverse()),[r.x,r.y]}var i=t.getBoundingClientRect();return[n.clientX-i.left-t.clientLeft,n.clientY-i.top-t.clientTop]}function Pn(){}function jn(t){var n=t[0],e=t[t.length-1];return e>n?[n,e]:[e,n]}function On(t){return t.rangeExtent?t.rangeExtent():jn(t.range())}function Yn(t,n){var e,r=0,u=t.length-1,i=t[r],a=t[u];return i>a&&(e=r,r=u,u=e,e=i,i=a,a=e),(n=n(a-i))&&(t[r]=n.floor(i),t[u]=n.ceil(a)),t}function Un(){return Math}function In(t,n,e,r){function u(){var u=Math.min(t.length,n.length)>2?Gn:Jn,c=r?Z:X;return a=u(t,n,c,e),o=u(n,t,c,d3.interpolate),i}function i(t){return a(t)}var a,o;return i.invert=function(t){return o(t)},i.domain=function(n){return arguments.length?(t=n.map(Number),u()):t},i.range=function(t){return arguments.length?(n=t,u()):n},i.rangeRound=function(t){return i.range(t).interpolate(d3.interpolateRound)},i.clamp=function(t){return arguments.length?(r=t,u()):r},i.interpolate=function(t){return arguments.length?(e=t,u()):e},i.ticks=function(n){return Bn(t,n)},i.tickFormat=function(n){return $n(t,n)},i.nice=function(){return Yn(t,Xn),u()},i.copy=function(){return In(t,n,e,r)},u()}function Vn(t,n){return d3.rebind(t,n,"range","rangeRound","interpolate","clamp")}function Xn(t){return t=Math.pow(10,Math.round(Math.log(t)/Math.LN10)-1),t&&{floor:function(n){return Math.floor(n/t)*t},ceil:function(n){return Math.ceil(n/t)*t}}}function Zn(t,n){var e=jn(t),r=e[1]-e[0],u=Math.pow(10,Math.floor(Math.log(r/n)/Math.LN10)),i=n/r*u;return.15>=i?u*=10:.35>=i?u*=5:.75>=i&&(u*=2),e[0]=Math.ceil(e[0]/u)*u,e[1]=Math.floor(e[1]/u)*u+.5*u,e[2]=u,e}function Bn(t,n){return d3.range.apply(d3,Zn(t,n))}function $n(t,n){return d3.format(",."+Math.max(0,-Math.floor(Math.log(Zn(t,n)[2])/Math.LN10+.01))+"f")}function Jn(t,n,e,r){var u=e(t[0],t[1]),i=r(n[0],n[1]);return function(t){return i(u(t))}}function Gn(t,n,e,r){var u=[],i=[],a=0,o=Math.min(t.length,n.length)-1;for(t[o]<t[0]&&(t=t.slice().reverse(),n=n.slice().reverse());o>=++a;)u.push(e(t[a-1],t[a])),i.push(r(n[a-1],n[a]));return function(n){var e=d3.bisect(t,n,1,o)-1;return i[e](u[e](n))}}function Kn(t,n){function e(e){return t(n(e))}var r=n.pow;return e.invert=function(n){return r(t.invert(n))},e.domain=function(u){return arguments.length?(n=0>u[0]?Qn:Wn,r=n.pow,t.domain(u.map(n)),e):t.domain().map(r)},e.nice=function(){return t.domain(Yn(t.domain(),Un)),e},e.ticks=function(){var e=jn(t.domain()),u=[];if(e.every(isFinite)){var i=Math.floor(e[0]),a=Math.ceil(e[1]),o=r(e[0]),c=r(e[1]);if(n===Qn)for(u.push(r(i));a>i++;)for(var l=9;l>0;l--)u.push(r(i)*l);else{for(;a>i;i++)for(var l=1;10>l;l++)u.push(r(i)*l);u.push(r(i))}for(i=0;o>u[i];i++);for(a=u.length;u[a-1]>c;a--);u=u.slice(i,a)}return u},e.tickFormat=function(t,u){if(2>arguments.length&&(u=Da),!arguments.length)return u;var i,a=Math.max(.1,t/e.ticks().length),o=n===Qn?(i=-1e-12,Math.floor):(i=1e-12,Math.ceil);return function(t){return a>=t/r(o(n(t)+i))?u(t):""}},e.copy=function(){return Kn(t.copy(),n)},Vn(e,t)}function Wn(t){return Math.log(0>t?0:t)/Math.LN10}function Qn(t){return-Math.log(t>0?0:-t)/Math.LN10}function te(t,n){function e(n){return t(r(n))}var r=ne(n),u=ne(1/n);return e.invert=function(n){return u(t.invert(n))},e.domain=function(n){return arguments.length?(t.domain(n.map(r)),e):t.domain().map(u)},e.ticks=function(t){return Bn(e.domain(),t)},e.tickFormat=function(t){return $n(e.domain(),t)},e.nice=function(){return e.domain(Yn(e.domain(),Xn))},e.exponent=function(t){if(!arguments.length)return n;var i=e.domain();return r=ne(n=t),u=ne(1/n),e.domain(i)},e.copy=function(){return te(t.copy(),n)},Vn(e,t)}function ne(t){return function(n){return 0>n?-Math.pow(-n,t):Math.pow(n,t)}}function ee(t,n){function e(n){return a[((u.get(n)||u.set(n,t.push(n)))-1)%a.length]}function r(n,e){return d3.range(t.length).map(function(t){return n+e*t})}var u,a,o;return e.domain=function(r){if(!arguments.length)return t;t=[],u=new i;for(var a,o=-1,c=r.length;c>++o;)u.has(a=r[o])||u.set(a,t.push(a));return e[n.t].apply(e,n.a)},e.range=function(t){return arguments.length?(a=t,o=0,n={t:"range",a:arguments},e):a},e.rangePoints=function(u,i){2>arguments.length&&(i=0);var c=u[0],l=u[1],s=(l-c)/(Math.max(1,t.length-1)+i);return a=r(2>t.length?(c+l)/2:c+s*i/2,s),o=0,n={t:"rangePoints",a:arguments},e},e.rangeBands=function(u,i,c){2>arguments.length&&(i=0),3>arguments.length&&(c=i);var l=u[1]<u[0],s=u[l-0],f=u[1-l],h=(f-s)/(t.length-i+2*c);return a=r(s+h*c,h),l&&a.reverse(),o=h*(1-i),n={t:"rangeBands",a:arguments},e},e.rangeRoundBands=function(u,i,c){2>arguments.length&&(i=0),3>arguments.length&&(c=i);var l=u[1]<u[0],s=u[l-0],f=u[1-l],h=Math.floor((f-s)/(t.length-i+2*c)),d=f-s-(t.length-i)*h;return a=r(s+Math.round(d/2),h),l&&a.reverse(),o=Math.round(h*(1-i)),n={t:"rangeRoundBands",a:arguments},e},e.rangeBand=function(){return o},e.rangeExtent=function(){return jn(n.a[0])},e.copy=function(){return ee(t,n)},e.domain(t)}function re(t,n){function e(){var e=0,i=n.length;for(u=[];i>++e;)u[e-1]=d3.quantile(t,e/i);return r}function r(t){return isNaN(t=+t)?0/0:n[d3.bisect(u,t)]}var u;return r.domain=function(n){return arguments.length?(t=n.filter(function(t){return!isNaN(t)}).sort(d3.ascending),e()):t},r.range=function(t){return arguments.length?(n=t,e()):n},r.quantiles=function(){return u},r.copy=function(){return re(t,n)},e()}function ue(t,n,e){function r(n){return e[Math.max(0,Math.min(a,Math.floor(i*(n-t))))]}function u(){return i=e.length/(n-t),a=e.length-1,r}var i,a;return r.domain=function(e){return arguments.length?(t=+e[0],n=+e[e.length-1],u()):[t,n]},r.range=function(t){return arguments.length?(e=t,u()):e},r.copy=function(){return ue(t,n,e)},u()}function ie(t,n){function e(e){return n[d3.bisect(t,e)]}return e.domain=function(n){return arguments.length?(t=n,e):t},e.range=function(t){return arguments.length?(n=t,e):n},e.copy=function(){return ie(t,n)},e}function ae(t){function n(t){return+t}return n.invert=n,n.domain=n.range=function(e){return arguments.length?(t=e.map(n),n):t},n.ticks=function(n){return Bn(t,n)},n.tickFormat=function(n){return $n(t,n)},n.copy=function(){return ae(t)},n}function oe(t){return t.innerRadius}function ce(t){return t.outerRadius}function le(t){return t.startAngle}function se(t){return t.endAngle}function fe(t){function n(n){function a(){s.push("M",i(t(f),l))}for(var o,s=[],f=[],h=-1,d=n.length,g=c(e),p=c(r);d>++h;)u.call(this,o=n[h],h)?f.push([+g.call(this,o,h),+p.call(this,o,h)]):f.length&&(a(),f=[]);return f.length&&a(),s.length?s.join(""):null}var e=he,r=de,u=o,i=ge,a=i.key,l=.7;return n.x=function(t){return arguments.length?(e=t,n):e},n.y=function(t){return arguments.length?(r=t,n):r},n.defined=function(t){return arguments.length?(u=t,n):u},n.interpolate=function(t){return arguments.length?(a="function"==typeof t?i=t:(i=Oa.get(t)||ge).key,n):a},n.tension=function(t){return arguments.length?(l=t,n):l},n}function he(t){return t[0]}function de(t){return t[1]}function ge(t){return t.join("L")}function pe(t){return ge(t)+"Z"}function me(t){for(var n=0,e=t.length,r=t[0],u=[r[0],",",r[1]];e>++n;)u.push("V",(r=t[n])[1],"H",r[0]);return u.join("")}function ve(t){for(var n=0,e=t.length,r=t[0],u=[r[0],",",r[1]];e>++n;)u.push("H",(r=t[n])[0],"V",r[1]);return u.join("")}function ye(t,n){return 4>t.length?ge(t):t[1]+xe(t.slice(1,t.length-1),_e(t,n))}function Me(t,n){return 3>t.length?ge(t):t[0]+xe((t.push(t[0]),t),_e([t[t.length-2]].concat(t,[t[1]]),n))}function be(t,n){return 3>t.length?ge(t):t[0]+xe(t,_e(t,n))}function xe(t,n){if(1>n.length||t.length!=n.length&&t.length!=n.length+2)return ge(t);var e=t.length!=n.length,r="",u=t[0],i=t[1],a=n[0],o=a,c=1;if(e&&(r+="Q"+(i[0]-2*a[0]/3)+","+(i[1]-2*a[1]/3)+","+i[0]+","+i[1],u=t[1],c=2),n.length>1){o=n[1],i=t[c],c++,r+="C"+(u[0]+a[0])+","+(u[1]+a[1])+","+(i[0]-o[0])+","+(i[1]-o[1])+","+i[0]+","+i[1];for(var l=2;n.length>l;l++,c++)i=t[c],o=n[l],r+="S"+(i[0]-o[0])+","+(i[1]-o[1])+","+i[0]+","+i[1]}if(e){var s=t[c];r+="Q"+(i[0]+2*o[0]/3)+","+(i[1]+2*o[1]/3)+","+s[0]+","+s[1]}return r}function _e(t,n){for(var e,r=[],u=(1-n)/2,i=t[0],a=t[1],o=1,c=t.length;c>++o;)e=i,i=a,a=t[o],r.push([u*(a[0]-e[0]),u*(a[1]-e[1])]);return r}function we(t){if(3>t.length)return ge(t);var n=1,e=t.length,r=t[0],u=r[0],i=r[1],a=[u,u,u,(r=t[1])[0]],o=[i,i,i,r[1]],c=[u,",",i];for(Ne(c,a,o);e>++n;)r=t[n],a.shift(),a.push(r[0]),o.shift(),o.push(r[1]),Ne(c,a,o);for(n=-1;2>++n;)a.shift(),a.push(r[0]),o.shift(),o.push(r[1]),Ne(c,a,o);return c.join("")}function Se(t){if(4>t.length)return ge(t);for(var n,e=[],r=-1,u=t.length,i=[0],a=[0];3>++r;)n=t[r],i.push(n[0]),a.push(n[1]);for(e.push(Ae(Ia,i)+","+Ae(Ia,a)),--r;u>++r;)n=t[r],i.shift(),i.push(n[0]),a.shift(),a.push(n[1]),Ne(e,i,a);return e.join("")}function ke(t){for(var n,e,r=-1,u=t.length,i=u+4,a=[],o=[];4>++r;)e=t[r%u],a.push(e[0]),o.push(e[1]);for(n=[Ae(Ia,a),",",Ae(Ia,o)],--r;i>++r;)e=t[r%u],a.shift(),a.push(e[0]),o.shift(),o.push(e[1]),Ne(n,a,o);return n.join("")}function Ee(t,n){var e=t.length-1;if(e)for(var r,u,i=t[0][0],a=t[0][1],o=t[e][0]-i,c=t[e][1]-a,l=-1;e>=++l;)r=t[l],u=l/e,r[0]=n*r[0]+(1-n)*(i+u*o),r[1]=n*r[1]+(1-n)*(a+u*c);return we(t)}function Ae(t,n){return t[0]*n[0]+t[1]*n[1]+t[2]*n[2]+t[3]*n[3]}function Ne(t,n,e){t.push("C",Ae(Ya,n),",",Ae(Ya,e),",",Ae(Ua,n),",",Ae(Ua,e),",",Ae(Ia,n),",",Ae(Ia,e))}function Te(t,n){return(n[1]-t[1])/(n[0]-t[0])}function qe(t){for(var n=0,e=t.length-1,r=[],u=t[0],i=t[1],a=r[0]=Te(u,i);e>++n;)r[n]=(a+(a=Te(u=i,i=t[n+1])))/2;return r[n]=a,r}function Ce(t){for(var n,e,r,u,i=[],a=qe(t),o=-1,c=t.length-1;c>++o;)n=Te(t[o],t[o+1]),1e-6>Math.abs(n)?a[o]=a[o+1]=0:(e=a[o]/n,r=a[o+1]/n,u=e*e+r*r,u>9&&(u=3*n/Math.sqrt(u),a[o]=u*e,a[o+1]=u*r));for(o=-1;c>=++o;)u=(t[Math.min(c,o+1)][0]-t[Math.max(0,o-1)][0])/(6*(1+a[o]*a[o])),i.push([u||0,a[o]*u||0]);return i}function ze(t){return 3>t.length?ge(t):t[0]+xe(t,Ce(t))}function De(t){for(var n,e,r,u=-1,i=t.length;i>++u;)n=t[u],e=n[0],r=n[1]+Pa,n[0]=e*Math.cos(r),n[1]=e*Math.sin(r);return t}function Le(t){function n(n){function o(){m.push("M",l(t(y),d),h,f(t(v.reverse()),d),"Z")}for(var s,g,p,m=[],v=[],y=[],M=-1,b=n.length,x=c(e),_=c(u),w=e===r?function(){return g}:c(r),S=u===i?function(){return p}:c(i);b>++M;)a.call(this,s=n[M],M)?(v.push([g=+x.call(this,s,M),p=+_.call(this,s,M)]),y.push([+w.call(this,s,M),+S.call(this,s,M)])):v.length&&(o(),v=[],y=[]);return v.length&&o(),m.length?m.join(""):null}var e=he,r=he,u=0,i=de,a=o,l=ge,s=l.key,f=l,h="L",d=.7;return n.x=function(t){return arguments.length?(e=r=t,n):r},n.x0=function(t){return arguments.length?(e=t,n):e},n.x1=function(t){return arguments.length?(r=t,n):r},n.y=function(t){return arguments.length?(u=i=t,n):i},n.y0=function(t){return arguments.length?(u=t,n):u},n.y1=function(t){return arguments.length?(i=t,n):i},n.defined=function(t){return arguments.length?(a=t,n):a},n.interpolate=function(t){return arguments.length?(s="function"==typeof t?l=t:(l=Oa.get(t)||ge).key,f=l.reverse||l,h=l.closed?"M":"L",n):s},n.tension=function(t){return arguments.length?(d=t,n):d},n}function Fe(t){return t.radius}function He(t){return[t.x,t.y]}function Re(t){return function(){var n=t.apply(this,arguments),e=n[0],r=n[1]+Pa;return[e*Math.cos(r),e*Math.sin(r)]}}function Pe(){return 64}function je(){return"circle"}function Oe(t){var n=Math.sqrt(t/Ri);return"M0,"+n+"A"+n+","+n+" 0 1,1 0,"+-n+"A"+n+","+n+" 0 1,1 0,"+n+"Z"}function Ye(t,n){t.attr("transform",function(t){return"translate("+n(t)+",0)"})}function Ue(t,n){t.attr("transform",function(t){return"translate(0,"+n(t)+")"})}function Ie(t,n,e){if(r=[],e&&n.length>1){for(var r,u,i,a=jn(t.domain()),o=-1,c=n.length,l=(n[1]-n[0])/++e;c>++o;)for(u=e;--u>0;)(i=+n[o]-u*l)>=a[0]&&r.push(i);for(--o,u=0;e>++u&&(i=+n[o]+u*l)<a[1];)r.push(i)}return r}function Ve(){Ja||(Ja=d3.select("body").append("div").style("visibility","hidden").style("top",0).style("height",0).style("width",0).style("overflow-y","scroll").append("div").style("height","2000px").node().parentNode);var t,n=d3.event;try{Ja.scrollTop=1e3,Ja.dispatchEvent(n),t=1e3-Ja.scrollTop}catch(e){t=n.wheelDelta||5*-n.detail}return t}function Xe(t){for(var n=t.source,e=t.target,r=Be(n,e),u=[n];n!==r;)n=n.parent,u.push(n);for(var i=u.length;e!==r;)u.splice(i,0,e),e=e.parent;return u}function Ze(t){for(var n=[],e=t.parent;null!=e;)n.push(t),t=e,e=e.parent;return n.push(t),n}function Be(t,n){if(t===n)return t;for(var e=Ze(t),r=Ze(n),u=e.pop(),i=r.pop(),a=null;u===i;)a=u,u=e.pop(),i=r.pop();return a}function $e(t){t.fixed|=2}function Je(t){t.fixed&=1}function Ge(t){t.fixed|=4,t.px=t.x,t.py=t.y}function Ke(t){t.fixed&=3}function We(t,n,e){var r=0,u=0;if(t.charge=0,!t.leaf)for(var i,a=t.nodes,o=a.length,c=-1;o>++c;)i=a[c],null!=i&&(We(i,n,e),t.charge+=i.charge,r+=i.charge*i.cx,u+=i.charge*i.cy);if(t.point){t.leaf||(t.point.x+=Math.random()-.5,t.point.y+=Math.random()-.5);var l=n*e[t.point.index];t.charge+=t.pointCharge=l,r+=l*t.point.x,u+=l*t.point.y}t.cx=r/t.charge,t.cy=u/t.charge}function Qe(){return 20}function tr(){return 1}function nr(t){return t.x}function er(t){return t.y}function rr(t,n,e){t.y0=n,t.y=e}function ur(t){return d3.range(t.length)}function ir(t){for(var n=-1,e=t[0].length,r=[];e>++n;)r[n]=0;return r}function ar(t){for(var n,e=1,r=0,u=t[0][1],i=t.length;i>e;++e)(n=t[e][1])>u&&(r=e,u=n);return r}function or(t){return t.reduce(cr,0)}function cr(t,n){return t+n[1]}function lr(t,n){return sr(t,Math.ceil(Math.log(n.length)/Math.LN2+1))}function sr(t,n){for(var e=-1,r=+t[0],u=(t[1]-r)/n,i=[];n>=++e;)i[e]=u*e+r;return i}function fr(t){return[d3.min(t),d3.max(t)]}function hr(t,n){return d3.rebind(t,n,"sort","children","value"),t.nodes=t,t.links=mr,t}function dr(t){return t.children}function gr(t){return t.value}function pr(t,n){return n.value-t.value}function mr(t){return d3.merge(t.map(function(t){return(t.children||[]).map(function(n){return{source:t,target:n}})}))}function vr(t,n){return t.value-n.value}function yr(t,n){var e=t._pack_next;t._pack_next=n,n._pack_prev=t,n._pack_next=e,e._pack_prev=n}function Mr(t,n){t._pack_next=n,n._pack_prev=t}function br(t,n){var e=n.x-t.x,r=n.y-t.y,u=t.r+n.r;return u*u-e*e-r*r>.001}function xr(t){function n(t){s=Math.min(t.x-t.r,s),f=Math.max(t.x+t.r,f),h=Math.min(t.y-t.r,h),d=Math.max(t.y+t.r,d)}if((e=t.children)&&(l=e.length)){var e,r,u,i,a,o,c,l,s=1/0,f=-1/0,h=1/0,d=-1/0;if(e.forEach(_r),r=e[0],r.x=-r.r,r.y=0,n(r),l>1&&(u=e[1],u.x=u.r,u.y=0,n(u),l>2))for(i=e[2],kr(r,u,i),n(i),yr(r,i),r._pack_prev=i,yr(i,u),u=r._pack_next,a=3;l>a;a++){kr(r,u,i=e[a]);var g=0,p=1,m=1;for(o=u._pack_next;o!==u;o=o._pack_next,p++)if(br(o,i)){g=1;break}if(1==g)for(c=r._pack_prev;c!==o._pack_prev&&!br(c,i);c=c._pack_prev,m++);g?(m>p||p==m&&u.r<r.r?Mr(r,u=o):Mr(r=c,u),a--):(yr(r,i),u=i,n(i))}var v=(s+f)/2,y=(h+d)/2,M=0;for(a=0;l>a;a++)i=e[a],i.x-=v,i.y-=y,M=Math.max(M,i.r+Math.sqrt(i.x*i.x+i.y*i.y));t.r=M,e.forEach(wr)}}function _r(t){t._pack_next=t._pack_prev=t}function wr(t){delete t._pack_next,delete t._pack_prev}function Sr(t,n,e,r){var u=t.children;if(t.x=n+=r*t.x,t.y=e+=r*t.y,t.r*=r,u)for(var i=-1,a=u.length;a>++i;)Sr(u[i],n,e,r)}function kr(t,n,e){var r=t.r+e.r,u=n.x-t.x,i=n.y-t.y;if(r&&(u||i)){var a=n.r+e.r,o=u*u+i*i;a*=a,r*=r;var c=.5+(r-a)/(2*o),l=Math.sqrt(Math.max(0,2*a*(r+o)-(r-=o)*r-a*a))/(2*o);e.x=t.x+c*u+l*i,e.y=t.y+c*i-l*u}else e.x=t.x+r,e.y=t.y}function Er(t){return 1+d3.max(t,function(t){return t.y})}function Ar(t){return t.reduce(function(t,n){return t+n.x},0)/t.length}function Nr(t){var n=t.children;return n&&n.length?Nr(n[0]):t}function Tr(t){var n,e=t.children;return e&&(n=e.length)?Tr(e[n-1]):t}function qr(t,n){return t.parent==n.parent?1:2}function Cr(t){var n=t.children;return n&&n.length?n[0]:t._tree.thread}function zr(t){var n,e=t.children;return e&&(n=e.length)?e[n-1]:t._tree.thread}function Dr(t,n){var e=t.children;if(e&&(u=e.length))for(var r,u,i=-1;u>++i;)n(r=Dr(e[i],n),t)>0&&(t=r);return t}function Lr(t,n){return t.x-n.x}function Fr(t,n){return n.x-t.x}function Hr(t,n){return t.depth-n.depth}function Rr(t,n){function e(t,r){var u=t.children;if(u&&(a=u.length))for(var i,a,o=null,c=-1;a>++c;)i=u[c],e(i,o),o=i;n(t,r)}e(t,null)}function Pr(t){for(var n,e=0,r=0,u=t.children,i=u.length;--i>=0;)n=u[i]._tree,n.prelim+=e,n.mod+=e,e+=n.shift+(r+=n.change)}function jr(t,n,e){t=t._tree,n=n._tree;var r=e/(n.number-t.number);t.change+=r,n.change-=r,n.shift+=e,n.prelim+=e,n.mod+=e}function Or(t,n,e){return t._tree.ancestor.parent==n.parent?t._tree.ancestor:e}function Yr(t){return{x:t.x,y:t.y,dx:t.dx,dy:t.dy}}function Ur(t,n){var e=t.x+n[3],r=t.y+n[0],u=t.dx-n[1]-n[3],i=t.dy-n[0]-n[2];return 0>u&&(e+=u/2,u=0),0>i&&(r+=i/2,i=0),{x:e,y:r,dx:u,dy:i}}function Ir(t,n){function e(t,e){return d3.xhr(t,n,e).response(r)}function r(t){return e.parse(t.responseText)}function u(n){return n.map(i).join(t)}function i(t){return a.test(t)?'"'+t.replace(/\"/g,'""')+'"':t}var a=RegExp('["'+t+"\n]"),o=t.charCodeAt(0);return e.parse=function(t){var n;return e.parseRows(t,function(t){return n?n(t):(n=Function("d","return {"+t.map(function(t,n){return JSON.stringify(t)+": d["+n+"]"}).join(",")+"}"),void 0)})},e.parseRows=function(t,n){function e(){if(s>=l)return a;if(u)return u=!1,i;var n=s;if(34===t.charCodeAt(n)){for(var e=n;l>e++;)if(34===t.charCodeAt(e)){if(34!==t.charCodeAt(e+1))break;++e}s=e+2;var r=t.charCodeAt(e+1);return 13===r?(u=!0,10===t.charCodeAt(e+2)&&++s):10===r&&(u=!0),t.substring(n+1,e).replace(/""/g,'"')}for(;l>s;){var r=t.charCodeAt(s++),c=1;if(10===r)u=!0;else if(13===r)u=!0,10===t.charCodeAt(s)&&(++s,++c);else if(r!==o)continue;return t.substring(n,s-c)}return t.substring(n)}for(var r,u,i={},a={},c=[],l=t.length,s=0,f=0;(r=e())!==a;){for(var h=[];r!==i&&r!==a;)h.push(r),r=e();(!n||(h=n(h,f++)))&&c.push(h)}return c},e.format=function(t){return t.map(u).join("\n")},e}function Vr(t,n){no.hasOwnProperty(t.type)&&no[t.type](t,n)}function Xr(t,n,e){var r,u=-1,i=t.length-e;for(n.lineStart();i>++u;)r=t[u],n.point(r[0],r[1]);n.lineEnd()}function Zr(t,n){var e=-1,r=t.length;for(n.polygonStart();r>++e;)Xr(t[e],n,1);n.polygonEnd()}function Br(t){return[Math.atan2(t[1],t[0]),Math.asin(Math.max(-1,Math.min(1,t[2])))]}function $r(t,n){return Pi>Math.abs(t[0]-n[0])&&Pi>Math.abs(t[1]-n[1])}function Jr(t){var n=t[0],e=t[1],r=Math.cos(e);return[r*Math.cos(n),r*Math.sin(n),Math.sin(e)]}function Gr(t,n){return t[0]*n[0]+t[1]*n[1]+t[2]*n[2]}function Kr(t,n){return[t[1]*n[2]-t[2]*n[1],t[2]*n[0]-t[0]*n[2],t[0]*n[1]-t[1]*n[0]]}function Wr(t,n){t[0]+=n[0],t[1]+=n[1],t[2]+=n[2]}function Qr(t,n){return[t[0]*n,t[1]*n,t[2]*n]}function tu(t){var n=Math.sqrt(t[0]*t[0]+t[1]*t[1]+t[2]*t[2]);t[0]/=n,t[1]/=n,t[2]/=n}function nu(t){function n(n){function r(e,r){e=t(e,r),n.point(e[0],e[1])}function i(){s=0/0,p.point=a,n.lineStart()}function a(r,i){var a=Jr([r,i]),o=t(r,i);e(s,f,l,h,d,g,s=o[0],f=o[1],l=r,h=a[0],d=a[1],g=a[2],u,n),n.point(s,f)}function o(){p.point=r,n.lineEnd()}function c(){var t,r,c,m,v,y,M;i(),p.point=function(n,e){a(t=n,r=e),c=s,m=f,v=h,y=d,M=g,p.point=a},p.lineEnd=function(){e(s,f,l,h,d,g,c,m,t,v,y,M,u,n),p.lineEnd=o,o()}}var l,s,f,h,d,g,p={point:r,lineStart:i,lineEnd:o,polygonStart:function(){n.polygonStart(),p.lineStart=c},polygonEnd:function(){n.polygonEnd(),p.lineStart=i}};return p}function e(n,u,i,a,o,c,l,s,f,h,d,g,p,m){var v=l-n,y=s-u,M=v*v+y*y;if(M>4*r&&p--){var b=a+h,x=o+d,_=c+g,w=Math.sqrt(b*b+x*x+_*_),S=Math.asin(_/=w),k=Pi>Math.abs(Math.abs(_)-1)?(i+f)/2:Math.atan2(x,b),E=t(k,S),A=E[0],N=E[1],T=A-n,q=N-u,C=y*T-v*q;(C*C/M>r||Math.abs((v*T+y*q)/M-.5)>.3)&&(e(n,u,i,a,o,c,A,N,k,b/=w,x/=w,_,p,m),m.point(A,N),e(A,N,k,b,x,_,l,s,f,h,d,g,p,m))}}var r=.5,u=16;return n.precision=function(t){return arguments.length?(u=(r=t*t)>0&&16,n):Math.sqrt(r)},n}function eu(t,n){function e(t,n){var e=Math.sqrt(i-2*u*Math.sin(n))/u;return[e*Math.sin(t*=u),a-e*Math.cos(t)]}var r=Math.sin(t),u=(r+Math.sin(n))/2,i=1+r*(2*u-r),a=Math.sqrt(i)/u;return e.invert=function(t,n){var e=a-n;return[Math.atan2(t,e)/u,Math.asin((i-(t*t+e*e)*u*u)/(2*u))]},e}function ru(t){function n(t,n){r>t&&(r=t),t>i&&(i=t),u>n&&(u=n),n>a&&(a=n)}function e(){o.point=o.lineEnd=Pn}var r,u,i,a,o={point:n,lineStart:Pn,lineEnd:Pn,polygonStart:function(){o.lineEnd=e},polygonEnd:function(){o.point=n}};return function(n){return a=i=-(r=u=1/0),d3.geo.stream(n,t(o)),[[r,u],[i,a]]}}function uu(t,n){if(!uo){++io,t*=ji;var e=Math.cos(n*=ji);ao+=(e*Math.cos(t)-ao)/io,oo+=(e*Math.sin(t)-oo)/io,co+=(Math.sin(n)-co)/io}}function iu(){var t,n;uo=1,au(),uo=2;var e=lo.point;lo.point=function(r,u){e(t=r,n=u)},lo.lineEnd=function(){lo.point(t,n),ou(),lo.lineEnd=ou}}function au(){function t(t,u){t*=ji;var i=Math.cos(u*=ji),a=i*Math.cos(t),o=i*Math.sin(t),c=Math.sin(u),l=Math.atan2(Math.sqrt((l=e*c-r*o)*l+(l=r*a-n*c)*l+(l=n*o-e*a)*l),n*a+e*o+r*c);io+=l,ao+=l*(n+(n=a)),oo+=l*(e+(e=o)),co+=l*(r+(r=c))}var n,e,r;uo>1||(1>uo&&(uo=1,io=ao=oo=co=0),lo.point=function(u,i){u*=ji;var a=Math.cos(i*=ji);n=a*Math.cos(u),e=a*Math.sin(u),r=Math.sin(i),lo.point=t})}function ou(){lo.point=uu}function cu(t,n){var e=Math.cos(t),r=Math.sin(t);return function(u,i,a,o){null!=u?(u=lu(e,u),i=lu(e,i),(a>0?i>u:u>i)&&(u+=2*a*Ri)):(u=t+2*a*Ri,i=t);for(var c,l=a*n,s=u;a>0?s>i:i>s;s-=l)o.point((c=Br([e,-r*Math.cos(s),-r*Math.sin(s)]))[0],c[1])}}function lu(t,n){var e=Jr(n);e[0]-=t,tu(e);var r=Math.acos(Math.max(-1,Math.min(1,-e[1])));return((0>-e[2]?-r:r)+2*Math.PI-Pi)%(2*Math.PI)}function su(t,n,e){return function(r){function u(n,e){t(n,e)&&r.point(n,e)}function i(t,n){m.point(t,n)}function a(){v.point=i,m.lineStart()}function o(){v.point=u,m.lineEnd()}function c(t,n){M.point(t,n),p.push([t,n])}function l(){M.lineStart(),p=[]}function s(){c(p[0][0],p[0][1]),M.lineEnd();var t,n=M.clean(),e=y.buffer(),u=e.length;if(!u)return g=!0,d+=mu(p,-1),p=null,void 0;if(p=null,1&n){t=e[0],h+=mu(t,1);var i,u=t.length-1,a=-1;for(r.lineStart();u>++a;)r.point((i=t[a])[0],i[1]);return r.lineEnd(),void 0}u>1&&2&n&&e.push(e.pop().concat(e.shift())),f.push(e.filter(gu))}var f,h,d,g,p,m=n(r),v={point:u,lineStart:a,lineEnd:o,polygonStart:function(){v.point=c,v.lineStart=l,v.lineEnd=s,g=!1,d=h=0,f=[],r.polygonStart()
},polygonEnd:function(){v.point=u,v.lineStart=a,v.lineEnd=o,f=d3.merge(f),f.length?fu(f,e,r):(-Pi>h||g&&-Pi>d)&&(r.lineStart(),e(null,null,1,r),r.lineEnd()),r.polygonEnd(),f=null},sphere:function(){r.polygonStart(),r.lineStart(),e(null,null,1,r),r.lineEnd(),r.polygonEnd()}},y=pu(),M=n(y);return v}}function fu(t,n,e){var r=[],u=[];if(t.forEach(function(t){var n=t.length;if(!(1>=n)){var e=t[0],i=t[n-1],a={point:e,points:t,other:null,visited:!1,entry:!0,subject:!0},o={point:e,points:[e],other:a,visited:!1,entry:!1,subject:!1};a.other=o,r.push(a),u.push(o),a={point:i,points:[i],other:null,visited:!1,entry:!1,subject:!0},o={point:i,points:[i],other:a,visited:!1,entry:!0,subject:!1},a.other=o,r.push(a),u.push(o)}}),u.sort(du),hu(r),hu(u),r.length)for(var i,a,o,c=r[0];;){for(i=c;i.visited;)if((i=i.next)===c)return;a=i.points,e.lineStart();do{if(i.visited=i.other.visited=!0,i.entry){if(i.subject)for(var l=0;a.length>l;l++)e.point((o=a[l])[0],o[1]);else n(i.point,i.next.point,1,e);i=i.next}else{if(i.subject){a=i.prev.points;for(var l=a.length;--l>=0;)e.point((o=a[l])[0],o[1])}else n(i.point,i.prev.point,-1,e);i=i.prev}i=i.other,a=i.points}while(!i.visited);e.lineEnd()}}function hu(t){if(n=t.length){for(var n,e,r=0,u=t[0];n>++r;)u.next=e=t[r],e.prev=u,u=e;u.next=e=t[0],e.prev=u}}function du(t,n){return(0>(t=t.point)[0]?t[1]-Ri/2-Pi:Ri/2-t[1])-(0>(n=n.point)[0]?n[1]-Ri/2-Pi:Ri/2-n[1])}function gu(t){return t.length>1}function pu(){var t,n=[];return{lineStart:function(){n.push(t=[])},point:function(n,e){t.push([n,e])},lineEnd:Pn,buffer:function(){var e=n;return n=[],t=null,e}}}function mu(t,n){if(!(e=t.length))return 0;for(var e,r,u,i=0,a=0,o=t[0],c=o[0],l=o[1],s=Math.cos(l),f=Math.atan2(n*Math.sin(c)*s,Math.sin(l)),h=1-n*Math.cos(c)*s,d=f;e>++i;)o=t[i],s=Math.cos(l=o[1]),r=Math.atan2(n*Math.sin(c=o[0])*s,Math.sin(l)),u=1-n*Math.cos(c)*s,Pi>Math.abs(h-2)&&Pi>Math.abs(u-2)||(Pi>Math.abs(u)||Pi>Math.abs(h)||(Pi>Math.abs(Math.abs(r-f)-Ri)?u+h>2&&(a+=4*(r-f)):a+=Pi>Math.abs(h-2)?4*(r-d):((3*Ri+r-f)%(2*Ri)-Ri)*(h+u)),d=f,f=r,h=u);return a}function vu(t){var n,e=0/0,r=0/0,u=0/0;return{lineStart:function(){t.lineStart(),n=1},point:function(i,a){var o=i>0?Ri:-Ri,c=Math.abs(i-e);Pi>Math.abs(c-Ri)?(t.point(e,r=(r+a)/2>0?Ri/2:-Ri/2),t.point(u,r),t.lineEnd(),t.lineStart(),t.point(o,r),t.point(i,r),n=0):u!==o&&c>=Ri&&(Pi>Math.abs(e-u)&&(e-=u*Pi),Pi>Math.abs(i-o)&&(i-=o*Pi),r=yu(e,r,i,a),t.point(u,r),t.lineEnd(),t.lineStart(),t.point(o,r),n=0),t.point(e=i,r=a),u=o},lineEnd:function(){t.lineEnd(),e=r=0/0},clean:function(){return 2-n}}}function yu(t,n,e,r){var u,i,a=Math.sin(t-e);return Math.abs(a)>Pi?Math.atan((Math.sin(n)*(i=Math.cos(r))*Math.sin(e)-Math.sin(r)*(u=Math.cos(n))*Math.sin(t))/(u*i*a)):(n+r)/2}function Mu(t,n,e,r){var u;if(null==t)u=e*Ri/2,r.point(-Ri,u),r.point(0,u),r.point(Ri,u),r.point(Ri,0),r.point(Ri,-u),r.point(0,-u),r.point(-Ri,-u),r.point(-Ri,0),r.point(-Ri,u);else if(Math.abs(t[0]-n[0])>Pi){var i=(t[0]<n[0]?1:-1)*Ri;u=e*i/2,r.point(-i,u),r.point(0,u),r.point(i,u)}else r.point(n[0],n[1])}function bu(t){function n(t,n){return Math.cos(t)*Math.cos(n)>i}function e(t){var e,u,i,a;return{lineStart:function(){i=u=!1,a=1},point:function(o,c){var l,s=[o,c],f=n(o,c);!e&&(i=u=f)&&t.lineStart(),f!==u&&(l=r(e,s),($r(e,l)||$r(s,l))&&(s[0]+=Pi,s[1]+=Pi,f=n(s[0],s[1]))),f!==u&&(a=0,(u=f)?(t.lineStart(),l=r(s,e),t.point(l[0],l[1])):(l=r(e,s),t.point(l[0],l[1]),t.lineEnd()),e=l),!f||e&&$r(e,s)||t.point(s[0],s[1]),e=s},lineEnd:function(){u&&t.lineEnd(),e=null},clean:function(){return a|(i&&u)<<1}}}function r(t,n){var e=Jr(t,0),r=Jr(n,0),u=[1,0,0],a=Kr(e,r),o=Gr(a,a),c=a[0],l=o-c*c;if(!l)return t;var s=i*o/l,f=-i*c/l,h=Kr(u,a),d=Qr(u,s),g=Qr(a,f);Wr(d,g);var p=h,m=Gr(d,p),v=Gr(p,p),y=Math.sqrt(m*m-v*(Gr(d,d)-1)),M=Qr(p,(-m-y)/v);return Wr(M,d),Br(M)}var u=t*ji,i=Math.cos(u),a=cu(u,6*ji);return su(n,e,a)}function xu(t,n){function e(e,r){return e=t(e,r),n(e[0],e[1])}return t.invert&&n.invert&&(e.invert=function(e,r){return e=n.invert(e,r),t.invert(e[0],e[1])}),e}function _u(t,n){return[t,n]}function wu(t,n,e){var r=d3.range(t,n-Pi,e).concat(n);return function(t){return r.map(function(n){return[t,n]})}}function Su(t,n,e){var r=d3.range(t,n-Pi,e).concat(n);return function(t){return r.map(function(n){return[n,t]})}}function ku(t,n,e,r){function u(t){var n=Math.sin(t*=d)*g,e=Math.sin(d-t)*g,r=e*l+n*f,u=e*s+n*h,i=e*a+n*c;return[Math.atan2(u,r)/ji,Math.atan2(i,Math.sqrt(r*r+u*u))/ji]}var i=Math.cos(n),a=Math.sin(n),o=Math.cos(r),c=Math.sin(r),l=i*Math.cos(t),s=i*Math.sin(t),f=o*Math.cos(e),h=o*Math.sin(e),d=Math.acos(Math.max(-1,Math.min(1,a*c+i*o*Math.cos(e-t)))),g=1/Math.sin(d);return u.distance=d,u}function Eu(t,n){return[t/(2*Ri),Math.max(-.5,Math.min(.5,Math.log(Math.tan(Ri/4+n/2))/(2*Ri)))]}function Au(t){return"m0,"+t+"a"+t+","+t+" 0 1,1 0,"+-2*t+"a"+t+","+t+" 0 1,1 0,"+2*t+"z"}function Nu(t){var n=nu(function(n,e){return t([n*Oi,e*Oi])});return function(t){return t=n(t),{point:function(n,e){t.point(n*ji,e*ji)},sphere:function(){t.sphere()},lineStart:function(){t.lineStart()},lineEnd:function(){t.lineEnd()},polygonStart:function(){t.polygonStart()},polygonEnd:function(){t.polygonEnd()}}}}function Tu(){function t(t,n){a.push("M",t,",",n,i)}function n(t,n){a.push("M",t,",",n),o.point=e}function e(t,n){a.push("L",t,",",n)}function r(){o.point=t}function u(){a.push("Z")}var i=Au(4.5),a=[],o={point:t,lineStart:function(){o.point=n},lineEnd:r,polygonStart:function(){o.lineEnd=u},polygonEnd:function(){o.lineEnd=r,o.point=t},pointRadius:function(t){return i=Au(t),o},result:function(){if(a.length){var t=a.join("");return a=[],t}}};return o}function qu(t){function n(n,e){t.moveTo(n,e),t.arc(n,e,a,0,2*Ri)}function e(n,e){t.moveTo(n,e),o.point=r}function r(n,e){t.lineTo(n,e)}function u(){o.point=n}function i(){t.closePath()}var a=4.5,o={point:n,lineStart:function(){o.point=e},lineEnd:u,polygonStart:function(){o.lineEnd=i},polygonEnd:function(){o.lineEnd=u,o.point=n},pointRadius:function(t){return a=t,o},result:Pn};return o}function Cu(){function t(t,n){po+=u*t-r*n,r=t,u=n}var n,e,r,u;mo.point=function(i,a){mo.point=t,n=r=i,e=u=a},mo.lineEnd=function(){t(n,e)}}function zu(t,n){uo||(ao+=t,oo+=n,++co)}function Du(){function t(t,r){var u=t-n,i=r-e,a=Math.sqrt(u*u+i*i);ao+=a*(n+t)/2,oo+=a*(e+r)/2,co+=a,n=t,e=r}var n,e;if(1!==uo){if(!(1>uo))return;uo=1,ao=oo=co=0}vo.point=function(r,u){vo.point=t,n=r,e=u}}function Lu(){vo.point=zu}function Fu(){function t(t,n){var e=u*t-r*n;ao+=e*(r+t),oo+=e*(u+n),co+=3*e,r=t,u=n}var n,e,r,u;2>uo&&(uo=2,ao=oo=co=0),vo.point=function(i,a){vo.point=t,n=r=i,e=u=a},vo.lineEnd=function(){t(n,e)}}function Hu(){function t(t,n){if(t*=ji,n*=ji,!(Pi>Math.abs(Math.abs(i)-Ri/2)&&Pi>Math.abs(Math.abs(n)-Ri/2))){var e=Math.cos(n),c=Math.sin(n);if(Pi>Math.abs(i-Ri/2))Mo+=2*(t-r);else{var l=t-u,s=Math.cos(l),f=Math.atan2(Math.sqrt((f=e*Math.sin(l))*f+(f=a*c-o*e*s)*f),o*c+a*e*s),h=(f+Ri+i+n)/4;Mo+=(0>l&&l>-Ri||l>Ri?-4:4)*Math.atan(Math.sqrt(Math.abs(Math.tan(h)*Math.tan(h-f/2)*Math.tan(h-Ri/4-i/2)*Math.tan(h-Ri/4-n/2))))}r=u,u=t,i=n,a=e,o=c}}var n,e,r,u,i,a,o;bo.point=function(c,l){bo.point=t,r=u=(n=c)*ji,i=(e=l)*ji,a=Math.cos(i),o=Math.sin(i)},bo.lineEnd=function(){t(n,e)}}function Ru(t){return Pu(function(){return t})()}function Pu(t){function n(t){return t=a(t[0]*ji,t[1]*ji),[t[0]*s+o,c-t[1]*s]}function e(t){return t=a.invert((t[0]-o)/s,(c-t[1])/s),[t[0]*Oi,t[1]*Oi]}function r(){a=xu(i=Ou(p,m,v),u);var t=u(d,g);return o=f-t[0]*s,c=h+t[1]*s,n}var u,i,a,o,c,l=nu(function(t,n){return t=u(t,n),[t[0]*s+o,c-t[1]*s]}),s=150,f=480,h=250,d=0,g=0,p=0,m=0,v=0,y=so,M=null;return n.stream=function(t){return ju(i,y(l(t)))},n.clipAngle=function(t){return arguments.length?(y=null==t?(M=t,so):bu(M=+t),n):M},n.scale=function(t){return arguments.length?(s=+t,r()):s},n.translate=function(t){return arguments.length?(f=+t[0],h=+t[1],r()):[f,h]},n.center=function(t){return arguments.length?(d=t[0]%360*ji,g=t[1]%360*ji,r()):[d*Oi,g*Oi]},n.rotate=function(t){return arguments.length?(p=t[0]%360*ji,m=t[1]%360*ji,v=t.length>2?t[2]%360*ji:0,r()):[p*Oi,m*Oi,v*Oi]},d3.rebind(n,l,"precision"),function(){return u=t.apply(this,arguments),n.invert=u.invert&&e,r()}}function ju(t,n){return{point:function(e,r){r=t(e*ji,r*ji),e=r[0],n.point(e>Ri?e-2*Ri:-Ri>e?e+2*Ri:e,r[1])},sphere:function(){n.sphere()},lineStart:function(){n.lineStart()},lineEnd:function(){n.lineEnd()},polygonStart:function(){n.polygonStart()},polygonEnd:function(){n.polygonEnd()}}}function Ou(t,n,e){return t?n||e?xu(Uu(t),Iu(n,e)):Uu(t):n||e?Iu(n,e):_u}function Yu(t){return function(n,e){return n+=t,[n>Ri?n-2*Ri:-Ri>n?n+2*Ri:n,e]}}function Uu(t){var n=Yu(t);return n.invert=Yu(-t),n}function Iu(t,n){function e(t,n){var e=Math.cos(n),o=Math.cos(t)*e,c=Math.sin(t)*e,l=Math.sin(n),s=l*r+o*u;return[Math.atan2(c*i-s*a,o*r-l*u),Math.asin(Math.max(-1,Math.min(1,s*i+c*a)))]}var r=Math.cos(t),u=Math.sin(t),i=Math.cos(n),a=Math.sin(n);return e.invert=function(t,n){var e=Math.cos(n),o=Math.cos(t)*e,c=Math.sin(t)*e,l=Math.sin(n),s=l*i-c*a;return[Math.atan2(c*i+l*a,o*r+s*u),Math.asin(Math.max(-1,Math.min(1,s*r-o*u)))]},e}function Vu(t,n){function e(n,e){var r=Math.cos(n),u=Math.cos(e),i=t(r*u);return[i*u*Math.sin(n),i*Math.sin(e)]}return e.invert=function(t,e){var r=Math.sqrt(t*t+e*e),u=n(r),i=Math.sin(u),a=Math.cos(u);return[Math.atan2(t*i,r*a),Math.asin(r&&e*i/r)]},e}function Xu(t,n,e,r){var u,i,a,o,c,l,s;return u=r[t],i=u[0],a=u[1],u=r[n],o=u[0],c=u[1],u=r[e],l=u[0],s=u[1],(s-a)*(o-i)-(c-a)*(l-i)>0}function Zu(t,n,e){return(e[0]-n[0])*(t[1]-n[1])<(e[1]-n[1])*(t[0]-n[0])}function Bu(t,n,e,r){var u=t[0],i=e[0],a=n[0]-u,o=r[0]-i,c=t[1],l=e[1],s=n[1]-c,f=r[1]-l,h=(o*(c-l)-f*(u-i))/(f*a-o*s);return[u+h*a,c+h*s]}function $u(t,n){var e={list:t.map(function(t,n){return{index:n,x:t[0],y:t[1]}}).sort(function(t,n){return t.y<n.y?-1:t.y>n.y?1:t.x<n.x?-1:t.x>n.x?1:0}),bottomSite:null},r={list:[],leftEnd:null,rightEnd:null,init:function(){r.leftEnd=r.createHalfEdge(null,"l"),r.rightEnd=r.createHalfEdge(null,"l"),r.leftEnd.r=r.rightEnd,r.rightEnd.l=r.leftEnd,r.list.unshift(r.leftEnd,r.rightEnd)},createHalfEdge:function(t,n){return{edge:t,side:n,vertex:null,l:null,r:null}},insert:function(t,n){n.l=t,n.r=t.r,t.r.l=n,t.r=n},leftBound:function(t){var n=r.leftEnd;do n=n.r;while(n!=r.rightEnd&&u.rightOf(n,t));return n=n.l},del:function(t){t.l.r=t.r,t.r.l=t.l,t.edge=null},right:function(t){return t.r},left:function(t){return t.l},leftRegion:function(t){return null==t.edge?e.bottomSite:t.edge.region[t.side]},rightRegion:function(t){return null==t.edge?e.bottomSite:t.edge.region[_o[t.side]]}},u={bisect:function(t,n){var e={region:{l:t,r:n},ep:{l:null,r:null}},r=n.x-t.x,u=n.y-t.y,i=r>0?r:-r,a=u>0?u:-u;return e.c=t.x*r+t.y*u+.5*(r*r+u*u),i>a?(e.a=1,e.b=u/r,e.c/=r):(e.b=1,e.a=r/u,e.c/=u),e},intersect:function(t,n){var e=t.edge,r=n.edge;if(!e||!r||e.region.r==r.region.r)return null;var u=e.a*r.b-e.b*r.a;if(1e-10>Math.abs(u))return null;var i,a,o=(e.c*r.b-r.c*e.b)/u,c=(r.c*e.a-e.c*r.a)/u,l=e.region.r,s=r.region.r;l.y<s.y||l.y==s.y&&l.x<s.x?(i=t,a=e):(i=n,a=r);var f=o>=a.region.r.x;return f&&"l"===i.side||!f&&"r"===i.side?null:{x:o,y:c}},rightOf:function(t,n){var e=t.edge,r=e.region.r,u=n.x>r.x;if(u&&"l"===t.side)return 1;if(!u&&"r"===t.side)return 0;if(1===e.a){var i=n.y-r.y,a=n.x-r.x,o=0,c=0;if(!u&&0>e.b||u&&e.b>=0?c=o=i>=e.b*a:(c=n.x+n.y*e.b>e.c,0>e.b&&(c=!c),c||(o=1)),!o){var l=r.x-e.region.l.x;c=e.b*(a*a-i*i)<l*i*(1+2*a/l+e.b*e.b),0>e.b&&(c=!c)}}else{var s=e.c-e.a*n.x,f=n.y-s,h=n.x-r.x,d=s-r.y;c=f*f>h*h+d*d}return"l"===t.side?c:!c},endPoint:function(t,e,r){t.ep[e]=r,t.ep[_o[e]]&&n(t)},distance:function(t,n){var e=t.x-n.x,r=t.y-n.y;return Math.sqrt(e*e+r*r)}},i={list:[],insert:function(t,n,e){t.vertex=n,t.ystar=n.y+e;for(var r=0,u=i.list,a=u.length;a>r;r++){var o=u[r];if(!(t.ystar>o.ystar||t.ystar==o.ystar&&n.x>o.vertex.x))break}u.splice(r,0,t)},del:function(t){for(var n=0,e=i.list,r=e.length;r>n&&e[n]!=t;++n);e.splice(n,1)},empty:function(){return 0===i.list.length},nextEvent:function(t){for(var n=0,e=i.list,r=e.length;r>n;++n)if(e[n]==t)return e[n+1];return null},min:function(){var t=i.list[0];return{x:t.vertex.x,y:t.ystar}},extractMin:function(){return i.list.shift()}};r.init(),e.bottomSite=e.list.shift();for(var a,o,c,l,s,f,h,d,g,p,m,v,y,M=e.list.shift();;)if(i.empty()||(a=i.min()),M&&(i.empty()||M.y<a.y||M.y==a.y&&M.x<a.x))o=r.leftBound(M),c=r.right(o),h=r.rightRegion(o),v=u.bisect(h,M),f=r.createHalfEdge(v,"l"),r.insert(o,f),p=u.intersect(o,f),p&&(i.del(o),i.insert(o,p,u.distance(p,M))),o=f,f=r.createHalfEdge(v,"r"),r.insert(o,f),p=u.intersect(f,c),p&&i.insert(f,p,u.distance(p,M)),M=e.list.shift();else{if(i.empty())break;o=i.extractMin(),l=r.left(o),c=r.right(o),s=r.right(c),h=r.leftRegion(o),d=r.rightRegion(c),m=o.vertex,u.endPoint(o.edge,o.side,m),u.endPoint(c.edge,c.side,m),r.del(o),i.del(c),r.del(c),y="l",h.y>d.y&&(g=h,h=d,d=g,y="r"),v=u.bisect(h,d),f=r.createHalfEdge(v,y),r.insert(l,f),u.endPoint(v,_o[y],m),p=u.intersect(l,f),p&&(i.del(l),i.insert(l,p,u.distance(p,h))),p=u.intersect(f,s),p&&i.insert(f,p,u.distance(p,h))}for(o=r.right(r.leftEnd);o!=r.rightEnd;o=r.right(o))n(o.edge)}function Ju(){return{leaf:!0,nodes:[],point:null}}function Gu(t,n,e,r,u,i){if(!t(n,e,r,u,i)){var a=.5*(e+u),o=.5*(r+i),c=n.nodes;c[0]&&Gu(t,c[0],e,r,a,o),c[1]&&Gu(t,c[1],a,r,u,o),c[2]&&Gu(t,c[2],e,o,a,i),c[3]&&Gu(t,c[3],a,o,u,i)}}function Ku(){this._=new Date(arguments.length>1?Date.UTC.apply(this,arguments):arguments[0])}function Wu(t,n,e,r){for(var u,i,a=0,o=n.length,c=e.length;o>a;){if(r>=c)return-1;if(u=n.charCodeAt(a++),37===u){if(i=Yo[n.charAt(a++)],!i||0>(r=i(t,e,r)))return-1}else if(u!=e.charCodeAt(r++))return-1}return r}function Qu(t){return RegExp("^(?:"+t.map(d3.requote).join("|")+")","i")}function ti(t){for(var n=new i,e=-1,r=t.length;r>++e;)n.set(t[e].toLowerCase(),e);return n}function ni(t,n,e){t+="";var r=t.length;return e>r?Array(e-r+1).join(n)+t:t}function ei(t,n,e){Lo.lastIndex=0;var r=Lo.exec(n.substring(e));return r?e+=r[0].length:-1}function ri(t,n,e){Do.lastIndex=0;var r=Do.exec(n.substring(e));return r?e+=r[0].length:-1}function ui(t,n,e){Ro.lastIndex=0;var r=Ro.exec(n.substring(e));return r?(t.m=Po.get(r[0].toLowerCase()),e+=r[0].length):-1}function ii(t,n,e){Fo.lastIndex=0;var r=Fo.exec(n.substring(e));return r?(t.m=Ho.get(r[0].toLowerCase()),e+=r[0].length):-1}function ai(t,n,e){return Wu(t,""+Oo.c,n,e)}function oi(t,n,e){return Wu(t,""+Oo.x,n,e)}function ci(t,n,e){return Wu(t,""+Oo.X,n,e)}function li(t,n,e){Uo.lastIndex=0;var r=Uo.exec(n.substring(e,e+4));return r?(t.y=+r[0],e+=r[0].length):-1}function si(t,n,e){Uo.lastIndex=0;var r=Uo.exec(n.substring(e,e+2));return r?(t.y=fi(+r[0]),e+=r[0].length):-1}function fi(t){return t+(t>68?1900:2e3)}function hi(t,n,e){Uo.lastIndex=0;var r=Uo.exec(n.substring(e,e+2));return r?(t.m=r[0]-1,e+=r[0].length):-1}function di(t,n,e){Uo.lastIndex=0;var r=Uo.exec(n.substring(e,e+2));return r?(t.d=+r[0],e+=r[0].length):-1}function gi(t,n,e){Uo.lastIndex=0;var r=Uo.exec(n.substring(e,e+2));return r?(t.H=+r[0],e+=r[0].length):-1}function pi(t,n,e){Uo.lastIndex=0;var r=Uo.exec(n.substring(e,e+2));return r?(t.M=+r[0],e+=r[0].length):-1}function mi(t,n,e){Uo.lastIndex=0;var r=Uo.exec(n.substring(e,e+2));return r?(t.S=+r[0],e+=r[0].length):-1}function vi(t,n,e){Uo.lastIndex=0;var r=Uo.exec(n.substring(e,e+3));return r?(t.L=+r[0],e+=r[0].length):-1}function yi(t,n,e){var r=Io.get(n.substring(e,e+=2).toLowerCase());return null==r?-1:(t.p=r,e)}function Mi(t){var n=t.getTimezoneOffset(),e=n>0?"-":"+",r=~~(Math.abs(n)/60),u=Math.abs(n)%60;return e+ni(r,"0",2)+ni(u,"0",2)}function bi(t){return t.toISOString()}function xi(t,n,e){function r(n){var e=t(n),r=i(e,1);return r-n>n-e?e:r}function u(e){return n(e=t(new wo(e-1)),1),e}function i(t,e){return n(t=new wo(+t),e),t}function a(t,r,i){var a=u(t),o=[];if(i>1)for(;r>a;)e(a)%i||o.push(new Date(+a)),n(a,1);else for(;r>a;)o.push(new Date(+a)),n(a,1);return o}function o(t,n,e){try{wo=Ku;var r=new Ku;return r._=t,a(r,n,e)}finally{wo=Date}}t.floor=t,t.round=r,t.ceil=u,t.offset=i,t.range=a;var c=t.utc=_i(t);return c.floor=c,c.round=_i(r),c.ceil=_i(u),c.offset=_i(i),c.range=o,t}function _i(t){return function(n,e){try{wo=Ku;var r=new Ku;return r._=n,t(r,e)._}finally{wo=Date}}}function wi(t,n,e){function r(n){return t(n)}return r.invert=function(n){return ki(t.invert(n))},r.domain=function(n){return arguments.length?(t.domain(n),r):t.domain().map(ki)},r.nice=function(t){return r.domain(Yn(r.domain(),function(){return t}))},r.ticks=function(e,u){var i=Si(r.domain());if("function"!=typeof e){var a=i[1]-i[0],o=a/e,c=d3.bisect(Xo,o);if(c==Xo.length)return n.year(i,e);if(!c)return t.ticks(e).map(ki);Math.log(o/Xo[c-1])<Math.log(Xo[c]/o)&&--c,e=n[c],u=e[1],e=e[0].range}return e(i[0],new Date(+i[1]+1),u)},r.tickFormat=function(){return e},r.copy=function(){return wi(t.copy(),n,e)},d3.rebind(r,t,"range","rangeRound","interpolate","clamp")}function Si(t){var n=t[0],e=t[t.length-1];return e>n?[n,e]:[e,n]}function ki(t){return new Date(t)}function Ei(t){return function(n){for(var e=t.length-1,r=t[e];!r[1](n);)r=t[--e];return r[0](n)}}function Ai(t){var n=new Date(t,0,1);return n.setFullYear(t),n}function Ni(t){var n=t.getFullYear(),e=Ai(n),r=Ai(n+1);return n+(t-e)/(r-e)}function Ti(t){var n=new Date(Date.UTC(t,0,1));return n.setUTCFullYear(t),n}function qi(t){var n=t.getUTCFullYear(),e=Ti(n),r=Ti(n+1);return n+(t-e)/(r-e)}var Ci=".",zi=",",Di=[3,3];Date.now||(Date.now=function(){return+new Date});try{document.createElement("div").style.setProperty("opacity",0,"")}catch(Li){var Fi=CSSStyleDeclaration.prototype,Hi=Fi.setProperty;Fi.setProperty=function(t,n,e){Hi.call(this,t,n+"",e)}}d3={version:"3.0.2"};var Ri=Math.PI,Pi=1e-6,ji=Ri/180,Oi=180/Ri,Yi=u;try{Yi(document.documentElement.childNodes)[0].nodeType}catch(Ui){Yi=r}var Ii=[].__proto__?function(t,n){t.__proto__=n}:function(t,n){for(var e in n)t[e]=n[e]};d3.map=function(t){var n=new i;for(var e in t)n.set(e,t[e]);return n},e(i,{has:function(t){return Vi+t in this},get:function(t){return this[Vi+t]},set:function(t,n){return this[Vi+t]=n},remove:function(t){return t=Vi+t,t in this&&delete this[t]},keys:function(){var t=[];return this.forEach(function(n){t.push(n)}),t},values:function(){var t=[];return this.forEach(function(n,e){t.push(e)}),t},entries:function(){var t=[];return this.forEach(function(n,e){t.push({key:n,value:e})}),t},forEach:function(t){for(var n in this)n.charCodeAt(0)===Xi&&t.call(this,n.substring(1),this[n])}});var Vi="\0",Xi=Vi.charCodeAt(0);d3.functor=c,d3.rebind=function(t,n){for(var e,r=1,u=arguments.length;u>++r;)t[e=arguments[r]]=l(t,n,n[e]);return t},d3.ascending=function(t,n){return n>t?-1:t>n?1:t>=n?0:0/0},d3.descending=function(t,n){return t>n?-1:n>t?1:n>=t?0:0/0},d3.mean=function(t,n){var e,r=t.length,u=0,i=-1,a=0;if(1===arguments.length)for(;r>++i;)s(e=t[i])&&(u+=(e-u)/++a);else for(;r>++i;)s(e=n.call(t,t[i],i))&&(u+=(e-u)/++a);return a?u:void 0},d3.median=function(t,n){return arguments.length>1&&(t=t.map(n)),t=t.filter(s),t.length?d3.quantile(t.sort(d3.ascending),.5):void 0},d3.min=function(t,n){var e,r,u=-1,i=t.length;if(1===arguments.length){for(;i>++u&&(null==(e=t[u])||e!=e);)e=void 0;for(;i>++u;)null!=(r=t[u])&&e>r&&(e=r)}else{for(;i>++u&&(null==(e=n.call(t,t[u],u))||e!=e);)e=void 0;for(;i>++u;)null!=(r=n.call(t,t[u],u))&&e>r&&(e=r)}return e},d3.max=function(t,n){var e,r,u=-1,i=t.length;if(1===arguments.length){for(;i>++u&&(null==(e=t[u])||e!=e);)e=void 0;for(;i>++u;)null!=(r=t[u])&&r>e&&(e=r)}else{for(;i>++u&&(null==(e=n.call(t,t[u],u))||e!=e);)e=void 0;for(;i>++u;)null!=(r=n.call(t,t[u],u))&&r>e&&(e=r)}return e},d3.extent=function(t,n){var e,r,u,i=-1,a=t.length;if(1===arguments.length){for(;a>++i&&(null==(e=u=t[i])||e!=e);)e=u=void 0;for(;a>++i;)null!=(r=t[i])&&(e>r&&(e=r),r>u&&(u=r))}else{for(;a>++i&&(null==(e=u=n.call(t,t[i],i))||e!=e);)e=void 0;for(;a>++i;)null!=(r=n.call(t,t[i],i))&&(e>r&&(e=r),r>u&&(u=r))}return[e,u]},d3.random={normal:function(t,n){var e=arguments.length;return 2>e&&(n=1),1>e&&(t=0),function(){var e,r,u;do e=2*Math.random()-1,r=2*Math.random()-1,u=e*e+r*r;while(!u||u>1);return t+n*e*Math.sqrt(-2*Math.log(u)/u)}},logNormal:function(t,n){var e=arguments.length;2>e&&(n=1),1>e&&(t=0);var r=d3.random.normal();return function(){return Math.exp(t+n*r())}},irwinHall:function(t){return function(){for(var n=0,e=0;t>e;e++)n+=Math.random();return n/t}}},d3.sum=function(t,n){var e,r=0,u=t.length,i=-1;if(1===arguments.length)for(;u>++i;)isNaN(e=+t[i])||(r+=e);else for(;u>++i;)isNaN(e=+n.call(t,t[i],i))||(r+=e);return r},d3.quantile=function(t,n){var e=(t.length-1)*n+1,r=Math.floor(e),u=+t[r-1],i=e-r;return i?u+i*(t[r]-u):u},d3.shuffle=function(t){for(var n,e,r=t.length;r;)e=0|Math.random()*r--,n=t[r],t[r]=t[e],t[e]=n;return t},d3.transpose=function(t){return d3.zip.apply(d3,t)},d3.zip=function(){if(!(r=arguments.length))return[];for(var t=-1,n=d3.min(arguments,f),e=Array(n);n>++t;)for(var r,u=-1,i=e[t]=Array(r);r>++u;)i[u]=arguments[u][t];return e},d3.bisector=function(t){return{left:function(n,e,r,u){for(3>arguments.length&&(r=0),4>arguments.length&&(u=n.length);u>r;){var i=r+u>>>1;e>t.call(n,n[i],i)?r=i+1:u=i}return r},right:function(n,e,r,u){for(3>arguments.length&&(r=0),4>arguments.length&&(u=n.length);u>r;){var i=r+u>>>1;t.call(n,n[i],i)>e?u=i:r=i+1}return r}}};var Zi=d3.bisector(function(t){return t});d3.bisectLeft=Zi.left,d3.bisect=d3.bisectRight=Zi.right,d3.nest=function(){function t(n,o){if(o>=a.length)return r?r.call(u,n):e?n.sort(e):n;for(var c,l,s,f=-1,h=n.length,d=a[o++],g=new i,p={};h>++f;)(s=g.get(c=d(l=n[f])))?s.push(l):g.set(c,[l]);return g.forEach(function(n,e){p[n]=t(e,o)}),p}function n(t,e){if(e>=a.length)return t;var r,u=[],i=o[e++];for(r in t)u.push({key:r,values:n(t[r],e)});return i&&u.sort(function(t,n){return i(t.key,n.key)}),u}var e,r,u={},a=[],o=[];return u.map=function(n){return t(n,0)},u.entries=function(e){return n(t(e,0),0)},u.key=function(t){return a.push(t),u},u.sortKeys=function(t){return o[a.length-1]=t,u},u.sortValues=function(t){return e=t,u},u.rollup=function(t){return r=t,u},u},d3.keys=function(t){var n=[];for(var e in t)n.push(e);return n},d3.values=function(t){var n=[];for(var e in t)n.push(t[e]);return n},d3.entries=function(t){var n=[];for(var e in t)n.push({key:e,value:t[e]});return n},d3.permute=function(t,n){for(var e=[],r=-1,u=n.length;u>++r;)e[r]=t[n[r]];return e},d3.merge=function(t){return Array.prototype.concat.apply([],t)},d3.range=function(t,n,e){if(3>arguments.length&&(e=1,2>arguments.length&&(n=t,t=0)),1/0===(n-t)/e)throw Error("infinite range");var r,u=[],i=d(Math.abs(e)),a=-1;if(t*=i,n*=i,e*=i,0>e)for(;(r=t+e*++a)>n;)u.push(r/i);else for(;n>(r=t+e*++a);)u.push(r/i);return u},d3.requote=function(t){return t.replace(Bi,"\\$&")};var Bi=/[\\\^\$\*\+\?\|\[\]\(\)\.\{\}]/g;d3.round=function(t,n){return n?Math.round(t*(n=Math.pow(10,n)))/n:Math.round(t)},d3.xhr=function(t,n,e){function r(){var t=l.status;!t&&l.responseText||t>=200&&300>t||304===t?i.load.call(u,c.call(u,l)):i.error.call(u,l)}var u={},i=d3.dispatch("progress","load","error"),o={},c=a,l=new(window.XDomainRequest&&/^(http(s)?:)?\/\//.test(t)?XDomainRequest:XMLHttpRequest);return"onload"in l?l.onload=l.onerror=r:l.onreadystatechange=function(){l.readyState>3&&r()},l.onprogress=function(t){var n=d3.event;d3.event=t;try{i.progress.call(u,l)}finally{d3.event=n}},u.header=function(t,n){return t=(t+"").toLowerCase(),2>arguments.length?o[t]:(null==n?delete o[t]:o[t]=n+"",u)},u.mimeType=function(t){return arguments.length?(n=null==t?null:t+"",u):n},u.response=function(t){return c=t,u},["get","post"].forEach(function(t){u[t]=function(){return u.send.apply(u,[t].concat(Yi(arguments)))}}),u.send=function(e,r,i){if(2===arguments.length&&"function"==typeof r&&(i=r,r=null),l.open(e,t,!0),null==n||"accept"in o||(o.accept=n+",*/*"),l.setRequestHeader)for(var a in o)l.setRequestHeader(a,o[a]);return null!=n&&l.overrideMimeType&&l.overrideMimeType(n),null!=i&&u.on("error",i).on("load",function(t){i(null,t)}),l.send(null==r?null:r),u},u.abort=function(){return l.abort(),u},d3.rebind(u,i,"on"),2===arguments.length&&"function"==typeof n&&(e=n,n=null),null==e?u:u.get(g(e))},d3.text=function(){return d3.xhr.apply(d3,arguments).response(p)},d3.json=function(t,n){return d3.xhr(t,"application/json",n).response(m)},d3.html=function(t,n){return d3.xhr(t,"text/html",n).response(v)},d3.xml=function(){return d3.xhr.apply(d3,arguments).response(y)};var $i={svg:"http://www.w3.org/2000/svg",xhtml:"http://www.w3.org/1999/xhtml",xlink:"http://www.w3.org/1999/xlink",xml:"http://www.w3.org/XML/1998/namespace",xmlns:"http://www.w3.org/2000/xmlns/"};d3.ns={prefix:$i,qualify:function(t){var n=t.indexOf(":"),e=t;return n>=0&&(e=t.substring(0,n),t=t.substring(n+1)),$i.hasOwnProperty(e)?{space:$i[e],local:t}:t}},d3.dispatch=function(){for(var t=new M,n=-1,e=arguments.length;e>++n;)t[arguments[n]]=b(t);return t},M.prototype.on=function(t,n){var e=t.indexOf("."),r="";return e>0&&(r=t.substring(e+1),t=t.substring(0,e)),2>arguments.length?this[t].on(r):this[t].on(r,n)},d3.format=function(t){var n=Ji.exec(t),e=n[1]||" ",r=n[2]||">",u=n[3]||"",i=n[4]||"",a=n[5],o=+n[6],c=n[7],l=n[8],s=n[9],f=1,h="",d=!1;switch(l&&(l=+l.substring(1)),(a||"0"===e&&"="===r)&&(a=e="0",r="=",c&&(o-=Math.floor((o-1)/4))),s){case"n":c=!0,s="g";break;case"%":f=100,h="%",s="f";break;case"p":f=100,h="%",s="r";break;case"b":case"o":case"x":case"X":i&&(i="0"+s.toLowerCase());case"c":case"d":d=!0,l=0;break;case"s":f=-1,s="r"}"#"===i&&(i=""),"r"!=s||l||(s="g"),s=Gi.get(s)||_;var g=a&&c;return function(t){if(d&&t%1)return"";var n=0>t||0===t&&0>1/t?(t=-t,"-"):u;if(0>f){var p=d3.formatPrefix(t,l);t=p.scale(t),h=p.symbol}else t*=f;t=s(t,l),!a&&c&&(t=Ki(t));var m=i.length+t.length+(g?0:n.length),v=o>m?Array(m=o-m+1).join(e):"";return g&&(t=Ki(v+t)),Ci&&t.replace(".",Ci),n+=i,("<"===r?n+t+v:">"===r?v+n+t:"^"===r?v.substring(0,m>>=1)+n+t+v.substring(m):n+(g?t:v+t))+h}};var Ji=/(?:([^{])?([<>=^]))?([+\- ])?(#)?(0)?([0-9]+)?(,)?(\.[0-9]+)?([a-zA-Z%])?/,Gi=d3.map({b:function(t){return t.toString(2)},c:function(t){return String.fromCharCode(t)},o:function(t){return t.toString(8)},x:function(t){return t.toString(16)},X:function(t){return t.toString(16).toUpperCase()},g:function(t,n){return t.toPrecision(n)},e:function(t,n){return t.toExponential(n)},f:function(t,n){return t.toFixed(n)},r:function(t,n){return d3.round(t,n=x(t,n)).toFixed(Math.max(0,Math.min(20,n)))}}),Ki=a;if(Di){var Wi=Di.length;Ki=function(t){for(var n=t.lastIndexOf("."),e=n>=0?"."+t.substring(n+1):(n=t.length,""),r=[],u=0,i=Di[0];n>0&&i>0;)r.push(t.substring(n-=i,n+i)),i=Di[u=(u+1)%Wi];return r.reverse().join(zi||"")+e}}var Qi=["y","z","a","f","p","n","μ","m","","k","M","G","T","P","E","Z","Y"].map(w);d3.formatPrefix=function(t,n){var e=0;return t&&(0>t&&(t*=-1),n&&(t=d3.round(t,x(t,n))),e=1+Math.floor(1e-12+Math.log(t)/Math.LN10),e=Math.max(-24,Math.min(24,3*Math.floor((0>=e?e+1:e-1)/3)))),Qi[8+e/3]};var ta=function(){return a},na=d3.map({linear:ta,poly:q,quad:function(){return A},cubic:function(){return N},sin:function(){return C},exp:function(){return z},circle:function(){return D},elastic:L,back:F,bounce:function(){return H}}),ea=d3.map({"in":a,out:k,"in-out":E,"out-in":function(t){return E(k(t))}});d3.ease=function(t){var n=t.indexOf("-"),e=n>=0?t.substring(0,n):t,r=n>=0?t.substring(n+1):"in";return e=na.get(e)||ta,r=ea.get(r)||a,S(r(e.apply(null,Array.prototype.slice.call(arguments,1))))},d3.event=null,d3.transform=function(t){var n=document.createElementNS(d3.ns.prefix.svg,"g");return(d3.transform=function(t){n.setAttribute("transform",t);var e=n.transform.baseVal.consolidate();return new O(e?e.matrix:ra)})(t)},O.prototype.toString=function(){return"translate("+this.translate+")rotate("+this.rotate+")skewX("+this.skew+")scale("+this.scale+")"};var ra={a:1,b:0,c:0,d:1,e:0,f:0};d3.interpolate=function(t,n){for(var e,r=d3.interpolators.length;--r>=0&&!(e=d3.interpolators[r](t,n)););return e},d3.interpolateNumber=function(t,n){return n-=t,function(e){return t+n*e}},d3.interpolateRound=function(t,n){return n-=t,function(e){return Math.round(t+n*e)}},d3.interpolateString=function(t,n){var e,r,u,i,a,o=0,c=0,l=[],s=[];for(ua.lastIndex=0,r=0;e=ua.exec(n);++r)e.index&&l.push(n.substring(o,c=e.index)),s.push({i:l.length,x:e[0]}),l.push(null),o=ua.lastIndex;for(n.length>o&&l.push(n.substring(o)),r=0,i=s.length;(e=ua.exec(t))&&i>r;++r)if(a=s[r],a.x==e[0]){if(a.i)if(null==l[a.i+1])for(l[a.i-1]+=a.x,l.splice(a.i,1),u=r+1;i>u;++u)s[u].i--;else for(l[a.i-1]+=a.x+l[a.i+1],l.splice(a.i,2),u=r+1;i>u;++u)s[u].i-=2;else if(null==l[a.i+1])l[a.i]=a.x;else for(l[a.i]=a.x+l[a.i+1],l.splice(a.i+1,1),u=r+1;i>u;++u)s[u].i--;s.splice(r,1),i--,r--}else a.x=d3.interpolateNumber(parseFloat(e[0]),parseFloat(a.x));for(;i>r;)a=s.pop(),null==l[a.i+1]?l[a.i]=a.x:(l[a.i]=a.x+l[a.i+1],l.splice(a.i+1,1)),i--;return 1===l.length?null==l[0]?s[0].x:function(){return n}:function(t){for(r=0;i>r;++r)l[(a=s[r]).i]=a.x(t);return l.join("")}},d3.interpolateTransform=function(t,n){var e,r=[],u=[],i=d3.transform(t),a=d3.transform(n),o=i.translate,c=a.translate,l=i.rotate,s=a.rotate,f=i.skew,h=a.skew,d=i.scale,g=a.scale;return o[0]!=c[0]||o[1]!=c[1]?(r.push("translate(",null,",",null,")"),u.push({i:1,x:d3.interpolateNumber(o[0],c[0])},{i:3,x:d3.interpolateNumber(o[1],c[1])})):c[0]||c[1]?r.push("translate("+c+")"):r.push(""),l!=s?(l-s>180?s+=360:s-l>180&&(l+=360),u.push({i:r.push(r.pop()+"rotate(",null,")")-2,x:d3.interpolateNumber(l,s)})):s&&r.push(r.pop()+"rotate("+s+")"),f!=h?u.push({i:r.push(r.pop()+"skewX(",null,")")-2,x:d3.interpolateNumber(f,h)}):h&&r.push(r.pop()+"skewX("+h+")"),d[0]!=g[0]||d[1]!=g[1]?(e=r.push(r.pop()+"scale(",null,",",null,")"),u.push({i:e-4,x:d3.interpolateNumber(d[0],g[0])},{i:e-2,x:d3.interpolateNumber(d[1],g[1])})):(1!=g[0]||1!=g[1])&&r.push(r.pop()+"scale("+g+")"),e=u.length,function(t){for(var n,i=-1;e>++i;)r[(n=u[i]).i]=n.x(t);return r.join("")}},d3.interpolateRgb=function(t,n){t=d3.rgb(t),n=d3.rgb(n);var e=t.r,r=t.g,u=t.b,i=n.r-e,a=n.g-r,o=n.b-u;return function(t){return"#"+G(Math.round(e+i*t))+G(Math.round(r+a*t))+G(Math.round(u+o*t))}},d3.interpolateHsl=function(t,n){t=d3.hsl(t),n=d3.hsl(n);var e=t.h,r=t.s,u=t.l,i=n.h-e,a=n.s-r,o=n.l-u;return i>180?i-=360:-180>i&&(i+=360),function(t){return un(e+i*t,r+a*t,u+o*t)+""}},d3.interpolateLab=function(t,n){t=d3.lab(t),n=d3.lab(n);var e=t.l,r=t.a,u=t.b,i=n.l-e,a=n.a-r,o=n.b-u;return function(t){return fn(e+i*t,r+a*t,u+o*t)+""}},d3.interpolateHcl=function(t,n){t=d3.hcl(t),n=d3.hcl(n);var e=t.h,r=t.c,u=t.l,i=n.h-e,a=n.c-r,o=n.l-u;return i>180?i-=360:-180>i&&(i+=360),function(t){return cn(e+i*t,r+a*t,u+o*t)+""}},d3.interpolateArray=function(t,n){var e,r=[],u=[],i=t.length,a=n.length,o=Math.min(t.length,n.length);for(e=0;o>e;++e)r.push(d3.interpolate(t[e],n[e]));for(;i>e;++e)u[e]=t[e];for(;a>e;++e)u[e]=n[e];return function(t){for(e=0;o>e;++e)u[e]=r[e](t);return u}},d3.interpolateObject=function(t,n){var e,r={},u={};for(e in t)e in n?r[e]=V(e)(t[e],n[e]):u[e]=t[e];for(e in n)e in t||(u[e]=n[e]);return function(t){for(e in r)u[e]=r[e](t);return u}};var ua=/[-+]?(?:\d+\.?\d*|\.?\d+)(?:[eE][-+]?\d+)?/g;d3.interpolators=[d3.interpolateObject,function(t,n){return n instanceof Array&&d3.interpolateArray(t,n)},function(t,n){return("string"==typeof t||"string"==typeof n)&&d3.interpolateString(t+"",n+"")},function(t,n){return("string"==typeof n?aa.has(n)||/^(#|rgb\(|hsl\()/.test(n):n instanceof B)&&d3.interpolateRgb(t,n)},function(t,n){return!isNaN(t=+t)&&!isNaN(n=+n)&&d3.interpolateNumber(t,n)}],B.prototype.toString=function(){return this.rgb()+""},d3.rgb=function(t,n,e){return 1===arguments.length?t instanceof J?$(t.r,t.g,t.b):K(""+t,$,un):$(~~t,~~n,~~e)};var ia=J.prototype=new B;ia.brighter=function(t){t=Math.pow(.7,arguments.length?t:1);var n=this.r,e=this.g,r=this.b,u=30;return n||e||r?(n&&u>n&&(n=u),e&&u>e&&(e=u),r&&u>r&&(r=u),$(Math.min(255,Math.floor(n/t)),Math.min(255,Math.floor(e/t)),Math.min(255,Math.floor(r/t)))):$(u,u,u)},ia.darker=function(t){return t=Math.pow(.7,arguments.length?t:1),$(Math.floor(t*this.r),Math.floor(t*this.g),Math.floor(t*this.b))
},ia.hsl=function(){return W(this.r,this.g,this.b)},ia.toString=function(){return"#"+G(this.r)+G(this.g)+G(this.b)};var aa=d3.map({aliceblue:"#f0f8ff",antiquewhite:"#faebd7",aqua:"#00ffff",aquamarine:"#7fffd4",azure:"#f0ffff",beige:"#f5f5dc",bisque:"#ffe4c4",black:"#000000",blanchedalmond:"#ffebcd",blue:"#0000ff",blueviolet:"#8a2be2",brown:"#a52a2a",burlywood:"#deb887",cadetblue:"#5f9ea0",chartreuse:"#7fff00",chocolate:"#d2691e",coral:"#ff7f50",cornflowerblue:"#6495ed",cornsilk:"#fff8dc",crimson:"#dc143c",cyan:"#00ffff",darkblue:"#00008b",darkcyan:"#008b8b",darkgoldenrod:"#b8860b",darkgray:"#a9a9a9",darkgreen:"#006400",darkgrey:"#a9a9a9",darkkhaki:"#bdb76b",darkmagenta:"#8b008b",darkolivegreen:"#556b2f",darkorange:"#ff8c00",darkorchid:"#9932cc",darkred:"#8b0000",darksalmon:"#e9967a",darkseagreen:"#8fbc8f",darkslateblue:"#483d8b",darkslategray:"#2f4f4f",darkslategrey:"#2f4f4f",darkturquoise:"#00ced1",darkviolet:"#9400d3",deeppink:"#ff1493",deepskyblue:"#00bfff",dimgray:"#696969",dimgrey:"#696969",dodgerblue:"#1e90ff",firebrick:"#b22222",floralwhite:"#fffaf0",forestgreen:"#228b22",fuchsia:"#ff00ff",gainsboro:"#dcdcdc",ghostwhite:"#f8f8ff",gold:"#ffd700",goldenrod:"#daa520",gray:"#808080",green:"#008000",greenyellow:"#adff2f",grey:"#808080",honeydew:"#f0fff0",hotpink:"#ff69b4",indianred:"#cd5c5c",indigo:"#4b0082",ivory:"#fffff0",khaki:"#f0e68c",lavender:"#e6e6fa",lavenderblush:"#fff0f5",lawngreen:"#7cfc00",lemonchiffon:"#fffacd",lightblue:"#add8e6",lightcoral:"#f08080",lightcyan:"#e0ffff",lightgoldenrodyellow:"#fafad2",lightgray:"#d3d3d3",lightgreen:"#90ee90",lightgrey:"#d3d3d3",lightpink:"#ffb6c1",lightsalmon:"#ffa07a",lightseagreen:"#20b2aa",lightskyblue:"#87cefa",lightslategray:"#778899",lightslategrey:"#778899",lightsteelblue:"#b0c4de",lightyellow:"#ffffe0",lime:"#00ff00",limegreen:"#32cd32",linen:"#faf0e6",magenta:"#ff00ff",maroon:"#800000",mediumaquamarine:"#66cdaa",mediumblue:"#0000cd",mediumorchid:"#ba55d3",mediumpurple:"#9370db",mediumseagreen:"#3cb371",mediumslateblue:"#7b68ee",mediumspringgreen:"#00fa9a",mediumturquoise:"#48d1cc",mediumvioletred:"#c71585",midnightblue:"#191970",mintcream:"#f5fffa",mistyrose:"#ffe4e1",moccasin:"#ffe4b5",navajowhite:"#ffdead",navy:"#000080",oldlace:"#fdf5e6",olive:"#808000",olivedrab:"#6b8e23",orange:"#ffa500",orangered:"#ff4500",orchid:"#da70d6",palegoldenrod:"#eee8aa",palegreen:"#98fb98",paleturquoise:"#afeeee",palevioletred:"#db7093",papayawhip:"#ffefd5",peachpuff:"#ffdab9",peru:"#cd853f",pink:"#ffc0cb",plum:"#dda0dd",powderblue:"#b0e0e6",purple:"#800080",red:"#ff0000",rosybrown:"#bc8f8f",royalblue:"#4169e1",saddlebrown:"#8b4513",salmon:"#fa8072",sandybrown:"#f4a460",seagreen:"#2e8b57",seashell:"#fff5ee",sienna:"#a0522d",silver:"#c0c0c0",skyblue:"#87ceeb",slateblue:"#6a5acd",slategray:"#708090",slategrey:"#708090",snow:"#fffafa",springgreen:"#00ff7f",steelblue:"#4682b4",tan:"#d2b48c",teal:"#008080",thistle:"#d8bfd8",tomato:"#ff6347",turquoise:"#40e0d0",violet:"#ee82ee",wheat:"#f5deb3",white:"#ffffff",whitesmoke:"#f5f5f5",yellow:"#ffff00",yellowgreen:"#9acd32"});aa.forEach(function(t,n){aa.set(t,K(n,$,un))}),d3.hsl=function(t,n,e){return 1===arguments.length?t instanceof rn?en(t.h,t.s,t.l):K(""+t,W,en):en(+t,+n,+e)};var oa=rn.prototype=new B;oa.brighter=function(t){return t=Math.pow(.7,arguments.length?t:1),en(this.h,this.s,this.l/t)},oa.darker=function(t){return t=Math.pow(.7,arguments.length?t:1),en(this.h,this.s,t*this.l)},oa.rgb=function(){return un(this.h,this.s,this.l)},d3.hcl=function(t,n,e){return 1===arguments.length?t instanceof on?an(t.h,t.c,t.l):t instanceof sn?hn(t.l,t.a,t.b):hn((t=Q((t=d3.rgb(t)).r,t.g,t.b)).l,t.a,t.b):an(+t,+n,+e)};var ca=on.prototype=new B;ca.brighter=function(t){return an(this.h,this.c,Math.min(100,this.l+la*(arguments.length?t:1)))},ca.darker=function(t){return an(this.h,this.c,Math.max(0,this.l-la*(arguments.length?t:1)))},ca.rgb=function(){return cn(this.h,this.c,this.l).rgb()},d3.lab=function(t,n,e){return 1===arguments.length?t instanceof sn?ln(t.l,t.a,t.b):t instanceof on?cn(t.l,t.c,t.h):Q((t=d3.rgb(t)).r,t.g,t.b):ln(+t,+n,+e)};var la=18,sa=.95047,fa=1,ha=1.08883,da=sn.prototype=new B;da.brighter=function(t){return ln(Math.min(100,this.l+la*(arguments.length?t:1)),this.a,this.b)},da.darker=function(t){return ln(Math.max(0,this.l-la*(arguments.length?t:1)),this.a,this.b)},da.rgb=function(){return fn(this.l,this.a,this.b)};var ga=function(t,n){return n.querySelector(t)},pa=function(t,n){return n.querySelectorAll(t)},ma=document.documentElement,va=ma.matchesSelector||ma.webkitMatchesSelector||ma.mozMatchesSelector||ma.msMatchesSelector||ma.oMatchesSelector,ya=function(t,n){return va.call(t,n)};"function"==typeof Sizzle&&(ga=function(t,n){return Sizzle(t,n)[0]||null},pa=function(t,n){return Sizzle.uniqueSort(Sizzle(t,n))},ya=Sizzle.matchesSelector);var Ma=[];d3.selection=function(){return ba},d3.selection.prototype=Ma,Ma.select=function(t){var n,e,r,u,i=[];"function"!=typeof t&&(t=vn(t));for(var a=-1,o=this.length;o>++a;){i.push(n=[]),n.parentNode=(r=this[a]).parentNode;for(var c=-1,l=r.length;l>++c;)(u=r[c])?(n.push(e=t.call(u,u.__data__,c)),e&&"__data__"in u&&(e.__data__=u.__data__)):n.push(null)}return mn(i)},Ma.selectAll=function(t){var n,e,r=[];"function"!=typeof t&&(t=yn(t));for(var u=-1,i=this.length;i>++u;)for(var a=this[u],o=-1,c=a.length;c>++o;)(e=a[o])&&(r.push(n=Yi(t.call(e,e.__data__,o))),n.parentNode=e);return mn(r)},Ma.attr=function(t,n){if(2>arguments.length){if("string"==typeof t){var e=this.node();return t=d3.ns.qualify(t),t.local?e.getAttributeNS(t.space,t.local):e.getAttribute(t)}for(n in t)this.each(Mn(n,t[n]));return this}return this.each(Mn(t,n))},Ma.classed=function(t,n){if(2>arguments.length){if("string"==typeof t){var e=this.node(),r=(t=t.trim().split(/^|\s+/g)).length,u=-1;if(n=e.classList){for(;r>++u;)if(!n.contains(t[u]))return!1}else for(n=e.className,null!=n.baseVal&&(n=n.baseVal);r>++u;)if(!bn(t[u]).test(n))return!1;return!0}for(n in t)this.each(xn(n,t[n]));return this}return this.each(xn(t,n))},Ma.style=function(t,n,e){var r=arguments.length;if(3>r){if("string"!=typeof t){2>r&&(n="");for(e in t)this.each(wn(e,t[e],n));return this}if(2>r)return getComputedStyle(this.node(),null).getPropertyValue(t);e=""}return this.each(wn(t,n,e))},Ma.property=function(t,n){if(2>arguments.length){if("string"==typeof t)return this.node()[t];for(n in t)this.each(Sn(n,t[n]));return this}return this.each(Sn(t,n))},Ma.text=function(t){return arguments.length?this.each("function"==typeof t?function(){var n=t.apply(this,arguments);this.textContent=null==n?"":n}:null==t?function(){this.textContent=""}:function(){this.textContent=t}):this.node().textContent},Ma.html=function(t){return arguments.length?this.each("function"==typeof t?function(){var n=t.apply(this,arguments);this.innerHTML=null==n?"":n}:null==t?function(){this.innerHTML=""}:function(){this.innerHTML=t}):this.node().innerHTML},Ma.append=function(t){function n(){return this.appendChild(document.createElementNS(this.namespaceURI,t))}function e(){return this.appendChild(document.createElementNS(t.space,t.local))}return t=d3.ns.qualify(t),this.select(t.local?e:n)},Ma.insert=function(t,n){function e(){return this.insertBefore(document.createElementNS(this.namespaceURI,t),ga(n,this))}function r(){return this.insertBefore(document.createElementNS(t.space,t.local),ga(n,this))}return t=d3.ns.qualify(t),this.select(t.local?r:e)},Ma.remove=function(){return this.each(function(){var t=this.parentNode;t&&t.removeChild(this)})},Ma.data=function(t,n){function e(t,e){var r,u,a,o=t.length,f=e.length,h=Math.min(o,f),d=Math.max(o,f),g=[],p=[],m=[];if(n){var v,y=new i,M=[],b=e.length;for(r=-1;o>++r;)v=n.call(u=t[r],u.__data__,r),y.has(v)?m[b++]=u:y.set(v,u),M.push(v);for(r=-1;f>++r;)v=n.call(e,a=e[r],r),y.has(v)?(g[r]=u=y.get(v),u.__data__=a,p[r]=m[r]=null):(p[r]=kn(a),g[r]=m[r]=null),y.remove(v);for(r=-1;o>++r;)y.has(M[r])&&(m[r]=t[r])}else{for(r=-1;h>++r;)u=t[r],a=e[r],u?(u.__data__=a,g[r]=u,p[r]=m[r]=null):(p[r]=kn(a),g[r]=m[r]=null);for(;f>r;++r)p[r]=kn(e[r]),g[r]=m[r]=null;for(;d>r;++r)m[r]=t[r],p[r]=g[r]=null}p.update=g,p.parentNode=g.parentNode=m.parentNode=t.parentNode,c.push(p),l.push(g),s.push(m)}var r,u,a=-1,o=this.length;if(!arguments.length){for(t=Array(o=(r=this[0]).length);o>++a;)(u=r[a])&&(t[a]=u.__data__);return t}var c=qn([]),l=mn([]),s=mn([]);if("function"==typeof t)for(;o>++a;)e(r=this[a],t.call(r,r.parentNode.__data__,a));else for(;o>++a;)e(r=this[a],t);return l.enter=function(){return c},l.exit=function(){return s},l},Ma.datum=function(t){return arguments.length?this.property("__data__",t):this.property("__data__")},Ma.filter=function(t){var n,e,r,u=[];"function"!=typeof t&&(t=En(t));for(var i=0,a=this.length;a>i;i++){u.push(n=[]),n.parentNode=(e=this[i]).parentNode;for(var o=0,c=e.length;c>o;o++)(r=e[o])&&t.call(r,r.__data__,o)&&n.push(r)}return mn(u)},Ma.order=function(){for(var t=-1,n=this.length;n>++t;)for(var e,r=this[t],u=r.length-1,i=r[u];--u>=0;)(e=r[u])&&(i&&i!==e.nextSibling&&i.parentNode.insertBefore(e,i),i=e);return this},Ma.sort=function(t){t=An.apply(this,arguments);for(var n=-1,e=this.length;e>++n;)this[n].sort(t);return this.order()},Ma.on=function(t,n,e){var r=arguments.length;if(3>r){if("string"!=typeof t){2>r&&(n=!1);for(e in t)this.each(Nn(e,t[e],n));return this}if(2>r)return(r=this.node()["__on"+t])&&r._;e=!1}return this.each(Nn(t,n,e))},Ma.each=function(t){return Tn(this,function(n,e,r){t.call(n,n.__data__,e,r)})},Ma.call=function(t){var n=Yi(arguments);return t.apply(n[0]=this,n),this},Ma.empty=function(){return!this.node()},Ma.node=function(){for(var t=0,n=this.length;n>t;t++)for(var e=this[t],r=0,u=e.length;u>r;r++){var i=e[r];if(i)return i}return null},Ma.transition=function(){var t,n,e=_a||++Sa,r=[],u=Object.create(ka);u.time=Date.now();for(var i=-1,a=this.length;a>++i;){r.push(t=[]);for(var o=this[i],c=-1,l=o.length;l>++c;)(n=o[c])&&zn(n,c,e,u),t.push(n)}return Cn(r,e)};var ba=mn([[document]]);ba[0].parentNode=ma,d3.select=function(t){return"string"==typeof t?ba.select(t):mn([[t]])},d3.selectAll=function(t){return"string"==typeof t?ba.selectAll(t):mn([Yi(t)])};var xa=[];d3.selection.enter=qn,d3.selection.enter.prototype=xa,xa.append=Ma.append,xa.insert=Ma.insert,xa.empty=Ma.empty,xa.node=Ma.node,xa.select=function(t){for(var n,e,r,u,i,a=[],o=-1,c=this.length;c>++o;){r=(u=this[o]).update,a.push(n=[]),n.parentNode=u.parentNode;for(var l=-1,s=u.length;s>++l;)(i=u[l])?(n.push(r[l]=e=t.call(u.parentNode,i.__data__,l)),e.__data__=i.__data__):n.push(null)}return mn(a)};var _a,wa=[],Sa=0,ka={ease:T,delay:0,duration:250};wa.call=Ma.call,wa.empty=Ma.empty,wa.node=Ma.node,d3.transition=function(t){return arguments.length?_a?t.transition():t:ba.transition()},d3.transition.prototype=wa,wa.select=function(t){var n,e,r,u=this.id,i=[];"function"!=typeof t&&(t=vn(t));for(var a=-1,o=this.length;o>++a;){i.push(n=[]);for(var c=this[a],l=-1,s=c.length;s>++l;)(r=c[l])&&(e=t.call(r,r.__data__,l))?("__data__"in r&&(e.__data__=r.__data__),zn(e,l,u,r.__transition__[u]),n.push(e)):n.push(null)}return Cn(i,u)},wa.selectAll=function(t){var n,e,r,u,i,a=this.id,o=[];"function"!=typeof t&&(t=yn(t));for(var c=-1,l=this.length;l>++c;)for(var s=this[c],f=-1,h=s.length;h>++f;)if(r=s[f]){i=r.__transition__[a],e=t.call(r,r.__data__,f),o.push(n=[]);for(var d=-1,g=e.length;g>++d;)zn(u=e[d],d,a,i),n.push(u)}return Cn(o,a)},wa.filter=function(t){var n,e,r,u=[];"function"!=typeof t&&(t=En(t));for(var i=0,a=this.length;a>i;i++){u.push(n=[]);for(var e=this[i],o=0,c=e.length;c>o;o++)(r=e[o])&&t.call(r,r.__data__,o)&&n.push(r)}return Cn(u,this.id,this.time).ease(this.ease())},wa.attr=function(t,n){function e(){this.removeAttribute(i)}function r(){this.removeAttributeNS(i.space,i.local)}if(2>arguments.length){for(n in t)this.attr(n,t[n]);return this}var u=V(t),i=d3.ns.qualify(t);return Ln(this,"attr."+t,n,function(t){function n(){var n,e=this.getAttribute(i);return e!==t&&(n=u(e,t),function(t){this.setAttribute(i,n(t))})}function a(){var n,e=this.getAttributeNS(i.space,i.local);return e!==t&&(n=u(e,t),function(t){this.setAttributeNS(i.space,i.local,n(t))})}return null==t?i.local?r:e:(t+="",i.local?a:n)})},wa.attrTween=function(t,n){function e(t,e){var r=n.call(this,t,e,this.getAttribute(u));return r&&function(t){this.setAttribute(u,r(t))}}function r(t,e){var r=n.call(this,t,e,this.getAttributeNS(u.space,u.local));return r&&function(t){this.setAttributeNS(u.space,u.local,r(t))}}var u=d3.ns.qualify(t);return this.tween("attr."+t,u.local?r:e)},wa.style=function(t,n,e){function r(){this.style.removeProperty(t)}var u=arguments.length;if(3>u){if("string"!=typeof t){2>u&&(n="");for(e in t)this.style(e,t[e],n);return this}e=""}var i=V(t);return Ln(this,"style."+t,n,function(n){function u(){var r,u=getComputedStyle(this,null).getPropertyValue(t);return u!==n&&(r=i(u,n),function(n){this.style.setProperty(t,r(n),e)})}return null==n?r:(n+="",u)})},wa.styleTween=function(t,n,e){return 3>arguments.length&&(e=""),this.tween("style."+t,function(r,u){var i=n.call(this,r,u,getComputedStyle(this,null).getPropertyValue(t));return i&&function(n){this.style.setProperty(t,i(n),e)}})},wa.text=function(t){return Ln(this,"text",t,Dn)},wa.remove=function(){return this.each("end.transition",function(){var t;!this.__transition__&&(t=this.parentNode)&&t.removeChild(this)})},wa.ease=function(t){var n=this.id;return 1>arguments.length?this.node().__transition__[n].ease:("function"!=typeof t&&(t=d3.ease.apply(d3,arguments)),Tn(this,function(e){e.__transition__[n].ease=t}))},wa.delay=function(t){var n=this.id;return Tn(this,"function"==typeof t?function(e,r,u){e.__transition__[n].delay=0|t.call(e,e.__data__,r,u)}:(t|=0,function(e){e.__transition__[n].delay=t}))},wa.duration=function(t){var n=this.id;return Tn(this,"function"==typeof t?function(e,r,u){e.__transition__[n].duration=Math.max(1,0|t.call(e,e.__data__,r,u))}:(t=Math.max(1,0|t),function(e){e.__transition__[n].duration=t}))},wa.each=function(t,n){var e=this.id;if(2>arguments.length){var r=ka,u=_a;_a=e,Tn(this,function(n,r,u){ka=n.__transition__[e],t.call(n,n.__data__,r,u)}),ka=r,_a=u}else Tn(this,function(r){r.__transition__[e].event.on(t,n)});return this},wa.transition=function(){for(var t,n,e,r,u=this.id,i=++Sa,a=[],o=0,c=this.length;c>o;o++){a.push(t=[]);for(var n=this[o],l=0,s=n.length;s>l;l++)(e=n[l])&&(r=Object.create(e.__transition__[u]),r.delay+=r.duration,zn(e,l,i,r)),t.push(e)}return Cn(a,i)},wa.tween=function(t,n){var e=this.id;return 2>arguments.length?this.node().__transition__[e].tween.get(t):Tn(this,null==n?function(n){n.__transition__[e].tween.remove(t)}:function(r){r.__transition__[e].tween.set(t,n)})};var Ea,Aa,Na=0,Ta={},qa=null;d3.timer=function(t,n,e){if(3>arguments.length){if(2>arguments.length)n=0;else if(!isFinite(n))return;e=Date.now()}var r=Ta[t.id];r&&r.callback===t?(r.then=e,r.delay=n):Ta[t.id=++Na]=qa={callback:t,then:e,delay:n,next:qa},Ea||(Aa=clearTimeout(Aa),Ea=1,Ca(Fn))},d3.timer.flush=function(){for(var t,n=Date.now(),e=qa;e;)t=n-e.then,e.delay||(e.flush=e.callback(t)),e=e.next;Hn()};var Ca=window.requestAnimationFrame||window.webkitRequestAnimationFrame||window.mozRequestAnimationFrame||window.oRequestAnimationFrame||window.msRequestAnimationFrame||function(t){setTimeout(t,17)};d3.mouse=function(t){return Rn(t,P())};var za=/WebKit/.test(navigator.userAgent)?-1:0;d3.touches=function(t,n){return 2>arguments.length&&(n=P().touches),n?Yi(n).map(function(n){var e=Rn(t,n);return e.identifier=n.identifier,e}):[]},d3.scale={},d3.scale.linear=function(){return In([0,1],[0,1],d3.interpolate,!1)},d3.scale.log=function(){return Kn(d3.scale.linear(),Wn)};var Da=d3.format(".0e");Wn.pow=function(t){return Math.pow(10,t)},Qn.pow=function(t){return-Math.pow(10,-t)},d3.scale.pow=function(){return te(d3.scale.linear(),1)},d3.scale.sqrt=function(){return d3.scale.pow().exponent(.5)},d3.scale.ordinal=function(){return ee([],{t:"range",a:[[]]})},d3.scale.category10=function(){return d3.scale.ordinal().range(La)},d3.scale.category20=function(){return d3.scale.ordinal().range(Fa)},d3.scale.category20b=function(){return d3.scale.ordinal().range(Ha)},d3.scale.category20c=function(){return d3.scale.ordinal().range(Ra)};var La=["#1f77b4","#ff7f0e","#2ca02c","#d62728","#9467bd","#8c564b","#e377c2","#7f7f7f","#bcbd22","#17becf"],Fa=["#1f77b4","#aec7e8","#ff7f0e","#ffbb78","#2ca02c","#98df8a","#d62728","#ff9896","#9467bd","#c5b0d5","#8c564b","#c49c94","#e377c2","#f7b6d2","#7f7f7f","#c7c7c7","#bcbd22","#dbdb8d","#17becf","#9edae5"],Ha=["#393b79","#5254a3","#6b6ecf","#9c9ede","#637939","#8ca252","#b5cf6b","#cedb9c","#8c6d31","#bd9e39","#e7ba52","#e7cb94","#843c39","#ad494a","#d6616b","#e7969c","#7b4173","#a55194","#ce6dbd","#de9ed6"],Ra=["#3182bd","#6baed6","#9ecae1","#c6dbef","#e6550d","#fd8d3c","#fdae6b","#fdd0a2","#31a354","#74c476","#a1d99b","#c7e9c0","#756bb1","#9e9ac8","#bcbddc","#dadaeb","#636363","#969696","#bdbdbd","#d9d9d9"];d3.scale.quantile=function(){return re([],[])},d3.scale.quantize=function(){return ue(0,1,[0,1])},d3.scale.threshold=function(){return ie([.5],[0,1])},d3.scale.identity=function(){return ae([0,1])},d3.svg={},d3.svg.arc=function(){function t(){var t=n.apply(this,arguments),i=e.apply(this,arguments),a=r.apply(this,arguments)+Pa,o=u.apply(this,arguments)+Pa,c=(a>o&&(c=a,a=o,o=c),o-a),l=Ri>c?"0":"1",s=Math.cos(a),f=Math.sin(a),h=Math.cos(o),d=Math.sin(o);return c>=ja?t?"M0,"+i+"A"+i+","+i+" 0 1,1 0,"+-i+"A"+i+","+i+" 0 1,1 0,"+i+"M0,"+t+"A"+t+","+t+" 0 1,0 0,"+-t+"A"+t+","+t+" 0 1,0 0,"+t+"Z":"M0,"+i+"A"+i+","+i+" 0 1,1 0,"+-i+"A"+i+","+i+" 0 1,1 0,"+i+"Z":t?"M"+i*s+","+i*f+"A"+i+","+i+" 0 "+l+",1 "+i*h+","+i*d+"L"+t*h+","+t*d+"A"+t+","+t+" 0 "+l+",0 "+t*s+","+t*f+"Z":"M"+i*s+","+i*f+"A"+i+","+i+" 0 "+l+",1 "+i*h+","+i*d+"L0,0"+"Z"}var n=oe,e=ce,r=le,u=se;return t.innerRadius=function(e){return arguments.length?(n=c(e),t):n},t.outerRadius=function(n){return arguments.length?(e=c(n),t):e},t.startAngle=function(n){return arguments.length?(r=c(n),t):r},t.endAngle=function(n){return arguments.length?(u=c(n),t):u},t.centroid=function(){var t=(n.apply(this,arguments)+e.apply(this,arguments))/2,i=(r.apply(this,arguments)+u.apply(this,arguments))/2+Pa;return[Math.cos(i)*t,Math.sin(i)*t]},t};var Pa=-Ri/2,ja=2*Ri-1e-6;d3.svg.line=function(){return fe(a)};var Oa=d3.map({linear:ge,"linear-closed":pe,"step-before":me,"step-after":ve,basis:we,"basis-open":Se,"basis-closed":ke,bundle:Ee,cardinal:be,"cardinal-open":ye,"cardinal-closed":Me,monotone:ze});Oa.forEach(function(t,n){n.key=t,n.closed=/-closed$/.test(t)});var Ya=[0,2/3,1/3,0],Ua=[0,1/3,2/3,0],Ia=[0,1/6,2/3,1/6];d3.svg.line.radial=function(){var t=fe(De);return t.radius=t.x,delete t.x,t.angle=t.y,delete t.y,t},me.reverse=ve,ve.reverse=me,d3.svg.area=function(){return Le(a)},d3.svg.area.radial=function(){var t=Le(De);return t.radius=t.x,delete t.x,t.innerRadius=t.x0,delete t.x0,t.outerRadius=t.x1,delete t.x1,t.angle=t.y,delete t.y,t.startAngle=t.y0,delete t.y0,t.endAngle=t.y1,delete t.y1,t},d3.svg.chord=function(){function e(t,n){var e=r(this,o,t,n),c=r(this,l,t,n);return"M"+e.p0+i(e.r,e.p1,e.a1-e.a0)+(u(e,c)?a(e.r,e.p1,e.r,e.p0):a(e.r,e.p1,c.r,c.p0)+i(c.r,c.p1,c.a1-c.a0)+a(c.r,c.p1,e.r,e.p0))+"Z"}function r(t,n,e,r){var u=n.call(t,e,r),i=s.call(t,u,r),a=f.call(t,u,r)+Pa,o=h.call(t,u,r)+Pa;return{r:i,a0:a,a1:o,p0:[i*Math.cos(a),i*Math.sin(a)],p1:[i*Math.cos(o),i*Math.sin(o)]}}function u(t,n){return t.a0==n.a0&&t.a1==n.a1}function i(t,n,e){return"A"+t+","+t+" 0 "+ +(e>Ri)+",1 "+n}function a(t,n,e,r){return"Q 0,0 "+r}var o=n,l=t,s=Fe,f=le,h=se;return e.radius=function(t){return arguments.length?(s=c(t),e):s},e.source=function(t){return arguments.length?(o=c(t),e):o},e.target=function(t){return arguments.length?(l=c(t),e):l},e.startAngle=function(t){return arguments.length?(f=c(t),e):f},e.endAngle=function(t){return arguments.length?(h=c(t),e):h},e},d3.svg.diagonal=function(){function e(t,n){var e=r.call(this,t,n),a=u.call(this,t,n),o=(e.y+a.y)/2,c=[e,{x:e.x,y:o},{x:a.x,y:o},a];return c=c.map(i),"M"+c[0]+"C"+c[1]+" "+c[2]+" "+c[3]}var r=n,u=t,i=He;return e.source=function(t){return arguments.length?(r=c(t),e):r},e.target=function(t){return arguments.length?(u=c(t),e):u},e.projection=function(t){return arguments.length?(i=t,e):i},e},d3.svg.diagonal.radial=function(){var t=d3.svg.diagonal(),n=He,e=t.projection;return t.projection=function(t){return arguments.length?e(Re(n=t)):n},t},d3.svg.symbol=function(){function t(t,r){return(Va.get(n.call(this,t,r))||Oe)(e.call(this,t,r))}var n=je,e=Pe;return t.type=function(e){return arguments.length?(n=c(e),t):n},t.size=function(n){return arguments.length?(e=c(n),t):e},t};var Va=d3.map({circle:Oe,cross:function(t){var n=Math.sqrt(t/5)/2;return"M"+-3*n+","+-n+"H"+-n+"V"+-3*n+"H"+n+"V"+-n+"H"+3*n+"V"+n+"H"+n+"V"+3*n+"H"+-n+"V"+n+"H"+-3*n+"Z"},diamond:function(t){var n=Math.sqrt(t/(2*Za)),e=n*Za;return"M0,"+-n+"L"+e+",0"+" 0,"+n+" "+-e+",0"+"Z"},square:function(t){var n=Math.sqrt(t)/2;return"M"+-n+","+-n+"L"+n+","+-n+" "+n+","+n+" "+-n+","+n+"Z"},"triangle-down":function(t){var n=Math.sqrt(t/Xa),e=n*Xa/2;return"M0,"+e+"L"+n+","+-e+" "+-n+","+-e+"Z"},"triangle-up":function(t){var n=Math.sqrt(t/Xa),e=n*Xa/2;return"M0,"+-e+"L"+n+","+e+" "+-n+","+e+"Z"}});d3.svg.symbolTypes=Va.keys();var Xa=Math.sqrt(3),Za=Math.tan(30*ji);d3.svg.axis=function(){function t(t){t.each(function(){var t,f=d3.select(this),h=null==l?e.ticks?e.ticks.apply(e,c):e.domain():l,d=null==n?e.tickFormat?e.tickFormat.apply(e,c):String:n,g=Ie(e,h,s),p=f.selectAll(".minor").data(g,String),m=p.enter().insert("line","g").attr("class","tick minor").style("opacity",1e-6),v=d3.transition(p.exit()).style("opacity",1e-6).remove(),y=d3.transition(p).style("opacity",1),M=f.selectAll("g").data(h,String),b=M.enter().insert("g","path").style("opacity",1e-6),x=d3.transition(M.exit()).style("opacity",1e-6).remove(),_=d3.transition(M).style("opacity",1),w=On(e),S=f.selectAll(".domain").data([0]),k=d3.transition(S),E=e.copy(),A=this.__chart__||E;this.__chart__=E,S.enter().append("path").attr("class","domain"),b.append("line").attr("class","tick"),b.append("text");var N=b.select("line"),T=_.select("line"),q=M.select("text").text(d),C=b.select("text"),z=_.select("text");switch(r){case"bottom":t=Ye,m.attr("y2",i),y.attr("x2",0).attr("y2",i),N.attr("y2",u),C.attr("y",Math.max(u,0)+o),T.attr("x2",0).attr("y2",u),z.attr("x",0).attr("y",Math.max(u,0)+o),q.attr("dy",".71em").style("text-anchor","middle"),k.attr("d","M"+w[0]+","+a+"V0H"+w[1]+"V"+a);break;case"top":t=Ye,m.attr("y2",-i),y.attr("x2",0).attr("y2",-i),N.attr("y2",-u),C.attr("y",-(Math.max(u,0)+o)),T.attr("x2",0).attr("y2",-u),z.attr("x",0).attr("y",-(Math.max(u,0)+o)),q.attr("dy","0em").style("text-anchor","middle"),k.attr("d","M"+w[0]+","+-a+"V0H"+w[1]+"V"+-a);break;case"left":t=Ue,m.attr("x2",-i),y.attr("x2",-i).attr("y2",0),N.attr("x2",-u),C.attr("x",-(Math.max(u,0)+o)),T.attr("x2",-u).attr("y2",0),z.attr("x",-(Math.max(u,0)+o)).attr("y",0),q.attr("dy",".32em").style("text-anchor","end"),k.attr("d","M"+-a+","+w[0]+"H0V"+w[1]+"H"+-a);break;case"right":t=Ue,m.attr("x2",i),y.attr("x2",i).attr("y2",0),N.attr("x2",u),C.attr("x",Math.max(u,0)+o),T.attr("x2",u).attr("y2",0),z.attr("x",Math.max(u,0)+o).attr("y",0),q.attr("dy",".32em").style("text-anchor","start"),k.attr("d","M"+a+","+w[0]+"H0V"+w[1]+"H"+a)}if(e.ticks)b.call(t,A),_.call(t,E),x.call(t,E),m.call(t,A),y.call(t,E),v.call(t,E);else{var D=E.rangeBand()/2,L=function(t){return E(t)+D};b.call(t,L),_.call(t,L)}})}var n,e=d3.scale.linear(),r="bottom",u=6,i=6,a=6,o=3,c=[10],l=null,s=0;return t.scale=function(n){return arguments.length?(e=n,t):e},t.orient=function(n){return arguments.length?(r=n,t):r},t.ticks=function(){return arguments.length?(c=arguments,t):c},t.tickValues=function(n){return arguments.length?(l=n,t):l},t.tickFormat=function(e){return arguments.length?(n=e,t):n},t.tickSize=function(n,e){if(!arguments.length)return u;var r=arguments.length-1;return u=+n,i=r>1?+e:u,a=r>0?+arguments[r]:u,t},t.tickPadding=function(n){return arguments.length?(o=+n,t):o},t.tickSubdivide=function(n){return arguments.length?(s=+n,t):s},t},d3.svg.brush=function(){function t(i){i.each(function(){var i,a=d3.select(this),s=a.selectAll(".background").data([0]),f=a.selectAll(".extent").data([0]),h=a.selectAll(".resize").data(l,String);a.style("pointer-events","all").on("mousedown.brush",u).on("touchstart.brush",u),s.enter().append("rect").attr("class","background").style("visibility","hidden").style("cursor","crosshair"),f.enter().append("rect").attr("class","extent").style("cursor","move"),h.enter().append("g").attr("class",function(t){return"resize "+t}).style("cursor",function(t){return Ba[t]}).append("rect").attr("x",function(t){return/[ew]$/.test(t)?-3:null}).attr("y",function(t){return/^[ns]/.test(t)?-3:null}).attr("width",6).attr("height",6).style("visibility","hidden"),h.style("display",t.empty()?"none":null),h.exit().remove(),o&&(i=On(o),s.attr("x",i[0]).attr("width",i[1]-i[0]),e(a)),c&&(i=On(c),s.attr("y",i[0]).attr("height",i[1]-i[0]),r(a)),n(a)})}function n(t){t.selectAll(".resize").attr("transform",function(t){return"translate("+s[+/e$/.test(t)][0]+","+s[+/^s/.test(t)][1]+")"})}function e(t){t.select(".extent").attr("x",s[0][0]),t.selectAll(".extent,.n>rect,.s>rect").attr("width",s[1][0]-s[0][0])}function r(t){t.select(".extent").attr("y",s[0][1]),t.selectAll(".extent,.e>rect,.w>rect").attr("height",s[1][1]-s[0][1])}function u(){function u(){var t=d3.event.changedTouches;return t?d3.touches(v,t)[0]:d3.mouse(v)}function l(){32==d3.event.keyCode&&(S||(p=null,k[0]-=s[1][0],k[1]-=s[1][1],S=2),R())}function f(){32==d3.event.keyCode&&2==S&&(k[0]+=s[1][0],k[1]+=s[1][1],S=0,R())}function h(){var t=u(),i=!1;m&&(t[0]+=m[0],t[1]+=m[1]),S||(d3.event.altKey?(p||(p=[(s[0][0]+s[1][0])/2,(s[0][1]+s[1][1])/2]),k[0]=s[+(t[0]<p[0])][0],k[1]=s[+(t[1]<p[1])][1]):p=null),_&&d(t,o,0)&&(e(b),i=!0),w&&d(t,c,1)&&(r(b),i=!0),i&&(n(b),M({type:"brush",mode:S?"move":"resize"}))}function d(t,n,e){var r,u,a=On(n),o=a[0],c=a[1],l=k[e],f=s[1][e]-s[0][e];return S&&(o-=l,c-=f+l),r=Math.max(o,Math.min(c,t[e])),S?u=(r+=l)+f:(p&&(l=Math.max(o,Math.min(c,2*p[e]-r))),r>l?(u=r,r=l):u=l),s[0][e]!==r||s[1][e]!==u?(i=null,s[0][e]=r,s[1][e]=u,!0):void 0}function g(){h(),b.style("pointer-events","all").selectAll(".resize").style("display",t.empty()?"none":null),d3.select("body").style("cursor",null),E.on("mousemove.brush",null).on("mouseup.brush",null).on("touchmove.brush",null).on("touchend.brush",null).on("keydown.brush",null).on("keyup.brush",null),M({type:"brushend"}),R()}var p,m,v=this,y=d3.select(d3.event.target),M=a.of(v,arguments),b=d3.select(v),x=y.datum(),_=!/^(n|s)$/.test(x)&&o,w=!/^(e|w)$/.test(x)&&c,S=y.classed("extent"),k=u(),E=d3.select(window).on("mousemove.brush",h).on("mouseup.brush",g).on("touchmove.brush",h).on("touchend.brush",g).on("keydown.brush",l).on("keyup.brush",f);if(S)k[0]=s[0][0]-k[0],k[1]=s[0][1]-k[1];else if(x){var A=+/w$/.test(x),N=+/^n/.test(x);m=[s[1-A][0]-k[0],s[1-N][1]-k[1]],k[0]=s[A][0],k[1]=s[N][1]}else d3.event.altKey&&(p=k.slice());b.style("pointer-events","none").selectAll(".resize").style("display",null),d3.select("body").style("cursor",y.style("cursor")),M({type:"brushstart"}),h(),R()}var i,a=j(t,"brushstart","brush","brushend"),o=null,c=null,l=$a[0],s=[[0,0],[0,0]];return t.x=function(n){return arguments.length?(o=n,l=$a[!o<<1|!c],t):o},t.y=function(n){return arguments.length?(c=n,l=$a[!o<<1|!c],t):c},t.extent=function(n){var e,r,u,a,l;return arguments.length?(i=[[0,0],[0,0]],o&&(e=n[0],r=n[1],c&&(e=e[0],r=r[0]),i[0][0]=e,i[1][0]=r,o.invert&&(e=o(e),r=o(r)),e>r&&(l=e,e=r,r=l),s[0][0]=0|e,s[1][0]=0|r),c&&(u=n[0],a=n[1],o&&(u=u[1],a=a[1]),i[0][1]=u,i[1][1]=a,c.invert&&(u=c(u),a=c(a)),u>a&&(l=u,u=a,a=l),s[0][1]=0|u,s[1][1]=0|a),t):(n=i||s,o&&(e=n[0][0],r=n[1][0],i||(e=s[0][0],r=s[1][0],o.invert&&(e=o.invert(e),r=o.invert(r)),e>r&&(l=e,e=r,r=l))),c&&(u=n[0][1],a=n[1][1],i||(u=s[0][1],a=s[1][1],c.invert&&(u=c.invert(u),a=c.invert(a)),u>a&&(l=u,u=a,a=l))),o&&c?[[e,u],[r,a]]:o?[e,r]:c&&[u,a])},t.clear=function(){return i=null,s[0][0]=s[0][1]=s[1][0]=s[1][1]=0,t},t.empty=function(){return o&&s[0][0]===s[1][0]||c&&s[0][1]===s[1][1]},d3.rebind(t,a,"on")};var Ba={n:"ns-resize",e:"ew-resize",s:"ns-resize",w:"ew-resize",nw:"nwse-resize",ne:"nesw-resize",se:"nwse-resize",sw:"nesw-resize"},$a=[["n","e","s","w","nw","ne","se","sw"],["e","w"],["n","s"],[]];d3.behavior={},d3.behavior.drag=function(){function t(){this.on("mousedown.drag",n).on("touchstart.drag",n)}function n(){function t(){var t=o.parentNode;return null!=s?d3.touches(t).filter(function(t){return t.identifier===s})[0]:d3.mouse(t)}function n(){if(!o.parentNode)return u();var n=t(),e=n[0]-f[0],r=n[1]-f[1];h|=e|r,f=n,R(),c({type:"drag",x:n[0]+a[0],y:n[1]+a[1],dx:e,dy:r})}function u(){c({type:"dragend"}),h&&(R(),d3.event.target===l&&d.on("click.drag",i,!0)),d.on(null!=s?"touchmove.drag-"+s:"mousemove.drag",null).on(null!=s?"touchend.drag-"+s:"mouseup.drag",null)}function i(){R(),d.on("click.drag",null)}var a,o=this,c=e.of(o,arguments),l=d3.event.target,s=d3.event.touches?d3.event.changedTouches[0].identifier:null,f=t(),h=0,d=d3.select(window).on(null!=s?"touchmove.drag-"+s:"mousemove.drag",n).on(null!=s?"touchend.drag-"+s:"mouseup.drag",u,!0);r?(a=r.apply(o,arguments),a=[a.x-f[0],a.y-f[1]]):a=[0,0],null==s&&R(),c({type:"dragstart"})}var e=j(t,"drag","dragstart","dragend"),r=null;return t.origin=function(n){return arguments.length?(r=n,t):r},d3.rebind(t,e,"on")},d3.behavior.zoom=function(){function t(){this.on("mousedown.zoom",o).on("mousewheel.zoom",c).on("mousemove.zoom",l).on("DOMMouseScroll.zoom",c).on("dblclick.zoom",s).on("touchstart.zoom",f).on("touchmove.zoom",h).on("touchend.zoom",f)}function n(t){return[(t[0]-b[0])/x,(t[1]-b[1])/x]}function e(t){return[t[0]*x+b[0],t[1]*x+b[1]]}function r(t){x=Math.max(_[0],Math.min(_[1],t))}function u(t,n){n=e(n),b[0]+=t[0]-n[0],b[1]+=t[1]-n[1]}function i(){m&&m.domain(p.range().map(function(t){return(t-b[0])/x}).map(p.invert)),y&&y.domain(v.range().map(function(t){return(t-b[1])/x}).map(v.invert))}function a(t){i(),d3.event.preventDefault(),t({type:"zoom",scale:x,translate:b})}function o(){function t(){l=1,u(d3.mouse(i),f),a(o)}function e(){l&&R(),s.on("mousemove.zoom",null).on("mouseup.zoom",null),l&&d3.event.target===c&&s.on("click.zoom",r,!0)}function r(){R(),s.on("click.zoom",null)}var i=this,o=w.of(i,arguments),c=d3.event.target,l=0,s=d3.select(window).on("mousemove.zoom",t).on("mouseup.zoom",e),f=n(d3.mouse(i));window.focus(),R()}function c(){d||(d=n(d3.mouse(this))),r(Math.pow(2,.002*Ve())*x),u(d3.mouse(this),d),a(w.of(this,arguments))}function l(){d=null}function s(){var t=d3.mouse(this),e=n(t),i=Math.log(x)/Math.LN2;r(Math.pow(2,d3.event.shiftKey?Math.ceil(i)-1:Math.floor(i)+1)),u(t,e),a(w.of(this,arguments))}function f(){var t=d3.touches(this),e=Date.now();if(g=x,d={},t.forEach(function(t){d[t.identifier]=n(t)}),R(),1===t.length){if(500>e-M){var i=t[0],o=n(t[0]);r(2*x),u(i,o),a(w.of(this,arguments))}M=e}}function h(){var t=d3.touches(this),n=t[0],e=d[n.identifier];if(i=t[1]){var i,o=d[i.identifier];n=[(n[0]+i[0])/2,(n[1]+i[1])/2],e=[(e[0]+o[0])/2,(e[1]+o[1])/2],r(d3.event.scale*g)}u(n,e),M=null,a(w.of(this,arguments))}var d,g,p,m,v,y,M,b=[0,0],x=1,_=Ga,w=j(t,"zoom");return t.translate=function(n){return arguments.length?(b=n.map(Number),i(),t):b},t.scale=function(n){return arguments.length?(x=+n,i(),t):x},t.scaleExtent=function(n){return arguments.length?(_=null==n?Ga:n.map(Number),t):_},t.x=function(n){return arguments.length?(m=n,p=n.copy(),b=[0,0],x=1,t):m},t.y=function(n){return arguments.length?(y=n,v=n.copy(),b=[0,0],x=1,t):y},d3.rebind(t,w,"on")};var Ja,Ga=[0,1/0];d3.layout={},d3.layout.bundle=function(){return function(t){for(var n=[],e=-1,r=t.length;r>++e;)n.push(Xe(t[e]));return n}},d3.layout.chord=function(){function t(){var t,l,f,h,d,g={},p=[],m=d3.range(i),v=[];for(e=[],r=[],t=0,h=-1;i>++h;){for(l=0,d=-1;i>++d;)l+=u[h][d];p.push(l),v.push(d3.range(i)),t+=l}for(a&&m.sort(function(t,n){return a(p[t],p[n])
}),o&&v.forEach(function(t,n){t.sort(function(t,e){return o(u[n][t],u[n][e])})}),t=(2*Ri-s*i)/t,l=0,h=-1;i>++h;){for(f=l,d=-1;i>++d;){var y=m[h],M=v[y][d],b=u[y][M],x=l,_=l+=b*t;g[y+"-"+M]={index:y,subindex:M,startAngle:x,endAngle:_,value:b}}r[y]={index:y,startAngle:f,endAngle:l,value:(l-f)/t},l+=s}for(h=-1;i>++h;)for(d=h-1;i>++d;){var w=g[h+"-"+d],S=g[d+"-"+h];(w.value||S.value)&&e.push(w.value<S.value?{source:S,target:w}:{source:w,target:S})}c&&n()}function n(){e.sort(function(t,n){return c((t.source.value+t.target.value)/2,(n.source.value+n.target.value)/2)})}var e,r,u,i,a,o,c,l={},s=0;return l.matrix=function(t){return arguments.length?(i=(u=t)&&u.length,e=r=null,l):u},l.padding=function(t){return arguments.length?(s=t,e=r=null,l):s},l.sortGroups=function(t){return arguments.length?(a=t,e=r=null,l):a},l.sortSubgroups=function(t){return arguments.length?(o=t,e=null,l):o},l.sortChords=function(t){return arguments.length?(c=t,e&&n(),l):c},l.chords=function(){return e||t(),e},l.groups=function(){return r||t(),r},l},d3.layout.force=function(){function t(t){return function(n,e,r,u){if(n.point!==t){var i=n.cx-t.x,a=n.cy-t.y,o=1/Math.sqrt(i*i+a*a);if(v>(u-e)*o){var c=n.charge*o*o;return t.px-=i*c,t.py-=a*c,!0}if(n.point&&isFinite(o)){var c=n.pointCharge*o*o;t.px-=i*c,t.py-=a*c}}return!n.charge}}function n(t){t.px=d3.event.x,t.py=d3.event.y,l.resume()}var e,r,u,i,o,l={},s=d3.dispatch("start","tick","end"),f=[1,1],h=.9,d=Qe,g=tr,p=-30,m=.1,v=.8,y=[],M=[];return l.tick=function(){if(.005>(r*=.99))return s.end({type:"end",alpha:r=0}),!0;var n,e,a,c,l,d,g,v,b,x=y.length,_=M.length;for(e=0;_>e;++e)a=M[e],c=a.source,l=a.target,v=l.x-c.x,b=l.y-c.y,(d=v*v+b*b)&&(d=r*i[e]*((d=Math.sqrt(d))-u[e])/d,v*=d,b*=d,l.x-=v*(g=c.weight/(l.weight+c.weight)),l.y-=b*g,c.x+=v*(g=1-g),c.y+=b*g);if((g=r*m)&&(v=f[0]/2,b=f[1]/2,e=-1,g))for(;x>++e;)a=y[e],a.x+=(v-a.x)*g,a.y+=(b-a.y)*g;if(p)for(We(n=d3.geom.quadtree(y),r,o),e=-1;x>++e;)(a=y[e]).fixed||n.visit(t(a));for(e=-1;x>++e;)a=y[e],a.fixed?(a.x=a.px,a.y=a.py):(a.x-=(a.px-(a.px=a.x))*h,a.y-=(a.py-(a.py=a.y))*h);s.tick({type:"tick",alpha:r})},l.nodes=function(t){return arguments.length?(y=t,l):y},l.links=function(t){return arguments.length?(M=t,l):M},l.size=function(t){return arguments.length?(f=t,l):f},l.linkDistance=function(t){return arguments.length?(d=c(t),l):d},l.distance=l.linkDistance,l.linkStrength=function(t){return arguments.length?(g=c(t),l):g},l.friction=function(t){return arguments.length?(h=t,l):h},l.charge=function(t){return arguments.length?(p="function"==typeof t?t:+t,l):p},l.gravity=function(t){return arguments.length?(m=t,l):m},l.theta=function(t){return arguments.length?(v=t,l):v},l.alpha=function(t){return arguments.length?(r?r=t>0?t:0:t>0&&(s.start({type:"start",alpha:r=t}),d3.timer(l.tick)),l):r},l.start=function(){function t(t,r){for(var u,i=n(e),a=-1,o=i.length;o>++a;)if(!isNaN(u=i[a][t]))return u;return Math.random()*r}function n(){if(!a){for(a=[],r=0;s>r;++r)a[r]=[];for(r=0;h>r;++r){var t=M[r];a[t.source.index].push(t.target),a[t.target.index].push(t.source)}}return a[e]}var e,r,a,c,s=y.length,h=M.length,m=f[0],v=f[1];for(e=0;s>e;++e)(c=y[e]).index=e,c.weight=0;for(u=[],i=[],e=0;h>e;++e)c=M[e],"number"==typeof c.source&&(c.source=y[c.source]),"number"==typeof c.target&&(c.target=y[c.target]),u[e]=d.call(this,c,e),i[e]=g.call(this,c,e),++c.source.weight,++c.target.weight;for(e=0;s>e;++e)c=y[e],isNaN(c.x)&&(c.x=t("x",m)),isNaN(c.y)&&(c.y=t("y",v)),isNaN(c.px)&&(c.px=c.x),isNaN(c.py)&&(c.py=c.y);if(o=[],"function"==typeof p)for(e=0;s>e;++e)o[e]=+p.call(this,y[e],e);else for(e=0;s>e;++e)o[e]=p;return l.resume()},l.resume=function(){return l.alpha(.1)},l.stop=function(){return l.alpha(0)},l.drag=function(){e||(e=d3.behavior.drag().origin(a).on("dragstart",$e).on("drag",n).on("dragend",Je)),this.on("mouseover.force",Ge).on("mouseout.force",Ke).call(e)},d3.rebind(l,s,"on")},d3.layout.partition=function(){function t(n,e,r,u){var i=n.children;if(n.x=e,n.y=n.depth*u,n.dx=r,n.dy=u,i&&(a=i.length)){var a,o,c,l=-1;for(r=n.value?r/n.value:0;a>++l;)t(o=i[l],e,c=o.value*r,u),e+=c}}function n(t){var e=t.children,r=0;if(e&&(u=e.length))for(var u,i=-1;u>++i;)r=Math.max(r,n(e[i]));return 1+r}function e(e,i){var a=r.call(this,e,i);return t(a[0],0,u[0],u[1]/n(a[0])),a}var r=d3.layout.hierarchy(),u=[1,1];return e.size=function(t){return arguments.length?(u=t,e):u},hr(e,r)},d3.layout.pie=function(){function t(i){var a=i.map(function(e,r){return+n.call(t,e,r)}),o=+("function"==typeof r?r.apply(this,arguments):r),c=(("function"==typeof u?u.apply(this,arguments):u)-r)/d3.sum(a),l=d3.range(i.length);null!=e&&l.sort(e===Ka?function(t,n){return a[n]-a[t]}:function(t,n){return e(i[t],i[n])});var s=[];return l.forEach(function(t){var n;s[t]={data:i[t],value:n=a[t],startAngle:o,endAngle:o+=n*c}}),s}var n=Number,e=Ka,r=0,u=2*Ri;return t.value=function(e){return arguments.length?(n=e,t):n},t.sort=function(n){return arguments.length?(e=n,t):e},t.startAngle=function(n){return arguments.length?(r=n,t):r},t.endAngle=function(n){return arguments.length?(u=n,t):u},t};var Ka={};d3.layout.stack=function(){function t(a,c){var l=a.map(function(e,r){return n.call(t,e,r)}),s=l.map(function(n){return n.map(function(n,e){return[i.call(t,n,e),o.call(t,n,e)]})}),f=e.call(t,s,c);l=d3.permute(l,f),s=d3.permute(s,f);var h,d,g,p=r.call(t,s,c),m=l.length,v=l[0].length;for(d=0;v>d;++d)for(u.call(t,l[0][d],g=p[d],s[0][d][1]),h=1;m>h;++h)u.call(t,l[h][d],g+=s[h-1][d][1],s[h][d][1]);return a}var n=a,e=ur,r=ir,u=rr,i=nr,o=er;return t.values=function(e){return arguments.length?(n=e,t):n},t.order=function(n){return arguments.length?(e="function"==typeof n?n:Wa.get(n)||ur,t):e},t.offset=function(n){return arguments.length?(r="function"==typeof n?n:Qa.get(n)||ir,t):r},t.x=function(n){return arguments.length?(i=n,t):i},t.y=function(n){return arguments.length?(o=n,t):o},t.out=function(n){return arguments.length?(u=n,t):u},t};var Wa=d3.map({"inside-out":function(t){var n,e,r=t.length,u=t.map(ar),i=t.map(or),a=d3.range(r).sort(function(t,n){return u[t]-u[n]}),o=0,c=0,l=[],s=[];for(n=0;r>n;++n)e=a[n],c>o?(o+=i[e],l.push(e)):(c+=i[e],s.push(e));return s.reverse().concat(l)},reverse:function(t){return d3.range(t.length).reverse()},"default":ur}),Qa=d3.map({silhouette:function(t){var n,e,r,u=t.length,i=t[0].length,a=[],o=0,c=[];for(e=0;i>e;++e){for(n=0,r=0;u>n;n++)r+=t[n][e][1];r>o&&(o=r),a.push(r)}for(e=0;i>e;++e)c[e]=(o-a[e])/2;return c},wiggle:function(t){var n,e,r,u,i,a,o,c,l,s=t.length,f=t[0],h=f.length,d=[];for(d[0]=c=l=0,e=1;h>e;++e){for(n=0,u=0;s>n;++n)u+=t[n][e][1];for(n=0,i=0,o=f[e][0]-f[e-1][0];s>n;++n){for(r=0,a=(t[n][e][1]-t[n][e-1][1])/(2*o);n>r;++r)a+=(t[r][e][1]-t[r][e-1][1])/o;i+=a*t[n][e][1]}d[e]=c-=u?i/u*o:0,l>c&&(l=c)}for(e=0;h>e;++e)d[e]-=l;return d},expand:function(t){var n,e,r,u=t.length,i=t[0].length,a=1/u,o=[];for(e=0;i>e;++e){for(n=0,r=0;u>n;n++)r+=t[n][e][1];if(r)for(n=0;u>n;n++)t[n][e][1]/=r;else for(n=0;u>n;n++)t[n][e][1]=a}for(e=0;i>e;++e)o[e]=0;return o},zero:ir});d3.layout.histogram=function(){function t(t,i){for(var a,o,c=[],l=t.map(e,this),s=r.call(this,l,i),f=u.call(this,s,l,i),i=-1,h=l.length,d=f.length-1,g=n?1:1/h;d>++i;)a=c[i]=[],a.dx=f[i+1]-(a.x=f[i]),a.y=0;if(d>0)for(i=-1;h>++i;)o=l[i],o>=s[0]&&s[1]>=o&&(a=c[d3.bisect(f,o,1,d)-1],a.y+=g,a.push(t[i]));return c}var n=!0,e=Number,r=fr,u=lr;return t.value=function(n){return arguments.length?(e=n,t):e},t.range=function(n){return arguments.length?(r=c(n),t):r},t.bins=function(n){return arguments.length?(u="number"==typeof n?function(t){return sr(t,n)}:c(n),t):u},t.frequency=function(e){return arguments.length?(n=!!e,t):n},t},d3.layout.hierarchy=function(){function t(n,a,o){var c=u.call(e,n,a);if(n.depth=a,o.push(n),c&&(l=c.length)){for(var l,s,f=-1,h=n.children=[],d=0,g=a+1;l>++f;)s=t(c[f],g,o),s.parent=n,h.push(s),d+=s.value;r&&h.sort(r),i&&(n.value=d)}else i&&(n.value=+i.call(e,n,a)||0);return n}function n(t,r){var u=t.children,a=0;if(u&&(o=u.length))for(var o,c=-1,l=r+1;o>++c;)a+=n(u[c],l);else i&&(a=+i.call(e,t,r)||0);return i&&(t.value=a),a}function e(n){var e=[];return t(n,0,e),e}var r=pr,u=dr,i=gr;return e.sort=function(t){return arguments.length?(r=t,e):r},e.children=function(t){return arguments.length?(u=t,e):u},e.value=function(t){return arguments.length?(i=t,e):i},e.revalue=function(t){return n(t,0),t},e},d3.layout.pack=function(){function t(t,u){var i=n.call(this,t,u),a=i[0];a.x=0,a.y=0,Rr(a,function(t){t.r=Math.sqrt(t.value)}),Rr(a,xr);var o=r[0],c=r[1],l=Math.max(2*a.r/o,2*a.r/c);if(e>0){var s=e*l/2;Rr(a,function(t){t.r+=s}),Rr(a,xr),Rr(a,function(t){t.r-=s}),l=Math.max(2*a.r/o,2*a.r/c)}return Sr(a,o/2,c/2,1/l),i}var n=d3.layout.hierarchy().sort(vr),e=0,r=[1,1];return t.size=function(n){return arguments.length?(r=n,t):r},t.padding=function(n){return arguments.length?(e=+n,t):e},hr(t,n)},d3.layout.cluster=function(){function t(t,u){var i,a=n.call(this,t,u),o=a[0],c=0;Rr(o,function(t){var n=t.children;n&&n.length?(t.x=Ar(n),t.y=Er(n)):(t.x=i?c+=e(t,i):0,t.y=0,i=t)});var l=Nr(o),s=Tr(o),f=l.x-e(l,s)/2,h=s.x+e(s,l)/2;return Rr(o,function(t){t.x=(t.x-f)/(h-f)*r[0],t.y=(1-(o.y?t.y/o.y:1))*r[1]}),a}var n=d3.layout.hierarchy().sort(null).value(null),e=qr,r=[1,1];return t.separation=function(n){return arguments.length?(e=n,t):e},t.size=function(n){return arguments.length?(r=n,t):r},hr(t,n)},d3.layout.tree=function(){function t(t,u){function i(t,n){var r=t.children,u=t._tree;if(r&&(a=r.length)){for(var a,c,l,s=r[0],f=s,h=-1;a>++h;)l=r[h],i(l,c),f=o(l,c,f),c=l;Pr(t);var d=.5*(s._tree.prelim+l._tree.prelim);n?(u.prelim=n._tree.prelim+e(t,n),u.mod=u.prelim-d):u.prelim=d}else n&&(u.prelim=n._tree.prelim+e(t,n))}function a(t,n){t.x=t._tree.prelim+n;var e=t.children;if(e&&(r=e.length)){var r,u=-1;for(n+=t._tree.mod;r>++u;)a(e[u],n)}}function o(t,n,r){if(n){for(var u,i=t,a=t,o=n,c=t.parent.children[0],l=i._tree.mod,s=a._tree.mod,f=o._tree.mod,h=c._tree.mod;o=zr(o),i=Cr(i),o&&i;)c=Cr(c),a=zr(a),a._tree.ancestor=t,u=o._tree.prelim+f-i._tree.prelim-l+e(o,i),u>0&&(jr(Or(o,t,r),t,u),l+=u,s+=u),f+=o._tree.mod,l+=i._tree.mod,h+=c._tree.mod,s+=a._tree.mod;o&&!zr(a)&&(a._tree.thread=o,a._tree.mod+=f-s),i&&!Cr(c)&&(c._tree.thread=i,c._tree.mod+=l-h,r=t)}return r}var c=n.call(this,t,u),l=c[0];Rr(l,function(t,n){t._tree={ancestor:t,prelim:0,mod:0,change:0,shift:0,number:n?n._tree.number+1:0}}),i(l),a(l,-l._tree.prelim);var s=Dr(l,Fr),f=Dr(l,Lr),h=Dr(l,Hr),d=s.x-e(s,f)/2,g=f.x+e(f,s)/2,p=h.depth||1;return Rr(l,function(t){t.x=(t.x-d)/(g-d)*r[0],t.y=t.depth/p*r[1],delete t._tree}),c}var n=d3.layout.hierarchy().sort(null).value(null),e=qr,r=[1,1];return t.separation=function(n){return arguments.length?(e=n,t):e},t.size=function(n){return arguments.length?(r=n,t):r},hr(t,n)},d3.layout.treemap=function(){function t(t,n){for(var e,r,u=-1,i=t.length;i>++u;)r=(e=t[u]).value*(0>n?0:n),e.area=isNaN(r)||0>=r?0:r}function n(e){var i=e.children;if(i&&i.length){var a,o,c,l=f(e),s=[],h=i.slice(),g=1/0,p="slice"===d?l.dx:"dice"===d?l.dy:"slice-dice"===d?1&e.depth?l.dy:l.dx:Math.min(l.dx,l.dy);for(t(h,l.dx*l.dy/e.value),s.area=0;(c=h.length)>0;)s.push(a=h[c-1]),s.area+=a.area,"squarify"!==d||g>=(o=r(s,p))?(h.pop(),g=o):(s.area-=s.pop().area,u(s,p,l,!1),p=Math.min(l.dx,l.dy),s.length=s.area=0,g=1/0);s.length&&(u(s,p,l,!0),s.length=s.area=0),i.forEach(n)}}function e(n){var r=n.children;if(r&&r.length){var i,a=f(n),o=r.slice(),c=[];for(t(o,a.dx*a.dy/n.value),c.area=0;i=o.pop();)c.push(i),c.area+=i.area,null!=i.z&&(u(c,i.z?a.dx:a.dy,a,!o.length),c.length=c.area=0);r.forEach(e)}}function r(t,n){for(var e,r=t.area,u=0,i=1/0,a=-1,o=t.length;o>++a;)(e=t[a].area)&&(i>e&&(i=e),e>u&&(u=e));return r*=r,n*=n,r?Math.max(n*u*g/r,r/(n*i*g)):1/0}function u(t,n,e,r){var u,i=-1,a=t.length,o=e.x,l=e.y,s=n?c(t.area/n):0;if(n==e.dx){for((r||s>e.dy)&&(s=e.dy);a>++i;)u=t[i],u.x=o,u.y=l,u.dy=s,o+=u.dx=Math.min(e.x+e.dx-o,s?c(u.area/s):0);u.z=!0,u.dx+=e.x+e.dx-o,e.y+=s,e.dy-=s}else{for((r||s>e.dx)&&(s=e.dx);a>++i;)u=t[i],u.x=o,u.y=l,u.dx=s,l+=u.dy=Math.min(e.y+e.dy-l,s?c(u.area/s):0);u.z=!1,u.dy+=e.y+e.dy-l,e.x+=s,e.dx-=s}}function i(r){var u=a||o(r),i=u[0];return i.x=0,i.y=0,i.dx=l[0],i.dy=l[1],a&&o.revalue(i),t([i],i.dx*i.dy/i.value),(a?e:n)(i),h&&(a=u),u}var a,o=d3.layout.hierarchy(),c=Math.round,l=[1,1],s=null,f=Yr,h=!1,d="squarify",g=.5*(1+Math.sqrt(5));return i.size=function(t){return arguments.length?(l=t,i):l},i.padding=function(t){function n(n){var e=t.call(i,n,n.depth);return null==e?Yr(n):Ur(n,"number"==typeof e?[e,e,e,e]:e)}function e(n){return Ur(n,t)}if(!arguments.length)return s;var r;return f=null==(s=t)?Yr:"function"==(r=typeof t)?n:"number"===r?(t=[t,t,t,t],e):e,i},i.round=function(t){return arguments.length?(c=t?Math.round:Number,i):c!=Number},i.sticky=function(t){return arguments.length?(h=t,a=null,i):h},i.ratio=function(t){return arguments.length?(g=t,i):g},i.mode=function(t){return arguments.length?(d=t+"",i):d},hr(i,o)},d3.csv=Ir(",","text/csv"),d3.tsv=Ir("	","text/tab-separated-values"),d3.geo={},d3.geo.stream=function(t,n){to.hasOwnProperty(t.type)?to[t.type](t,n):Vr(t,n)};var to={Feature:function(t,n){Vr(t.geometry,n)},FeatureCollection:function(t,n){for(var e=t.features,r=-1,u=e.length;u>++r;)Vr(e[r].geometry,n)}},no={Sphere:function(t,n){n.sphere()},Point:function(t,n){var e=t.coordinates;n.point(e[0],e[1])},MultiPoint:function(t,n){for(var e,r=t.coordinates,u=-1,i=r.length;i>++u;)e=r[u],n.point(e[0],e[1])},LineString:function(t,n){Xr(t.coordinates,n,0)},MultiLineString:function(t,n){for(var e=t.coordinates,r=-1,u=e.length;u>++r;)Xr(e[r],n,0)},Polygon:function(t,n){Zr(t.coordinates,n)},MultiPolygon:function(t,n){for(var e=t.coordinates,r=-1,u=e.length;u>++r;)Zr(e[r],n)},GeometryCollection:function(t,n){for(var e=t.geometries,r=-1,u=e.length;u>++r;)Vr(e[r],n)}};d3.geo.albersUsa=function(){function t(t){return n(t)(t)}function n(t){var n=t[0],a=t[1];return a>50?r:-140>n?u:21>a?i:e}var e=d3.geo.albers(),r=d3.geo.albers().rotate([160,0]).center([0,60]).parallels([55,65]),u=d3.geo.albers().rotate([160,0]).center([0,20]).parallels([8,18]),i=d3.geo.albers().rotate([60,0]).center([0,10]).parallels([8,18]);return t.scale=function(n){return arguments.length?(e.scale(n),r.scale(.6*n),u.scale(n),i.scale(1.5*n),t.translate(e.translate())):e.scale()},t.translate=function(n){if(!arguments.length)return e.translate();var a=e.scale(),o=n[0],c=n[1];return e.translate(n),r.translate([o-.4*a,c+.17*a]),u.translate([o-.19*a,c+.2*a]),i.translate([o+.58*a,c+.43*a]),t},t.scale(e.scale())},(d3.geo.albers=function(){var t=29.5*ji,n=45.5*ji,e=Pu(eu),r=e(t,n);return r.parallels=function(r){return arguments.length?e(t=r[0]*ji,n=r[1]*ji):[t*Oi,n*Oi]},r.rotate([98,0]).center([0,38]).scale(1e3)}).raw=eu;var eo=Vu(function(t){return Math.sqrt(2/(1+t))},function(t){return 2*Math.asin(t/2)});(d3.geo.azimuthalEqualArea=function(){return Ru(eo)}).raw=eo;var ro=Vu(function(t){var n=Math.acos(t);return n&&n/Math.sin(n)},a);(d3.geo.azimuthalEquidistant=function(){return Ru(ro)}).raw=ro,d3.geo.bounds=ru(a),d3.geo.centroid=function(t){uo=io=ao=oo=co=0,d3.geo.stream(t,lo);var n;return io&&Math.abs(n=Math.sqrt(ao*ao+oo*oo+co*co))>Pi?[Math.atan2(oo,ao)*Oi,Math.asin(Math.max(-1,Math.min(1,co/n)))*Oi]:void 0};var uo,io,ao,oo,co,lo={sphere:function(){2>uo&&(uo=2,io=ao=oo=co=0)},point:uu,lineStart:au,lineEnd:ou,polygonStart:function(){2>uo&&(uo=2,io=ao=oo=co=0),lo.lineStart=iu},polygonEnd:function(){lo.lineStart=au}};d3.geo.circle=function(){function t(){var t="function"==typeof r?r.apply(this,arguments):r,n=Ou(-t[0]*ji,-t[1]*ji,0).invert,u=[];return e(null,null,1,{point:function(t,e){u.push(t=n(t,e)),t[0]*=Oi,t[1]*=Oi}}),{type:"Polygon",coordinates:[u]}}var n,e,r=[0,0],u=6;return t.origin=function(n){return arguments.length?(r=n,t):r},t.angle=function(r){return arguments.length?(e=cu((n=+r)*ji,u*ji),t):n},t.precision=function(r){return arguments.length?(e=cu(n*ji,(u=+r)*ji),t):u},t.angle(90)};var so=su(o,vu,Mu);(d3.geo.equirectangular=function(){return Ru(_u).scale(250/Ri)}).raw=_u.invert=_u;var fo=Vu(function(t){return 1/t},Math.atan);(d3.geo.gnomonic=function(){return Ru(fo)}).raw=fo,d3.geo.graticule=function(){function t(){return{type:"MultiLineString",coordinates:n()}}function n(){return d3.range(Math.ceil(r/c)*c,e,c).map(a).concat(d3.range(Math.ceil(i/l)*l,u,l).map(o))}var e,r,u,i,a,o,c=22.5,l=c,s=2.5;return t.lines=function(){return n().map(function(t){return{type:"LineString",coordinates:t}})},t.outline=function(){return{type:"Polygon",coordinates:[a(r).concat(o(u).slice(1),a(e).reverse().slice(1),o(i).reverse().slice(1))]}},t.extent=function(n){return arguments.length?(r=+n[0][0],e=+n[1][0],i=+n[0][1],u=+n[1][1],r>e&&(n=r,r=e,e=n),i>u&&(n=i,i=u,u=n),t.precision(s)):[[r,i],[e,u]]},t.step=function(n){return arguments.length?(c=+n[0],l=+n[1],t):[c,l]},t.precision=function(n){return arguments.length?(s=+n,a=wu(i,u,s),o=Su(r,e,s),t):s},t.extent([[-180+Pi,-90+Pi],[180-Pi,90-Pi]])},d3.geo.interpolate=function(t,n){return ku(t[0]*ji,t[1]*ji,n[0]*ji,n[1]*ji)},d3.geo.greatArc=function(){function e(){for(var t=r||a.apply(this,arguments),n=u||o.apply(this,arguments),e=i||d3.geo.interpolate(t,n),u=0,l=c/e.distance,s=[t];1>(u+=l);)s.push(e(u));return s.push(n),{type:"LineString",coordinates:s}}var r,u,i,a=n,o=t,c=6*ji;return e.distance=function(){return(i||d3.geo.interpolate(r||a.apply(this,arguments),u||o.apply(this,arguments))).distance},e.source=function(t){return arguments.length?(a=t,r="function"==typeof t?null:t,i=r&&u?d3.geo.interpolate(r,u):null,e):a},e.target=function(t){return arguments.length?(o=t,u="function"==typeof t?null:t,i=r&&u?d3.geo.interpolate(r,u):null,e):o},e.precision=function(t){return arguments.length?(c=t*ji,e):c/ji},e},Eu.invert=function(t,n){return[2*Ri*t,2*Math.atan(Math.exp(2*Ri*n))-Ri/2]},(d3.geo.mercator=function(){return Ru(Eu).scale(500)}).raw=Eu;var ho=Vu(function(){return 1},Math.asin);(d3.geo.orthographic=function(){return Ru(ho)}).raw=ho,d3.geo.path=function(){function t(t){return t&&d3.geo.stream(t,r(u.pointRadius("function"==typeof i?+i.apply(this,arguments):i))),u.result()}var n,e,r,u,i=4.5;return t.area=function(t){return go=0,d3.geo.stream(t,r(mo)),go},t.centroid=function(t){return uo=ao=oo=co=0,d3.geo.stream(t,r(vo)),co?[ao/co,oo/co]:void 0},t.bounds=function(t){return ru(r)(t)},t.projection=function(e){return arguments.length?(r=(n=e)?e.stream||Nu(e):a,t):n},t.context=function(n){return arguments.length?(u=null==(e=n)?new Tu:new qu(n),t):e},t.pointRadius=function(n){return arguments.length?(i="function"==typeof n?n:+n,t):i},t.projection(d3.geo.albersUsa()).context(null)};var go,po,mo={point:Pn,lineStart:Pn,lineEnd:Pn,polygonStart:function(){po=0,mo.lineStart=Cu},polygonEnd:function(){mo.lineStart=mo.lineEnd=mo.point=Pn,go+=Math.abs(po/2)}},vo={point:zu,lineStart:Du,lineEnd:Lu,polygonStart:function(){vo.lineStart=Fu},polygonEnd:function(){vo.point=zu,vo.lineStart=Du,vo.lineEnd=Lu}};d3.geo.area=function(t){return yo=0,d3.geo.stream(t,bo),yo};var yo,Mo,bo={sphere:function(){yo+=4*Ri},point:Pn,lineStart:Pn,lineEnd:Pn,polygonStart:function(){Mo=0,bo.lineStart=Hu},polygonEnd:function(){yo+=0>Mo?4*Ri+Mo:Mo,bo.lineStart=bo.lineEnd=bo.point=Pn}};d3.geo.projection=Ru,d3.geo.projectionMutator=Pu;var xo=Vu(function(t){return 1/(1+t)},function(t){return 2*Math.atan(t)});(d3.geo.stereographic=function(){return Ru(xo)}).raw=xo,d3.geom={},d3.geom.hull=function(t){if(3>t.length)return[];var n,e,r,u,i,a,o,c,l,s,f=t.length,h=f-1,d=[],g=[],p=0;for(n=1;f>n;++n)t[n][1]<t[p][1]?p=n:t[n][1]==t[p][1]&&(p=t[n][0]<t[p][0]?n:p);for(n=0;f>n;++n)n!==p&&(u=t[n][1]-t[p][1],r=t[n][0]-t[p][0],d.push({angle:Math.atan2(u,r),index:n}));for(d.sort(function(t,n){return t.angle-n.angle}),l=d[0].angle,c=d[0].index,o=0,n=1;h>n;++n)e=d[n].index,l==d[n].angle?(r=t[c][0]-t[p][0],u=t[c][1]-t[p][1],i=t[e][0]-t[p][0],a=t[e][1]-t[p][1],r*r+u*u>=i*i+a*a?d[n].index=-1:(d[o].index=-1,l=d[n].angle,o=n,c=e)):(l=d[n].angle,o=n,c=e);for(g.push(p),n=0,e=0;2>n;++e)-1!==d[e].index&&(g.push(d[e].index),n++);for(s=g.length;h>e;++e)if(-1!==d[e].index){for(;!Xu(g[s-2],g[s-1],d[e].index,t);)--s;g[s++]=d[e].index}var m=[];for(n=0;s>n;++n)m.push(t[g[n]]);return m},d3.geom.polygon=function(t){return t.area=function(){for(var n=0,e=t.length,r=t[e-1][1]*t[0][0]-t[e-1][0]*t[0][1];e>++n;)r+=t[n-1][1]*t[n][0]-t[n-1][0]*t[n][1];return.5*r},t.centroid=function(n){var e,r,u=-1,i=t.length,a=0,o=0,c=t[i-1];for(arguments.length||(n=-1/(6*t.area()));i>++u;)e=c,c=t[u],r=e[0]*c[1]-c[0]*e[1],a+=(e[0]+c[0])*r,o+=(e[1]+c[1])*r;return[a*n,o*n]},t.clip=function(n){for(var e,r,u,i,a,o,c=-1,l=t.length,s=t[l-1];l>++c;){for(e=n.slice(),n.length=0,i=t[c],a=e[(u=e.length)-1],r=-1;u>++r;)o=e[r],Zu(o,s,i)?(Zu(a,s,i)||n.push(Bu(a,o,s,i)),n.push(o)):Zu(a,s,i)&&n.push(Bu(a,o,s,i)),a=o;s=i}return n},t},d3.geom.voronoi=function(t){var n=t.map(function(){return[]}),e=1e6;return $u(t,function(t){var r,u,i,a,o,c;1===t.a&&t.b>=0?(r=t.ep.r,u=t.ep.l):(r=t.ep.l,u=t.ep.r),1===t.a?(o=r?r.y:-e,i=t.c-t.b*o,c=u?u.y:e,a=t.c-t.b*c):(i=r?r.x:-e,o=t.c-t.a*i,a=u?u.x:e,c=t.c-t.a*a);var l=[i,o],s=[a,c];n[t.region.l.index].push(l,s),n[t.region.r.index].push(l,s)}),n=n.map(function(n,e){var r=t[e][0],u=t[e][1],i=n.map(function(t){return Math.atan2(t[0]-r,t[1]-u)});return d3.range(n.length).sort(function(t,n){return i[t]-i[n]}).filter(function(t,n,e){return!n||i[t]-i[e[n-1]]>Pi}).map(function(t){return n[t]})}),n.forEach(function(n,r){var u=n.length;if(!u)return n.push([-e,-e],[-e,e],[e,e],[e,-e]);if(!(u>2)){var i=t[r],a=n[0],o=n[1],c=i[0],l=i[1],s=a[0],f=a[1],h=o[0],d=o[1],g=Math.abs(h-s),p=d-f;if(Pi>Math.abs(p)){var m=f>l?-e:e;n.push([-e,m],[e,m])}else if(Pi>g){var v=s>c?-e:e;n.push([v,-e],[v,e])}else{var m=(s-c)*(d-f)>(h-s)*(f-l)?e:-e,y=Math.abs(p)-g;Pi>Math.abs(y)?n.push([0>p?m:-m,m]):(y>0&&(m*=-1),n.push([-e,m],[e,m]))}}}),n};var _o={l:"r",r:"l"};d3.geom.delaunay=function(t){var n=t.map(function(){return[]}),e=[];return $u(t,function(e){n[e.region.l.index].push(t[e.region.r.index])}),n.forEach(function(n,r){var u=t[r],i=u[0],a=u[1];n.forEach(function(t){t.angle=Math.atan2(t[0]-i,t[1]-a)}),n.sort(function(t,n){return t.angle-n.angle});for(var o=0,c=n.length-1;c>o;o++)e.push([u,n[o],n[o+1]])}),e},d3.geom.quadtree=function(t,n,e,r,u){function i(t,n,e,r,u,i){if(!isNaN(n.x)&&!isNaN(n.y))if(t.leaf){var o=t.point;o?.01>Math.abs(o.x-n.x)+Math.abs(o.y-n.y)?a(t,n,e,r,u,i):(t.point=null,a(t,o,e,r,u,i),a(t,n,e,r,u,i)):t.point=n}else a(t,n,e,r,u,i)}function a(t,n,e,r,u,a){var o=.5*(e+u),c=.5*(r+a),l=n.x>=o,s=n.y>=c,f=(s<<1)+l;t.leaf=!1,t=t.nodes[f]||(t.nodes[f]=Ju()),l?e=o:u=o,s?r=c:a=c,i(t,n,e,r,u,a)}var o,c=-1,l=t.length;if(5>arguments.length)if(3===arguments.length)u=e,r=n,e=n=0;else for(n=e=1/0,r=u=-1/0;l>++c;)o=t[c],n>o.x&&(n=o.x),e>o.y&&(e=o.y),o.x>r&&(r=o.x),o.y>u&&(u=o.y);var s=r-n,f=u-e;s>f?u=e+s:r=n+f;var h=Ju();return h.add=function(t){i(h,t,n,e,r,u)},h.visit=function(t){Gu(t,h,n,e,r,u)},t.forEach(h.add),h},d3.time={};var wo=Date,So=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];Ku.prototype={getDate:function(){return this._.getUTCDate()},getDay:function(){return this._.getUTCDay()},getFullYear:function(){return this._.getUTCFullYear()},getHours:function(){return this._.getUTCHours()},getMilliseconds:function(){return this._.getUTCMilliseconds()},getMinutes:function(){return this._.getUTCMinutes()},getMonth:function(){return this._.getUTCMonth()},getSeconds:function(){return this._.getUTCSeconds()},getTime:function(){return this._.getTime()},getTimezoneOffset:function(){return 0},valueOf:function(){return this._.valueOf()},setDate:function(){ko.setUTCDate.apply(this._,arguments)},setDay:function(){ko.setUTCDay.apply(this._,arguments)},setFullYear:function(){ko.setUTCFullYear.apply(this._,arguments)},setHours:function(){ko.setUTCHours.apply(this._,arguments)},setMilliseconds:function(){ko.setUTCMilliseconds.apply(this._,arguments)},setMinutes:function(){ko.setUTCMinutes.apply(this._,arguments)},setMonth:function(){ko.setUTCMonth.apply(this._,arguments)},setSeconds:function(){ko.setUTCSeconds.apply(this._,arguments)},setTime:function(){ko.setTime.apply(this._,arguments)}};var ko=Date.prototype,Eo="%a %b %e %X %Y",Ao="%m/%d/%Y",No="%H:%M:%S",To=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"],qo=["Sun","Mon","Tue","Wed","Thu","Fri","Sat"],Co=["January","February","March","April","May","June","July","August","September","October","November","December"],zo=["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];d3.time.format=function(t){function n(n){for(var r,u,i,a=[],o=-1,c=0;e>++o;)37===t.charCodeAt(o)&&(a.push(t.substring(c,o)),null!=(u=jo[r=t.charAt(++o)])&&(r=t.charAt(++o)),(i=Oo[r])&&(r=i(n,null==u?"e"===r?" ":"0":u)),a.push(r),c=o+1);return a.push(t.substring(c,o)),a.join("")}var e=t.length;return n.parse=function(n){var e={y:1900,m:0,d:1,H:0,M:0,S:0,L:0},r=Wu(e,t,n,0);if(r!=n.length)return null;"p"in e&&(e.H=e.H%12+12*e.p);var u=new wo;return u.setFullYear(e.y,e.m,e.d),u.setHours(e.H,e.M,e.S,e.L),u},n.toString=function(){return t},n};var Do=Qu(To),Lo=Qu(qo),Fo=Qu(Co),Ho=ti(Co),Ro=Qu(zo),Po=ti(zo),jo={"-":"",_:" ",0:"0"},Oo={a:function(t){return qo[t.getDay()]},A:function(t){return To[t.getDay()]},b:function(t){return zo[t.getMonth()]},B:function(t){return Co[t.getMonth()]},c:d3.time.format(Eo),d:function(t,n){return ni(t.getDate(),n,2)},e:function(t,n){return ni(t.getDate(),n,2)},H:function(t,n){return ni(t.getHours(),n,2)},I:function(t,n){return ni(t.getHours()%12||12,n,2)},j:function(t,n){return ni(1+d3.time.dayOfYear(t),n,3)},L:function(t,n){return ni(t.getMilliseconds(),n,3)},m:function(t,n){return ni(t.getMonth()+1,n,2)},M:function(t,n){return ni(t.getMinutes(),n,2)},p:function(t){return t.getHours()>=12?"PM":"AM"},S:function(t,n){return ni(t.getSeconds(),n,2)},U:function(t,n){return ni(d3.time.sundayOfYear(t),n,2)},w:function(t){return t.getDay()},W:function(t,n){return ni(d3.time.mondayOfYear(t),n,2)},x:d3.time.format(Ao),X:d3.time.format(No),y:function(t,n){return ni(t.getFullYear()%100,n,2)},Y:function(t,n){return ni(t.getFullYear()%1e4,n,4)},Z:Mi,"%":function(){return"%"}},Yo={a:ei,A:ri,b:ui,B:ii,c:ai,d:di,e:di,H:gi,I:gi,L:vi,m:hi,M:pi,p:yi,S:mi,x:oi,X:ci,y:si,Y:li},Uo=/^\s*\d+/,Io=d3.map({am:0,pm:1});d3.time.format.utc=function(t){function n(t){try{wo=Ku;var n=new wo;return n._=t,e(n)}finally{wo=Date}}var e=d3.time.format(t);return n.parse=function(t){try{wo=Ku;var n=e.parse(t);return n&&n._}finally{wo=Date}},n.toString=e.toString,n};var Vo=d3.time.format.utc("%Y-%m-%dT%H:%M:%S.%LZ");d3.time.format.iso=Date.prototype.toISOString?bi:Vo,bi.parse=function(t){var n=new Date(t);return isNaN(n)?null:n},bi.toString=Vo.toString,d3.time.second=xi(function(t){return new wo(1e3*Math.floor(t/1e3))},function(t,n){t.setTime(t.getTime()+1e3*Math.floor(n))},function(t){return t.getSeconds()}),d3.time.seconds=d3.time.second.range,d3.time.seconds.utc=d3.time.second.utc.range,d3.time.minute=xi(function(t){return new wo(6e4*Math.floor(t/6e4))},function(t,n){t.setTime(t.getTime()+6e4*Math.floor(n))},function(t){return t.getMinutes()}),d3.time.minutes=d3.time.minute.range,d3.time.minutes.utc=d3.time.minute.utc.range,d3.time.hour=xi(function(t){var n=t.getTimezoneOffset()/60;return new wo(36e5*(Math.floor(t/36e5-n)+n))},function(t,n){t.setTime(t.getTime()+36e5*Math.floor(n))},function(t){return t.getHours()}),d3.time.hours=d3.time.hour.range,d3.time.hours.utc=d3.time.hour.utc.range,d3.time.day=xi(function(t){var n=new wo(1970,0);return n.setFullYear(t.getFullYear(),t.getMonth(),t.getDate()),n},function(t,n){t.setDate(t.getDate()+n)},function(t){return t.getDate()-1}),d3.time.days=d3.time.day.range,d3.time.days.utc=d3.time.day.utc.range,d3.time.dayOfYear=function(t){var n=d3.time.year(t);return Math.floor((t-n-6e4*(t.getTimezoneOffset()-n.getTimezoneOffset()))/864e5)},So.forEach(function(t,n){t=t.toLowerCase(),n=7-n;var e=d3.time[t]=xi(function(t){return(t=d3.time.day(t)).setDate(t.getDate()-(t.getDay()+n)%7),t},function(t,n){t.setDate(t.getDate()+7*Math.floor(n))},function(t){var e=d3.time.year(t).getDay();return Math.floor((d3.time.dayOfYear(t)+(e+n)%7)/7)-(e!==n)});d3.time[t+"s"]=e.range,d3.time[t+"s"].utc=e.utc.range,d3.time[t+"OfYear"]=function(t){var e=d3.time.year(t).getDay();return Math.floor((d3.time.dayOfYear(t)+(e+n)%7)/7)}}),d3.time.week=d3.time.sunday,d3.time.weeks=d3.time.sunday.range,d3.time.weeks.utc=d3.time.sunday.utc.range,d3.time.weekOfYear=d3.time.sundayOfYear,d3.time.month=xi(function(t){return t=d3.time.day(t),t.setDate(1),t},function(t,n){t.setMonth(t.getMonth()+n)},function(t){return t.getMonth()}),d3.time.months=d3.time.month.range,d3.time.months.utc=d3.time.month.utc.range,d3.time.year=xi(function(t){return t=d3.time.day(t),t.setMonth(0,1),t},function(t,n){t.setFullYear(t.getFullYear()+n)},function(t){return t.getFullYear()}),d3.time.years=d3.time.year.range,d3.time.years.utc=d3.time.year.utc.range;var Xo=[1e3,5e3,15e3,3e4,6e4,3e5,9e5,18e5,36e5,108e5,216e5,432e5,864e5,1728e5,6048e5,2592e6,7776e6,31536e6],Zo=[[d3.time.second,1],[d3.time.second,5],[d3.time.second,15],[d3.time.second,30],[d3.time.minute,1],[d3.time.minute,5],[d3.time.minute,15],[d3.time.minute,30],[d3.time.hour,1],[d3.time.hour,3],[d3.time.hour,6],[d3.time.hour,12],[d3.time.day,1],[d3.time.day,2],[d3.time.week,1],[d3.time.month,1],[d3.time.month,3],[d3.time.year,1]],Bo=[[d3.time.format("%Y"),o],[d3.time.format("%B"),function(t){return t.getMonth()}],[d3.time.format("%b %d"),function(t){return 1!=t.getDate()}],[d3.time.format("%a %d"),function(t){return t.getDay()&&1!=t.getDate()}],[d3.time.format("%I %p"),function(t){return t.getHours()}],[d3.time.format("%I:%M"),function(t){return t.getMinutes()}],[d3.time.format(":%S"),function(t){return t.getSeconds()}],[d3.time.format(".%L"),function(t){return t.getMilliseconds()}]],$o=d3.scale.linear(),Jo=Ei(Bo);Zo.year=function(t,n){return $o.domain(t.map(Ni)).ticks(n).map(Ai)},d3.time.scale=function(){return wi(d3.scale.linear(),Zo,Jo)};var Go=Zo.map(function(t){return[t[0].utc,t[1]]}),Ko=[[d3.time.format.utc("%Y"),o],[d3.time.format.utc("%B"),function(t){return t.getUTCMonth()}],[d3.time.format.utc("%b %d"),function(t){return 1!=t.getUTCDate()}],[d3.time.format.utc("%a %d"),function(t){return t.getUTCDay()&&1!=t.getUTCDate()}],[d3.time.format.utc("%I %p"),function(t){return t.getUTCHours()}],[d3.time.format.utc("%I:%M"),function(t){return t.getUTCMinutes()}],[d3.time.format.utc(":%S"),function(t){return t.getUTCSeconds()}],[d3.time.format.utc(".%L"),function(t){return t.getUTCMilliseconds()}]],Wo=Ei(Ko);Go.year=function(t,n){return $o.domain(t.map(qi)).ticks(n).map(Ti)},d3.time.scale.utc=function(){return wi(d3.scale.linear(),Go,Wo)}})();

/*! nvd3 - v0.0.1 - 2013-12-18 */!function(){function a(a,b){return new Date(b,a+1,0).getDate()}function b(a,b,c){return function(d,e,f){var g=a(d),h=[];if(d>g&&b(g),f>1)for(;e>g;){var i=new Date(+g);0===c(i)%f&&h.push(i),b(g)}else for(;e>g;)h.push(new Date(+g)),b(g);return h}}var c=window.nv||{};c.version="1.1.15b",c.dev=!0,window.nv=c,c.tooltip=c.tooltip||{},c.utils=c.utils||{},c.models=c.models||{},c.charts={},c.graphs=[],c.logs={},c.dispatch=d3.dispatch("render_start","render_end"),c.dev&&(c.dispatch.on("render_start",function(){c.logs.startTime=+new Date}),c.dispatch.on("render_end",function(){c.logs.endTime=+new Date,c.logs.totalTime=c.logs.endTime-c.logs.startTime,c.log("total",c.logs.totalTime)})),c.log=function(){if(c.dev&&console.log&&console.log.apply)console.log.apply(console,arguments);else if(c.dev&&"function"==typeof console.log&&Function.prototype.bind){var a=Function.prototype.bind.call(console.log,console);a.apply(console,arguments)}return arguments[arguments.length-1]},c.render=function(a){a=a||1,c.render.active=!0,c.dispatch.render_start(),setTimeout(function(){for(var b,d,e=0;a>e&&(d=c.render.queue[e]);e++)b=d.generate(),typeof d.callback==typeof Function&&d.callback(b),c.graphs.push(b);c.render.queue.splice(0,e),c.render.queue.length?setTimeout(arguments.callee,0):(c.dispatch.render_end(),c.render.active=!1)},0)},c.render.active=!1,c.render.queue=[],c.addGraph=function(a){typeof arguments[0]==typeof Function&&(a={generate:arguments[0],callback:arguments[1]}),c.render.queue.push(a),c.render.active||c.render()},c.identity=function(a){return a},c.strip=function(a){return a.replace(/(\s|&)/g,"")},d3.time.monthEnd=function(a){return new Date(a.getFullYear(),a.getMonth(),0)},d3.time.monthEnds=b(d3.time.monthEnd,function(b){b.setUTCDate(b.getUTCDate()+1),b.setDate(a(b.getMonth()+1,b.getFullYear()))},function(a){return a.getMonth()}),c.interactiveGuideline=function(){"use strict";function a(l){l.each(function(l){function m(){var c=d3.mouse(this),d=c[0],e=c[1],i=!0,j=!1;if(k&&(d=d3.event.offsetX,e=d3.event.offsetY,"svg"!==d3.event.target.tagName&&(i=!1),d3.event.target.className.baseVal.match("nv-legend")&&(j=!0)),i&&(d-=f.left,e-=f.top),0>d||0>e||d>o||e>p||d3.event.relatedTarget&&void 0===d3.event.relatedTarget.ownerSVGElement||j){if(k&&d3.event.relatedTarget&&void 0===d3.event.relatedTarget.ownerSVGElement&&d3.event.relatedTarget.className.match(b.nvPointerEventsClass))return;return h.elementMouseout({mouseX:d,mouseY:e}),a.renderGuideLine(null),void 0}var l=g.invert(d);h.elementMousemove({mouseX:d,mouseY:e,pointXValue:l}),"dblclick"===d3.event.type&&h.elementDblclick({mouseX:d,mouseY:e,pointXValue:l})}var n=d3.select(this),o=d||960,p=e||400,q=n.selectAll("g.nv-wrap.nv-interactiveLineLayer").data([l]),r=q.enter().append("g").attr("class"," nv-wrap nv-interactiveLineLayer");r.append("g").attr("class","nv-interactiveGuideLine"),j&&(j.on("mousemove",m,!0).on("mouseout",m,!0).on("dblclick",m),a.renderGuideLine=function(a){if(i){var b=q.select(".nv-interactiveGuideLine").selectAll("line").data(null!=a?[c.utils.NaNtoZero(a)]:[],String);b.enter().append("line").attr("class","nv-guideline").attr("x1",function(a){return a}).attr("x2",function(a){return a}).attr("y1",p).attr("y2",0),b.exit().remove()}})})}var b=c.models.tooltip(),d=null,e=null,f={left:0,top:0},g=d3.scale.linear(),h=(d3.scale.linear(),d3.dispatch("elementMousemove","elementMouseout","elementDblclick")),i=!0,j=null,k=-1!==navigator.userAgent.indexOf("MSIE");return a.dispatch=h,a.tooltip=b,a.margin=function(b){return arguments.length?(f.top="undefined"!=typeof b.top?b.top:f.top,f.left="undefined"!=typeof b.left?b.left:f.left,a):f},a.width=function(b){return arguments.length?(d=b,a):d},a.height=function(b){return arguments.length?(e=b,a):e},a.xScale=function(b){return arguments.length?(g=b,a):g},a.showGuideLine=function(b){return arguments.length?(i=b,a):i},a.svgContainer=function(b){return arguments.length?(j=b,a):j},a},c.interactiveBisect=function(a,b,c){"use strict";if(!a instanceof Array)return null;"function"!=typeof c&&(c=function(a){return a.x});var d=d3.bisector(c).left,e=d3.max([0,d(a,b)-1]),f=c(a[e],e);if("undefined"==typeof f&&(f=e),f===b)return e;var g=d3.min([e+1,a.length-1]),h=c(a[g],g);return"undefined"==typeof h&&(h=g),Math.abs(h-b)>=Math.abs(f-b)?e:g},c.nearestValueIndex=function(a,b,c){"use strict";var d=1/0,e=null;return a.forEach(function(a,f){var g=Math.abs(b-a);d>=g&&c>g&&(d=g,e=f)}),e},function(){"use strict";window.nv.tooltip={},window.nv.models.tooltip=function(){function a(){if(l){var a=d3.select(l);"svg"!==a.node().tagName&&(a=a.select("svg"));var b=a.node()?a.attr("viewBox"):null;if(b){b=b.split(" ");var c=parseInt(a.style("width"))/b[2];n.left=n.left*c,n.top=n.top*c}}}function b(a){var b;b=l?d3.select(l):d3.select("body");var c=b.select(".nvtooltip");return null===c.node()&&(c=b.append("div").attr("class","nvtooltip "+(k?k:"xy-tooltip")).attr("id",p)),c.node().innerHTML=a,c.style("top",0).style("left",0).style("opacity",0),c.selectAll("div, table, td, tr").classed(q,!0),c.classed(q,!0),c.node()}function d(){if(o&&u(f)){a();var e=n.left,k=null!=j?j:n.top,p=b(t(f));if(m=p,l){var q=l.getElementsByTagName("svg")[0];q?q.getBoundingClientRect():l.getBoundingClientRect();var r={left:0,top:0};if(q){var s=q.getBoundingClientRect(),v=l.getBoundingClientRect(),w=s.top;if(0>w){var x=l.getBoundingClientRect();w=Math.abs(w)>x.height?0:w}r.top=Math.abs(w-v.top),r.left=Math.abs(s.left-v.left)}e+=l.offsetLeft+r.left-2*l.scrollLeft,k+=l.offsetTop+r.top-2*l.scrollTop}return i&&i>0&&(k=Math.floor(k/i)*i),c.tooltip.calcTooltipPosition([e,k],g,h,p),d}}var e=null,f=null,g="w",h=50,i=25,j=null,k=null,l=null,m=null,n={left:null,top:null},o=!0,p="nvtooltip-"+Math.floor(1e5*Math.random()),q="nv-pointer-events-none",r=function(a){return a},s=function(a){return a},t=function(a){if(null!=e)return e;if(null==a)return"";var b=d3.select(document.createElement("table")),c=b.selectAll("thead").data([a]).enter().append("thead");c.append("tr").append("td").attr("colspan",3).append("strong").classed("x-value",!0).html(s(a.value));var d=b.selectAll("tbody").data([a]).enter().append("tbody"),f=d.selectAll("tr").data(function(a){return a.series}).enter().append("tr").classed("highlight",function(a){return a.highlight});f.append("td").classed("legend-color-guide",!0).append("div").style("background-color",function(a){return a.color}),f.append("td").classed("key",!0).html(function(a){return a.key}),f.append("td").classed("value",!0).html(function(a,b){return r(a.value,b)}),f.selectAll("td").each(function(a){if(a.highlight){var b=d3.scale.linear().domain([0,1]).range(["#fff",a.color]),c=.6;d3.select(this).style("border-bottom-color",b(c)).style("border-top-color",b(c))}});var g=b.node().outerHTML;return void 0!==a.footer&&(g+="<div class='footer'>"+a.footer+"</div>"),g},u=function(a){return a&&a.series&&a.series.length>0?!0:!1};return d.nvPointerEventsClass=q,d.content=function(a){return arguments.length?(e=a,d):e},d.tooltipElem=function(){return m},d.contentGenerator=function(a){return arguments.length?("function"==typeof a&&(t=a),d):t},d.data=function(a){return arguments.length?(f=a,d):f},d.gravity=function(a){return arguments.length?(g=a,d):g},d.distance=function(a){return arguments.length?(h=a,d):h},d.snapDistance=function(a){return arguments.length?(i=a,d):i},d.classes=function(a){return arguments.length?(k=a,d):k},d.chartContainer=function(a){return arguments.length?(l=a,d):l},d.position=function(a){return arguments.length?(n.left="undefined"!=typeof a.left?a.left:n.left,n.top="undefined"!=typeof a.top?a.top:n.top,d):n},d.fixedTop=function(a){return arguments.length?(j=a,d):j},d.enabled=function(a){return arguments.length?(o=a,d):o},d.valueFormatter=function(a){return arguments.length?("function"==typeof a&&(r=a),d):r},d.headerFormatter=function(a){return arguments.length?("function"==typeof a&&(s=a),d):s},d.id=function(){return p},d},c.tooltip.show=function(a,b,d,e,f,g){var h=document.createElement("div");h.className="nvtooltip "+(g?g:"xy-tooltip");var i=f;(!f||f.tagName.match(/g|svg/i))&&(i=document.getElementsByTagName("body")[0]),h.style.left=0,h.style.top=0,h.style.opacity=0,h.innerHTML=b,i.appendChild(h),f&&(a[0]=a[0]-f.scrollLeft,a[1]=a[1]-f.scrollTop),c.tooltip.calcTooltipPosition(a,d,e,h)},c.tooltip.findFirstNonSVGParent=function(a){for(;null!==a.tagName.match(/^g|svg$/i);)a=a.parentNode;return a},c.tooltip.findTotalOffsetTop=function(a,b){var c=b;do isNaN(a.offsetTop)||(c+=a.offsetTop);while(a=a.offsetParent);return c},c.tooltip.findTotalOffsetLeft=function(a,b){var c=b;do isNaN(a.offsetLeft)||(c+=a.offsetLeft);while(a=a.offsetParent);return c},c.tooltip.calcTooltipPosition=function(a,b,d,e){var f,g,h=parseInt(e.offsetHeight),i=parseInt(e.offsetWidth),j=c.utils.windowSize().width,k=c.utils.windowSize().height,l=window.pageYOffset,m=window.pageXOffset;k=window.innerWidth>=document.body.scrollWidth?k:k-16,j=window.innerHeight>=document.body.scrollHeight?j:j-16,b=b||"s",d=d||20;var n=function(a){return c.tooltip.findTotalOffsetTop(a,g)},o=function(a){return c.tooltip.findTotalOffsetLeft(a,f)};switch(b){case"e":f=a[0]-i-d,g=a[1]-h/2;var p=o(e),q=n(e);m>p&&(f=a[0]+d>m?a[0]+d:m-p+f),l>q&&(g=l-q+g),q+h>l+k&&(g=l+k-q+g-h);break;case"w":f=a[0]+d,g=a[1]-h/2;var p=o(e),q=n(e);p+i>j&&(f=a[0]-i-d),l>q&&(g=l+5),q+h>l+k&&(g=l+k-q+g-h);break;case"n":f=a[0]-i/2-5,g=a[1]+d;var p=o(e),q=n(e);m>p&&(f=m+5),p+i>j&&(f=f-i/2+5),q+h>l+k&&(g=l+k-q+g-h);break;case"s":f=a[0]-i/2,g=a[1]-h-d;var p=o(e),q=n(e);m>p&&(f=m+5),p+i>j&&(f=f-i/2+5),l>q&&(g=l);break;case"none":f=a[0],g=a[1]-d;var p=o(e),q=n(e)}return e.style.left=f+"px",e.style.top=g+"px",e.style.opacity=1,e.style.position="absolute",e},c.tooltip.cleanup=function(){for(var a=document.getElementsByClassName("nvtooltip"),b=[];a.length;)b.push(a[0]),a[0].style.transitionDelay="0 !important",a[0].style.opacity=0,a[0].className="nvtooltip-pending-removal";setTimeout(function(){for(;b.length;){var a=b.pop();a.parentNode.removeChild(a)}},500)}}(),c.utils.windowSize=function(){var a={width:640,height:480};return document.body&&document.body.offsetWidth&&(a.width=document.body.offsetWidth,a.height=document.body.offsetHeight),"CSS1Compat"==document.compatMode&&document.documentElement&&document.documentElement.offsetWidth&&(a.width=document.documentElement.offsetWidth,a.height=document.documentElement.offsetHeight),window.innerWidth&&window.innerHeight&&(a.width=window.innerWidth,a.height=window.innerHeight),a},c.utils.windowResize=function(a){if(void 0!==a){var b=window.onresize;window.onresize=function(c){"function"==typeof b&&b(c),a(c)}}},c.utils.getColor=function(a){return arguments.length?"[object Array]"===Object.prototype.toString.call(a)?function(b,c){return b.color||a[c%a.length]}:a:c.utils.defaultColor()},c.utils.defaultColor=function(){var a=d3.scale.category20().range();return function(b,c){return b.color||a[c%a.length]}},c.utils.customTheme=function(a,b,c){b=b||function(a){return a.key},c=c||d3.scale.category20().range();var d=c.length;return function(e){var f=b(e);return d||(d=c.length),"undefined"!=typeof a[f]?"function"==typeof a[f]?a[f]():a[f]:c[--d]}},c.utils.pjax=function(a,b){function d(d){d3.html(d,function(d){var e=d3.select(b).node();e.parentNode.replaceChild(d3.select(d).select(b).node(),e),c.utils.pjax(a,b)})}d3.selectAll(a).on("click",function(){history.pushState(this.href,this.textContent,this.href),d(this.href),d3.event.preventDefault()}),d3.select(window).on("popstate",function(){d3.event.state&&d(d3.event.state)})},c.utils.calcApproxTextWidth=function(a){if("function"==typeof a.style&&"function"==typeof a.text){var b=parseInt(a.style("font-size").replace("px","")),c=a.text().length;return.5*c*b}return 0},c.utils.NaNtoZero=function(a){return"number"!=typeof a||isNaN(a)||null===a||1/0===a?0:a},c.utils.optionsFunc=function(a){return a&&d3.map(a).forEach(function(a,b){"function"==typeof this[a]&&this[a](b)}.bind(this)),this},c.models.axis=function(){"use strict";function a(c){return c.each(function(a){var c=d3.select(this),f=c.selectAll("g.nv-wrap.nv-axis").data([a]),r=f.enter().append("g").attr("class","nvd3 nv-wrap nv-axis");r.append("g");var s=f.select("g");null!==o?b.ticks(o):("top"==b.orient()||"bottom"==b.orient())&&b.ticks(Math.abs(g.range()[1]-g.range()[0])/100),s.transition().call(b),q=q||b.scale();var t=b.tickFormat();null==t&&(t=q.tickFormat());var u=s.selectAll("text.nv-axislabel").data([h||null]);switch(u.exit().remove(),b.orient()){case"top":u.enter().append("text").attr("class","nv-axislabel");var v=2==g.range().length?g.range()[1]:g.range()[g.range().length-1]+(g.range()[1]-g.range()[0]);if(u.attr("text-anchor","middle").attr("y",0).attr("x",v/2),i){var w=f.selectAll("g.nv-axisMaxMin").data(g.domain());w.enter().append("g").attr("class","nv-axisMaxMin").append("text"),w.exit().remove(),w.attr("transform",function(a){return"translate("+g(a)+",0)"}).select("text").attr("dy","-0.5em").attr("y",-b.tickPadding()).attr("text-anchor","middle").text(function(a){var b=t(a);return(""+b).match("NaN")?"":b}),w.transition().attr("transform",function(a,b){return"translate("+g.range()[b]+",0)"})}break;case"bottom":var x=36,y=30,z=s.selectAll("g").select("text");if(k%360){z.each(function(){var a=this.getBBox().width;a>y&&(y=a)});var A=Math.abs(Math.sin(k*Math.PI/180)),x=(A?A*y:y)+30;z.attr("transform",function(){return"rotate("+k+" 0,0)"}).style("text-anchor",k%360>0?"start":"end")}u.enter().append("text").attr("class","nv-axislabel");var v=2==g.range().length?g.range()[1]:g.range()[g.range().length-1]+(g.range()[1]-g.range()[0]);if(u.attr("text-anchor","middle").attr("y",x).attr("x",v/2),i){var w=f.selectAll("g.nv-axisMaxMin").data([g.domain()[0],g.domain()[g.domain().length-1]]);w.enter().append("g").attr("class","nv-axisMaxMin").append("text"),w.exit().remove(),w.attr("transform",function(a){return"translate("+(g(a)+(n?g.rangeBand()/2:0))+",0)"}).select("text").attr("dy",".71em").attr("y",b.tickPadding()).attr("transform",function(){return"rotate("+k+" 0,0)"}).style("text-anchor",k?k%360>0?"start":"end":"middle").text(function(a){var b=t(a);return(""+b).match("NaN")?"":b}),w.transition().attr("transform",function(a){return"translate("+(g(a)+(n?g.rangeBand()/2:0))+",0)"})}m&&z.attr("transform",function(a,b){return"translate(0,"+(0==b%2?"0":"12")+")"});break;case"right":if(u.enter().append("text").attr("class","nv-axislabel"),u.style("text-anchor",l?"middle":"begin").attr("transform",l?"rotate(90)":"").attr("y",l?-Math.max(d.right,e)+12:-10).attr("x",l?g.range()[0]/2:b.tickPadding()),i){var w=f.selectAll("g.nv-axisMaxMin").data(g.domain());w.enter().append("g").attr("class","nv-axisMaxMin").append("text").style("opacity",0),w.exit().remove(),w.attr("transform",function(a){return"translate(0,"+g(a)+")"}).select("text").attr("dy",".32em").attr("y",0).attr("x",b.tickPadding()).style("text-anchor","start").text(function(a){var b=t(a);return(""+b).match("NaN")?"":b}),w.transition().attr("transform",function(a,b){return"translate(0,"+g.range()[b]+")"}).select("text").style("opacity",1)}break;case"left":if(u.enter().append("text").attr("class","nv-axislabel"),u.style("text-anchor",l?"middle":"end").attr("transform",l?"rotate(-90)":"").attr("y",l?-Math.max(d.left,e)+p:-10).attr("x",l?-g.range()[0]/2:-b.tickPadding()),i){var w=f.selectAll("g.nv-axisMaxMin").data(g.domain());w.enter().append("g").attr("class","nv-axisMaxMin").append("text").style("opacity",0),w.exit().remove(),w.attr("transform",function(a){return"translate(0,"+q(a)+")"}).select("text").attr("dy",".32em").attr("y",0).attr("x",-b.tickPadding()).attr("text-anchor","end").text(function(a){var b=t(a);return(""+b).match("NaN")?"":b}),w.transition().attr("transform",function(a,b){return"translate(0,"+g.range()[b]+")"}).select("text").style("opacity",1)}}if(u.text(function(a){return a}),!i||"left"!==b.orient()&&"right"!==b.orient()||(s.selectAll("g").each(function(a){d3.select(this).select("text").attr("opacity",1),(g(a)<g.range()[1]+10||g(a)>g.range()[0]-10)&&((a>1e-10||-1e-10>a)&&d3.select(this).attr("opacity",0),d3.select(this).select("text").attr("opacity",0))}),g.domain()[0]==g.domain()[1]&&0==g.domain()[0]&&f.selectAll("g.nv-axisMaxMin").style("opacity",function(a,b){return b?0:1})),i&&("top"===b.orient()||"bottom"===b.orient())){var B=[];f.selectAll("g.nv-axisMaxMin").each(function(a,b){try{b?B.push(g(a)-this.getBBox().width-4):B.push(g(a)+this.getBBox().width+4)}catch(c){b?B.push(g(a)-4):B.push(g(a)+4)}}),s.selectAll("g").each(function(a){(g(a)<B[0]||g(a)>B[1])&&(a>1e-10||-1e-10>a?d3.select(this).remove():d3.select(this).select("text").remove())})}j&&s.selectAll(".tick").filter(function(a){return!parseFloat(Math.round(1e5*a.__data__)/1e6)&&void 0!==a.__data__}).classed("zero",!0),q=g.copy()}),a}var b=d3.svg.axis(),d={top:0,right:0,bottom:0,left:0},e=75,f=60,g=d3.scale.linear(),h=null,i=!0,j=!0,k=0,l=!0,m=!1,n=!1,o=null,p=12;b.scale(g).orient("bottom").tickFormat(function(a){return a});var q;return a.axis=b,d3.rebind(a,b,"orient","tickValues","tickSubdivide","tickSize","tickPadding","tickFormat"),d3.rebind(a,g,"domain","range","rangeBand","rangeBands"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(d.top="undefined"!=typeof b.top?b.top:d.top,d.right="undefined"!=typeof b.right?b.right:d.right,d.bottom="undefined"!=typeof b.bottom?b.bottom:d.bottom,d.left="undefined"!=typeof b.left?b.left:d.left,a):d},a.width=function(b){return arguments.length?(e=b,a):e},a.ticks=function(b){return arguments.length?(o=b,a):o},a.height=function(b){return arguments.length?(f=b,a):f},a.axisLabel=function(b){return arguments.length?(h=b,a):h},a.showMaxMin=function(b){return arguments.length?(i=b,a):i},a.highlightZero=function(b){return arguments.length?(j=b,a):j},a.scale=function(c){return arguments.length?(g=c,b.scale(g),n="function"==typeof g.rangeBands,d3.rebind(a,g,"domain","range","rangeBand","rangeBands"),a):g},a.rotateYLabel=function(b){return arguments.length?(l=b,a):l},a.rotateLabels=function(b){return arguments.length?(k=b,a):k},a.staggerLabels=function(b){return arguments.length?(m=b,a):m},a.axisLabelDistance=function(b){return arguments.length?(p=b,a):p},a},c.models.historicalBar=function(){"use strict";function a(v){return v.each(function(a){var v=h-g.left-g.right,w=i-g.top-g.bottom,x=d3.select(this);k.domain(b||d3.extent(a[0].values.map(m).concat(o))),q?k.range(e||[.5*v/a[0].values.length,v*(a[0].values.length-.5)/a[0].values.length]):k.range(e||[0,v]),l.domain(d||d3.extent(a[0].values.map(n).concat(p))).range(f||[w,0]),k.domain()[0]===k.domain()[1]&&(k.domain()[0]?k.domain([k.domain()[0]-.01*k.domain()[0],k.domain()[1]+.01*k.domain()[1]]):k.domain([-1,1])),l.domain()[0]===l.domain()[1]&&(l.domain()[0]?l.domain([l.domain()[0]+.01*l.domain()[0],l.domain()[1]-.01*l.domain()[1]]):l.domain([-1,1]));var y=x.selectAll("g.nv-wrap.nv-historicalBar-"+j).data([a[0].values]),z=y.enter().append("g").attr("class","nvd3 nv-wrap nv-historicalBar-"+j),A=z.append("defs"),B=z.append("g"),C=y.select("g");B.append("g").attr("class","nv-bars"),y.attr("transform","translate("+g.left+","+g.top+")"),x.on("click",function(a,b){t.chartClick({data:a,index:b,pos:d3.event,id:j})}),A.append("clipPath").attr("id","nv-chart-clip-path-"+j).append("rect"),y.select("#nv-chart-clip-path-"+j+" rect").attr("width",v).attr("height",w),C.attr("clip-path",r?"url(#nv-chart-clip-path-"+j+")":"");var D=y.select(".nv-bars").selectAll(".nv-bar").data(function(a){return a},function(a,b){return m(a,b)});D.exit().remove(),D.enter().append("rect").attr("x",0).attr("y",function(a,b){return c.utils.NaNtoZero(l(Math.max(0,n(a,b))))}).attr("height",function(a,b){return c.utils.NaNtoZero(Math.abs(l(n(a,b))-l(0)))}).attr("transform",function(b,c){return"translate("+(k(m(b,c))-.45*(v/a[0].values.length))+",0)"}).on("mouseover",function(b,c){u&&(d3.select(this).classed("hover",!0),t.elementMouseover({point:b,series:a[0],pos:[k(m(b,c)),l(n(b,c))],pointIndex:c,seriesIndex:0,e:d3.event}))}).on("mouseout",function(b,c){u&&(d3.select(this).classed("hover",!1),t.elementMouseout({point:b,series:a[0],pointIndex:c,seriesIndex:0,e:d3.event}))}).on("click",function(a,b){u&&(t.elementClick({value:n(a,b),data:a,index:b,pos:[k(m(a,b)),l(n(a,b))],e:d3.event,id:j}),d3.event.stopPropagation())}).on("dblclick",function(a,b){u&&(t.elementDblClick({value:n(a,b),data:a,index:b,pos:[k(m(a,b)),l(n(a,b))],e:d3.event,id:j}),d3.event.stopPropagation())}),D.attr("fill",function(a,b){return s(a,b)}).attr("class",function(a,b,c){return(n(a,b)<0?"nv-bar negative":"nv-bar positive")+" nv-bar-"+c+"-"+b}).transition().attr("transform",function(b,c){return"translate("+(k(m(b,c))-.45*(v/a[0].values.length))+",0)"}).attr("width",.9*(v/a[0].values.length)),D.transition().attr("y",function(a,b){var d=n(a,b)<0?l(0):l(0)-l(n(a,b))<1?l(0)-1:l(n(a,b));return c.utils.NaNtoZero(d)}).attr("height",function(a,b){return c.utils.NaNtoZero(Math.max(Math.abs(l(n(a,b))-l(0)),1))})}),a}var b,d,e,f,g={top:0,right:0,bottom:0,left:0},h=960,i=500,j=Math.floor(1e4*Math.random()),k=d3.scale.linear(),l=d3.scale.linear(),m=function(a){return a.x},n=function(a){return a.y},o=[],p=[0],q=!1,r=!0,s=c.utils.defaultColor(),t=d3.dispatch("chartClick","elementClick","elementDblClick","elementMouseover","elementMouseout"),u=!0;return a.highlightPoint=function(a,b){d3.select(".nv-historicalBar-"+j).select(".nv-bars .nv-bar-0-"+a).classed("hover",b)},a.clearHighlights=function(){d3.select(".nv-historicalBar-"+j).select(".nv-bars .nv-bar.hover").classed("hover",!1)},a.dispatch=t,a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(m=b,a):m},a.y=function(b){return arguments.length?(n=b,a):n},a.margin=function(b){return arguments.length?(g.top="undefined"!=typeof b.top?b.top:g.top,g.right="undefined"!=typeof b.right?b.right:g.right,g.bottom="undefined"!=typeof b.bottom?b.bottom:g.bottom,g.left="undefined"!=typeof b.left?b.left:g.left,a):g},a.width=function(b){return arguments.length?(h=b,a):h},a.height=function(b){return arguments.length?(i=b,a):i},a.xScale=function(b){return arguments.length?(k=b,a):k},a.yScale=function(b){return arguments.length?(l=b,a):l},a.xDomain=function(c){return arguments.length?(b=c,a):b},a.yDomain=function(b){return arguments.length?(d=b,a):d},a.xRange=function(b){return arguments.length?(e=b,a):e},a.yRange=function(b){return arguments.length?(f=b,a):f},a.forceX=function(b){return arguments.length?(o=b,a):o},a.forceY=function(b){return arguments.length?(p=b,a):p},a.padData=function(b){return arguments.length?(q=b,a):q},a.clipEdge=function(b){return arguments.length?(r=b,a):r},a.color=function(b){return arguments.length?(s=c.utils.getColor(b),a):s},a.id=function(b){return arguments.length?(j=b,a):j},a.interactive=function(){return arguments.length?(u=!1,a):u},a},c.models.bullet=function(){"use strict";function a(c){return c.each(function(a,c){var d=m-b.left-b.right,o=n-b.top-b.bottom,r=d3.select(this),s=f.call(this,a,c).slice().sort(d3.descending),t=g.call(this,a,c).slice().sort(d3.descending),u=h.call(this,a,c).slice().sort(d3.descending),v=i.call(this,a,c).slice(),w=j.call(this,a,c).slice(),x=k.call(this,a,c).slice(),y=d3.scale.linear().domain(d3.extent(d3.merge([l,s]))).range(e?[d,0]:[0,d]);this.__chart__||d3.scale.linear().domain([0,1/0]).range(y.range()),this.__chart__=y;var z=d3.min(s),A=d3.max(s),B=s[1],C=r.selectAll("g.nv-wrap.nv-bullet").data([a]),D=C.enter().append("g").attr("class","nvd3 nv-wrap nv-bullet"),E=D.append("g"),F=C.select("g");E.append("rect").attr("class","nv-range nv-rangeMax"),E.append("rect").attr("class","nv-range nv-rangeAvg"),E.append("rect").attr("class","nv-range nv-rangeMin"),E.append("rect").attr("class","nv-measure"),E.append("path").attr("class","nv-markerTriangle"),C.attr("transform","translate("+b.left+","+b.top+")");var G=function(a){return Math.abs(y(a)-y(0))},H=function(a){return 0>a?y(a):y(0)};F.select("rect.nv-rangeMax").attr("height",o).attr("width",G(A>0?A:z)).attr("x",H(A>0?A:z)).datum(A>0?A:z),F.select("rect.nv-rangeAvg").attr("height",o).attr("width",G(B)).attr("x",H(B)).datum(B),F.select("rect.nv-rangeMin").attr("height",o).attr("width",G(A)).attr("x",H(A)).attr("width",G(A>0?z:A)).attr("x",H(A>0?z:A)).datum(A>0?z:A),F.select("rect.nv-measure").style("fill",p).attr("height",o/3).attr("y",o/3).attr("width",0>u?y(0)-y(u[0]):y(u[0])-y(0)).attr("x",H(u)).on("mouseover",function(){q.elementMouseover({value:u[0],label:x[0]||"Current",pos:[y(u[0]),o/2]})}).on("mouseout",function(){q.elementMouseout({value:u[0],label:x[0]||"Current"})});var I=o/6;t[0]?F.selectAll("path.nv-markerTriangle").attr("transform",function(){return"translate("+y(t[0])+","+o/2+")"}).attr("d","M0,"+I+"L"+I+","+-I+" "+-I+","+-I+"Z").on("mouseover",function(){q.elementMouseover({value:t[0],label:w[0]||"Previous",pos:[y(t[0]),o/2]})}).on("mouseout",function(){q.elementMouseout({value:t[0],label:w[0]||"Previous"})}):F.selectAll("path.nv-markerTriangle").remove(),C.selectAll(".nv-range").on("mouseover",function(a,b){var c=v[b]||(b?1==b?"Mean":"Minimum":"Maximum");q.elementMouseover({value:a,label:c,pos:[y(a),o/2]})}).on("mouseout",function(a,b){var c=v[b]||(b?1==b?"Mean":"Minimum":"Maximum");q.elementMouseout({value:a,label:c})})}),a}var b={top:0,right:0,bottom:0,left:0},d="left",e=!1,f=function(a){return a.ranges},g=function(a){return a.markers},h=function(a){return a.measures},i=function(a){return a.rangeLabels?a.rangeLabels:[]},j=function(a){return a.markerLabels?a.markerLabels:[]},k=function(a){return a.measureLabels?a.measureLabels:[]},l=[0],m=380,n=30,o=null,p=c.utils.getColor(["#1f77b4"]),q=d3.dispatch("elementMouseover","elementMouseout");return a.dispatch=q,a.options=c.utils.optionsFunc.bind(a),a.orient=function(b){return arguments.length?(d=b,e="right"==d||"bottom"==d,a):d},a.ranges=function(b){return arguments.length?(f=b,a):f},a.markers=function(b){return arguments.length?(g=b,a):g},a.measures=function(b){return arguments.length?(h=b,a):h},a.forceX=function(b){return arguments.length?(l=b,a):l},a.width=function(b){return arguments.length?(m=b,a):m},a.height=function(b){return arguments.length?(n=b,a):n},a.margin=function(c){return arguments.length?(b.top="undefined"!=typeof c.top?c.top:b.top,b.right="undefined"!=typeof c.right?c.right:b.right,b.bottom="undefined"!=typeof c.bottom?c.bottom:b.bottom,b.left="undefined"!=typeof c.left?c.left:b.left,a):b},a.tickFormat=function(b){return arguments.length?(o=b,a):o},a.color=function(b){return arguments.length?(p=c.utils.getColor(b),a):p},a},c.models.bulletChart=function(){"use strict";function a(c){return c.each(function(d,n){var r=d3.select(this),s=(j||parseInt(r.style("width"))||960)-f.left-f.right,t=k-f.top-f.bottom,u=this;if(a.update=function(){a(c)},a.container=this,!d||!g.call(this,d,n)){var v=r.selectAll(".nv-noData").data([o]);return v.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),v.attr("x",f.left+s/2).attr("y",18+f.top+t/2).text(function(a){return a}),a}r.selectAll(".nv-noData").remove();var w=g.call(this,d,n).slice().sort(d3.descending),x=h.call(this,d,n).slice().sort(d3.descending),y=i.call(this,d,n).slice().sort(d3.descending),z=r.selectAll("g.nv-wrap.nv-bulletChart").data([d]),A=z.enter().append("g").attr("class","nvd3 nv-wrap nv-bulletChart"),B=A.append("g"),C=z.select("g");B.append("g").attr("class","nv-bulletWrap"),B.append("g").attr("class","nv-titles"),z.attr("transform","translate("+f.left+","+f.top+")");var D=d3.scale.linear().domain([0,Math.max(w[0],x[0],y[0])]).range(e?[s,0]:[0,s]),E=this.__chart__||d3.scale.linear().domain([0,1/0]).range(D.range());this.__chart__=D;var F=B.select(".nv-titles").append("g").attr("text-anchor","end").attr("transform","translate(-6,"+(k-f.top-f.bottom)/2+")");F.append("text").attr("class","nv-title").text(function(a){return a.title}),F.append("text").attr("class","nv-subtitle").attr("dy","1em").text(function(a){return a.subtitle}),b.width(s).height(t);var G=C.select(".nv-bulletWrap");d3.transition(G).call(b);var H=l||D.tickFormat(s/100),I=C.selectAll("g.nv-tick").data(D.ticks(s/50),function(a){return this.textContent||H(a)}),J=I.enter().append("g").attr("class","nv-tick").attr("transform",function(a){return"translate("+E(a)+",0)"}).style("opacity",1e-6);J.append("line").attr("y1",t).attr("y2",7*t/6),J.append("text").attr("text-anchor","middle").attr("dy","1em").attr("y",7*t/6).text(H);var K=d3.transition(I).attr("transform",function(a){return"translate("+D(a)+",0)"}).style("opacity",1);K.select("line").attr("y1",t).attr("y2",7*t/6),K.select("text").attr("y",7*t/6),d3.transition(I.exit()).attr("transform",function(a){return"translate("+D(a)+",0)"}).style("opacity",1e-6).remove(),p.on("tooltipShow",function(a){a.key=d.title,m&&q(a,u.parentNode)})}),d3.timer.flush(),a}var b=c.models.bullet(),d="left",e=!1,f={top:5,right:40,bottom:20,left:120},g=function(a){return a.ranges},h=function(a){return a.markers},i=function(a){return a.measures},j=null,k=55,l=null,m=!0,n=function(a,b,c){return"<h3>"+b+"</h3>"+"<p>"+c+"</p>"},o="No Data Available.",p=d3.dispatch("tooltipShow","tooltipHide"),q=function(b,d){var e=b.pos[0]+(d.offsetLeft||0)+f.left,g=b.pos[1]+(d.offsetTop||0)+f.top,h=n(b.key,b.label,b.value,b,a);c.tooltip.show([e,g],h,b.value<0?"e":"w",null,d)};return b.dispatch.on("elementMouseover.tooltip",function(a){p.tooltipShow(a)}),b.dispatch.on("elementMouseout.tooltip",function(a){p.tooltipHide(a)}),p.on("tooltipHide",function(){m&&c.tooltip.cleanup()}),a.dispatch=p,a.bullet=b,d3.rebind(a,b,"color"),a.options=c.utils.optionsFunc.bind(a),a.orient=function(b){return arguments.length?(d=b,e="right"==d||"bottom"==d,a):d},a.ranges=function(b){return arguments.length?(g=b,a):g},a.markers=function(b){return arguments.length?(h=b,a):h},a.measures=function(b){return arguments.length?(i=b,a):i},a.width=function(b){return arguments.length?(j=b,a):j},a.height=function(b){return arguments.length?(k=b,a):k},a.margin=function(b){return arguments.length?(f.top="undefined"!=typeof b.top?b.top:f.top,f.right="undefined"!=typeof b.right?b.right:f.right,f.bottom="undefined"!=typeof b.bottom?b.bottom:f.bottom,f.left="undefined"!=typeof b.left?b.left:f.left,a):f},a.tickFormat=function(b){return arguments.length?(l=b,a):l},a.tooltips=function(b){return arguments.length?(m=b,a):m},a.tooltipContent=function(b){return arguments.length?(n=b,a):n},a.noData=function(b){return arguments.length?(o=b,a):o},a},c.models.cumulativeLineChart=function(){"use strict";function a(x){return x.each(function(x){function I(){d3.select(a.container).style("cursor","ew-resize")}function J(){G.x=d3.event.x,G.i=Math.round(F.invert(G.x)),L()}function K(){d3.select(a.container).style("cursor","auto"),z.index=G.i,D.stateChange(z)}function L(){db.data([G]);var b=a.transitionDuration();a.transitionDuration(0),a.update(),a.transitionDuration(b)}var M=d3.select(this).classed("nv-chart-"+y,!0),N=this,O=(n||parseInt(M.style("width"))||960)-l.left-l.right,P=(o||parseInt(M.style("height"))||400)-l.top-l.bottom;if(a.update=function(){M.transition().duration(E).call(a)},a.container=this,z.disabled=x.map(function(a){return!!a.disabled}),!A){var Q;A={};for(Q in z)A[Q]=z[Q]instanceof Array?z[Q].slice(0):z[Q]}var R=d3.behavior.drag().on("dragstart",I).on("drag",J).on("dragend",K);if(!(x&&x.length&&x.filter(function(a){return a.values.length}).length)){var S=M.selectAll(".nv-noData").data([B]);return S.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),S.attr("x",l.left+O/2).attr("y",l.top+P/2).text(function(a){return a}),a}if(M.selectAll(".nv-noData").remove(),d=f.xScale(),e=f.yScale(),w)f.yDomain(null);else{var T=x.filter(function(a){return!a.disabled}).map(function(a){var b=d3.extent(a.values,f.y());return b[0]<-.95&&(b[0]=-.95),[(b[0]-b[1])/(1+b[1]),(b[1]-b[0])/(1+b[0])]}),U=[d3.min(T,function(a){return a[0]}),d3.max(T,function(a){return a[1]})];f.yDomain(U)}F.domain([0,x[0].values.length-1]).range([0,O]).clamp(!0);var x=b(G.i,x),V=v?"none":"all",W=M.selectAll("g.nv-wrap.nv-cumulativeLine").data([x]),X=W.enter().append("g").attr("class","nvd3 nv-wrap nv-cumulativeLine").append("g"),Y=W.select("g");
if(X.append("g").attr("class","nv-interactive"),X.append("g").attr("class","nv-x nv-axis").style("pointer-events","none"),X.append("g").attr("class","nv-y nv-axis"),X.append("g").attr("class","nv-background"),X.append("g").attr("class","nv-linesWrap").style("pointer-events",V),X.append("g").attr("class","nv-avgLinesWrap").style("pointer-events","none"),X.append("g").attr("class","nv-legendWrap"),X.append("g").attr("class","nv-controlsWrap"),p&&(i.width(O),Y.select(".nv-legendWrap").datum(x).call(i),l.top!=i.height()&&(l.top=i.height(),P=(o||parseInt(M.style("height"))||400)-l.top-l.bottom),Y.select(".nv-legendWrap").attr("transform","translate(0,"+-l.top+")")),u){var Z=[{key:"Re-scale y-axis",disabled:!w}];j.width(140).color(["#444","#444","#444"]),Y.select(".nv-controlsWrap").datum(Z).attr("transform","translate(0,"+-l.top+")").call(j)}W.attr("transform","translate("+l.left+","+l.top+")"),s&&Y.select(".nv-y.nv-axis").attr("transform","translate("+O+",0)");var $=x.filter(function(a){return a.tempDisabled});W.select(".tempDisabled").remove(),$.length&&W.append("text").attr("class","tempDisabled").attr("x",O/2).attr("y","-.71em").style("text-anchor","end").text($.map(function(a){return a.key}).join(", ")+" values cannot be calculated for this time period."),v&&(k.width(O).height(P).margin({left:l.left,top:l.top}).svgContainer(M).xScale(d),W.select(".nv-interactive").call(k)),X.select(".nv-background").append("rect"),Y.select(".nv-background rect").attr("width",O).attr("height",P),f.y(function(a){return a.display.y}).width(O).height(P).color(x.map(function(a,b){return a.color||m(a,b)}).filter(function(a,b){return!x[b].disabled&&!x[b].tempDisabled}));var _=Y.select(".nv-linesWrap").datum(x.filter(function(a){return!a.disabled&&!a.tempDisabled}));_.call(f),x.forEach(function(a,b){a.seriesIndex=b});var ab=x.filter(function(a){return!a.disabled&&!!C(a)}),bb=Y.select(".nv-avgLinesWrap").selectAll("line").data(ab,function(a){return a.key}),cb=function(a){var b=e(C(a));return 0>b?0:b>P?P:b};bb.enter().append("line").style("stroke-width",2).style("stroke-dasharray","10,10").style("stroke",function(a){return f.color()(a,a.seriesIndex)}).attr("x1",0).attr("x2",O).attr("y1",cb).attr("y2",cb),bb.style("stroke-opacity",function(a){var b=e(C(a));return 0>b||b>P?0:1}).attr("x1",0).attr("x2",O).attr("y1",cb).attr("y2",cb),bb.exit().remove();var db=_.selectAll(".nv-indexLine").data([G]);db.enter().append("rect").attr("class","nv-indexLine").attr("width",3).attr("x",-2).attr("fill","red").attr("fill-opacity",.5).style("pointer-events","all").call(R),db.attr("transform",function(a){return"translate("+F(a.i)+",0)"}).attr("height",P),q&&(g.scale(d).ticks(Math.min(x[0].values.length,O/70)).tickSize(-P,0),Y.select(".nv-x.nv-axis").attr("transform","translate(0,"+e.range()[0]+")"),d3.transition(Y.select(".nv-x.nv-axis")).call(g)),r&&(h.scale(e).ticks(P/36).tickSize(-O,0),d3.transition(Y.select(".nv-y.nv-axis")).call(h)),Y.select(".nv-background rect").on("click",function(){G.x=d3.mouse(this)[0],G.i=Math.round(F.invert(G.x)),z.index=G.i,D.stateChange(z),L()}),f.dispatch.on("elementClick",function(a){G.i=a.pointIndex,G.x=F(G.i),z.index=G.i,D.stateChange(z),L()}),j.dispatch.on("legendClick",function(b){b.disabled=!b.disabled,w=!b.disabled,z.rescaleY=w,D.stateChange(z),a.update()}),i.dispatch.on("stateChange",function(b){z.disabled=b.disabled,D.stateChange(z),a.update()}),k.dispatch.on("elementMousemove",function(b){f.clearHighlights();var d,e,i,j=[];if(x.filter(function(a,b){return a.seriesIndex=b,!a.disabled}).forEach(function(g,h){e=c.interactiveBisect(g.values,b.pointXValue,a.x()),f.highlightPoint(h,e,!0);var k=g.values[e];"undefined"!=typeof k&&("undefined"==typeof d&&(d=k),"undefined"==typeof i&&(i=a.xScale()(a.x()(k,e))),j.push({key:g.key,value:a.y()(k,e),color:m(g,g.seriesIndex)}))}),j.length>2){var n=a.yScale().invert(b.mouseY),o=Math.abs(a.yScale().domain()[0]-a.yScale().domain()[1]),p=.03*o,q=c.nearestValueIndex(j.map(function(a){return a.value}),n,p);null!==q&&(j[q].highlight=!0)}var r=g.tickFormat()(a.x()(d,e),e);k.tooltip.position({left:i+l.left,top:b.mouseY+l.top}).chartContainer(N.parentNode).enabled(t).valueFormatter(function(a){return h.tickFormat()(a)}).data({value:r,series:j})(),k.renderGuideLine(i)}),k.dispatch.on("elementMouseout",function(){D.tooltipHide(),f.clearHighlights()}),D.on("tooltipShow",function(a){t&&H(a,N.parentNode)}),D.on("changeState",function(b){"undefined"!=typeof b.disabled&&(x.forEach(function(a,c){a.disabled=b.disabled[c]}),z.disabled=b.disabled),"undefined"!=typeof b.index&&(G.i=b.index,G.x=F(G.i),z.index=b.index,db.data([G])),"undefined"!=typeof b.rescaleY&&(w=b.rescaleY),a.update()})}),a}function b(a,b){return b.map(function(b){if(!b.values)return b;var c=f.y()(b.values[a],a);return-.95>c?(b.tempDisabled=!0,b):(b.tempDisabled=!1,b.values=b.values.map(function(a,b){return a.display={y:(f.y()(a,b)-c)/(1+c)},a}),b)})}var d,e,f=c.models.line(),g=c.models.axis(),h=c.models.axis(),i=c.models.legend(),j=c.models.legend(),k=c.interactiveGuideline(),l={top:30,right:30,bottom:50,left:60},m=c.utils.defaultColor(),n=null,o=null,p=!0,q=!0,r=!0,s=!1,t=!0,u=!0,v=!1,w=!0,x=function(a,b,c){return"<h3>"+a+"</h3>"+"<p>"+c+" at "+b+"</p>"},y=f.id(),z={index:0,rescaleY:w},A=null,B="No Data Available.",C=function(a){return a.average},D=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState"),E=250;g.orient("bottom").tickPadding(7),h.orient(s?"right":"left"),j.updateState(!1);var F=d3.scale.linear(),G={i:0,x:0},H=function(b,d){var e=b.pos[0]+(d.offsetLeft||0),i=b.pos[1]+(d.offsetTop||0),j=g.tickFormat()(f.x()(b.point,b.pointIndex)),k=h.tickFormat()(f.y()(b.point,b.pointIndex)),l=x(b.series.key,j,k,b,a);c.tooltip.show([e,i],l,null,null,d)};return f.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+l.left,a.pos[1]+l.top],D.tooltipShow(a)}),f.dispatch.on("elementMouseout.tooltip",function(a){D.tooltipHide(a)}),D.on("tooltipHide",function(){t&&c.tooltip.cleanup()}),a.dispatch=D,a.lines=f,a.legend=i,a.xAxis=g,a.yAxis=h,a.interactiveLayer=k,d3.rebind(a,f,"defined","isArea","x","y","xScale","yScale","size","xDomain","yDomain","xRange","yRange","forceX","forceY","interactive","clipEdge","clipVoronoi","useVoronoi","id"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(l.top="undefined"!=typeof b.top?b.top:l.top,l.right="undefined"!=typeof b.right?b.right:l.right,l.bottom="undefined"!=typeof b.bottom?b.bottom:l.bottom,l.left="undefined"!=typeof b.left?b.left:l.left,a):l},a.width=function(b){return arguments.length?(n=b,a):n},a.height=function(b){return arguments.length?(o=b,a):o},a.color=function(b){return arguments.length?(m=c.utils.getColor(b),i.color(m),a):m},a.rescaleY=function(b){return arguments.length?(w=b,a):w},a.showControls=function(b){return arguments.length?(u=b,a):u},a.useInteractiveGuideline=function(b){return arguments.length?(v=b,b===!0&&(a.interactive(!1),a.useVoronoi(!1)),a):v},a.showLegend=function(b){return arguments.length?(p=b,a):p},a.showXAxis=function(b){return arguments.length?(q=b,a):q},a.showYAxis=function(b){return arguments.length?(r=b,a):r},a.rightAlignYAxis=function(b){return arguments.length?(s=b,h.orient(b?"right":"left"),a):s},a.tooltips=function(b){return arguments.length?(t=b,a):t},a.tooltipContent=function(b){return arguments.length?(x=b,a):x},a.state=function(b){return arguments.length?(z=b,a):z},a.defaultState=function(b){return arguments.length?(A=b,a):A},a.noData=function(b){return arguments.length?(B=b,a):B},a.average=function(b){return arguments.length?(C=b,a):C},a.transitionDuration=function(b){return arguments.length?(E=b,a):E},a},c.models.discreteBar=function(){"use strict";function a(c){return c.each(function(a){var c=j-i.left-i.right,l=k-i.top-i.bottom,w=d3.select(this);a.forEach(function(a,b){a.values.forEach(function(a){a.series=b})});var x=b&&d?[]:a.map(function(a){return a.values.map(function(a,b){return{x:o(a,b),y:p(a,b),y0:a.y0}})});m.domain(b||d3.merge(x).map(function(a){return a.x})).rangeBands(e||[0,c],.1),n.domain(d||d3.extent(d3.merge(x).map(function(a){return a.y}).concat(q))),s?n.range(f||[l-(n.domain()[0]<0?12:0),n.domain()[1]>0?12:0]):n.range(f||[l,0]),g=g||m,h=h||n.copy().range([n(0),n(0)]);var y=w.selectAll("g.nv-wrap.nv-discretebar").data([a]),z=y.enter().append("g").attr("class","nvd3 nv-wrap nv-discretebar"),A=z.append("g");y.select("g"),A.append("g").attr("class","nv-groups"),y.attr("transform","translate("+i.left+","+i.top+")");var B=y.select(".nv-groups").selectAll(".nv-group").data(function(a){return a},function(a){return a.key});B.enter().append("g").style("stroke-opacity",1e-6).style("fill-opacity",1e-6),B.exit().transition().style("stroke-opacity",1e-6).style("fill-opacity",1e-6).remove(),B.attr("class",function(a,b){return"nv-group nv-series-"+b}).classed("hover",function(a){return a.hover}),B.transition().style("stroke-opacity",1).style("fill-opacity",.75);var C=B.selectAll("g.nv-bar").data(function(a){return a.values});C.exit().remove();var D=C.enter().append("g").attr("transform",function(a,b){return"translate("+(m(o(a,b))+.05*m.rangeBand())+", "+n(0)+")"}).on("mouseover",function(b,c){d3.select(this).classed("hover",!0),u.elementMouseover({value:p(b,c),point:b,series:a[b.series],pos:[m(o(b,c))+m.rangeBand()*(b.series+.5)/a.length,n(p(b,c))],pointIndex:c,seriesIndex:b.series,e:d3.event})}).on("mouseout",function(b,c){d3.select(this).classed("hover",!1),u.elementMouseout({value:p(b,c),point:b,series:a[b.series],pointIndex:c,seriesIndex:b.series,e:d3.event})}).on("click",function(b,c){u.elementClick({value:p(b,c),point:b,series:a[b.series],pos:[m(o(b,c))+m.rangeBand()*(b.series+.5)/a.length,n(p(b,c))],pointIndex:c,seriesIndex:b.series,e:d3.event}),d3.event.stopPropagation()}).on("dblclick",function(b,c){u.elementDblClick({value:p(b,c),point:b,series:a[b.series],pos:[m(o(b,c))+m.rangeBand()*(b.series+.5)/a.length,n(p(b,c))],pointIndex:c,seriesIndex:b.series,e:d3.event}),d3.event.stopPropagation()});D.append("rect").attr("height",0).attr("width",.9*m.rangeBand()/a.length),s?(D.append("text").attr("text-anchor","middle"),C.select("text").text(function(a,b){return t(p(a,b))}).transition().attr("x",.9*m.rangeBand()/2).attr("y",function(a,b){return p(a,b)<0?n(p(a,b))-n(0)+12:-4})):C.selectAll("text").remove(),C.attr("class",function(a,b){return p(a,b)<0?"nv-bar negative":"nv-bar positive"}).style("fill",function(a,b){return a.color||r(a,b)}).style("stroke",function(a,b){return a.color||r(a,b)}).select("rect").attr("class",v).transition().attr("width",.9*m.rangeBand()/a.length),C.transition().attr("transform",function(a,b){var c=m(o(a,b))+.05*m.rangeBand(),d=p(a,b)<0?n(0):n(0)-n(p(a,b))<1?n(0)-1:n(p(a,b));return"translate("+c+", "+d+")"}).select("rect").attr("height",function(a,b){return Math.max(Math.abs(n(p(a,b))-n(d&&d[0]||0))||1)}),g=m.copy(),h=n.copy()}),a}var b,d,e,f,g,h,i={top:0,right:0,bottom:0,left:0},j=960,k=500,l=Math.floor(1e4*Math.random()),m=d3.scale.ordinal(),n=d3.scale.linear(),o=function(a){return a.x},p=function(a){return a.y},q=[0],r=c.utils.defaultColor(),s=!1,t=d3.format(",.2f"),u=d3.dispatch("chartClick","elementClick","elementDblClick","elementMouseover","elementMouseout"),v="discreteBar";return a.dispatch=u,a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(o=b,a):o},a.y=function(b){return arguments.length?(p=b,a):p},a.margin=function(b){return arguments.length?(i.top="undefined"!=typeof b.top?b.top:i.top,i.right="undefined"!=typeof b.right?b.right:i.right,i.bottom="undefined"!=typeof b.bottom?b.bottom:i.bottom,i.left="undefined"!=typeof b.left?b.left:i.left,a):i},a.width=function(b){return arguments.length?(j=b,a):j},a.height=function(b){return arguments.length?(k=b,a):k},a.xScale=function(b){return arguments.length?(m=b,a):m},a.yScale=function(b){return arguments.length?(n=b,a):n},a.xDomain=function(c){return arguments.length?(b=c,a):b},a.yDomain=function(b){return arguments.length?(d=b,a):d},a.xRange=function(b){return arguments.length?(e=b,a):e},a.yRange=function(b){return arguments.length?(f=b,a):f},a.forceY=function(b){return arguments.length?(q=b,a):q},a.color=function(b){return arguments.length?(r=c.utils.getColor(b),a):r},a.id=function(b){return arguments.length?(l=b,a):l},a.showValues=function(b){return arguments.length?(s=b,a):s},a.valueFormat=function(b){return arguments.length?(t=b,a):t},a.rectClass=function(b){return arguments.length?(v=b,a):v},a},c.models.discreteBarChart=function(){"use strict";function a(c){return c.each(function(c){var k=d3.select(this),q=this,v=(i||parseInt(k.style("width"))||960)-h.left-h.right,w=(j||parseInt(k.style("height"))||400)-h.top-h.bottom;if(a.update=function(){s.beforeUpdate(),k.transition().duration(t).call(a)},a.container=this,!(c&&c.length&&c.filter(function(a){return a.values.length}).length)){var x=k.selectAll(".nv-noData").data([r]);return x.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),x.attr("x",h.left+v/2).attr("y",h.top+w/2).text(function(a){return a}),a}k.selectAll(".nv-noData").remove(),b=e.xScale(),d=e.yScale().clamp(!0);var y=k.selectAll("g.nv-wrap.nv-discreteBarWithAxes").data([c]),z=y.enter().append("g").attr("class","nvd3 nv-wrap nv-discreteBarWithAxes").append("g"),A=z.append("defs"),B=y.select("g");z.append("g").attr("class","nv-x nv-axis"),z.append("g").attr("class","nv-y nv-axis").append("g").attr("class","nv-zeroLine").append("line"),z.append("g").attr("class","nv-barsWrap"),B.attr("transform","translate("+h.left+","+h.top+")"),n&&B.select(".nv-y.nv-axis").attr("transform","translate("+v+",0)"),e.width(v).height(w);var C=B.select(".nv-barsWrap").datum(c.filter(function(a){return!a.disabled}));if(C.transition().call(e),A.append("clipPath").attr("id","nv-x-label-clip-"+e.id()).append("rect"),B.select("#nv-x-label-clip-"+e.id()+" rect").attr("width",b.rangeBand()*(o?2:1)).attr("height",16).attr("x",-b.rangeBand()/(o?1:2)),l){f.scale(b).ticks(v/100).tickSize(-w,0),B.select(".nv-x.nv-axis").attr("transform","translate(0,"+(d.range()[0]+(e.showValues()&&d.domain()[0]<0?16:0))+")"),B.select(".nv-x.nv-axis").transition().call(f);var D=B.select(".nv-x.nv-axis").selectAll("g");o&&D.selectAll("text").attr("transform",function(a,b,c){return"translate(0,"+(0==c%2?"5":"17")+")"})}m&&(g.scale(d).ticks(w/36).tickSize(-v,0),B.select(".nv-y.nv-axis").transition().call(g)),B.select(".nv-zeroLine line").attr("x1",0).attr("x2",v).attr("y1",d(0)).attr("y2",d(0)),s.on("tooltipShow",function(a){p&&u(a,q.parentNode)})}),a}var b,d,e=c.models.discreteBar(),f=c.models.axis(),g=c.models.axis(),h={top:15,right:10,bottom:50,left:60},i=null,j=null,k=c.utils.getColor(),l=!0,m=!0,n=!1,o=!1,p=!0,q=function(a,b,c){return"<h3>"+b+"</h3>"+"<p>"+c+"</p>"},r="No Data Available.",s=d3.dispatch("tooltipShow","tooltipHide","beforeUpdate"),t=250;f.orient("bottom").highlightZero(!1).showMaxMin(!1).tickFormat(function(a){return a}),g.orient(n?"right":"left").tickFormat(d3.format(",.1f"));var u=function(b,d){var h=b.pos[0]+(d.offsetLeft||0),i=b.pos[1]+(d.offsetTop||0),j=f.tickFormat()(e.x()(b.point,b.pointIndex)),k=g.tickFormat()(e.y()(b.point,b.pointIndex)),l=q(b.series.key,j,k,b,a);c.tooltip.show([h,i],l,b.value<0?"n":"s",null,d)};return e.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+h.left,a.pos[1]+h.top],s.tooltipShow(a)}),e.dispatch.on("elementMouseout.tooltip",function(a){s.tooltipHide(a)}),s.on("tooltipHide",function(){p&&c.tooltip.cleanup()}),a.dispatch=s,a.discretebar=e,a.xAxis=f,a.yAxis=g,d3.rebind(a,e,"x","y","xDomain","yDomain","xRange","yRange","forceX","forceY","id","showValues","valueFormat"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(h.top="undefined"!=typeof b.top?b.top:h.top,h.right="undefined"!=typeof b.right?b.right:h.right,h.bottom="undefined"!=typeof b.bottom?b.bottom:h.bottom,h.left="undefined"!=typeof b.left?b.left:h.left,a):h},a.width=function(b){return arguments.length?(i=b,a):i},a.height=function(b){return arguments.length?(j=b,a):j},a.color=function(b){return arguments.length?(k=c.utils.getColor(b),e.color(k),a):k},a.showXAxis=function(b){return arguments.length?(l=b,a):l},a.showYAxis=function(b){return arguments.length?(m=b,a):m},a.rightAlignYAxis=function(b){return arguments.length?(n=b,g.orient(b?"right":"left"),a):n},a.staggerLabels=function(b){return arguments.length?(o=b,a):o},a.tooltips=function(b){return arguments.length?(p=b,a):p},a.tooltipContent=function(b){return arguments.length?(q=b,a):q},a.noData=function(b){return arguments.length?(r=b,a):r},a.transitionDuration=function(b){return arguments.length?(t=b,a):t},a},c.models.distribution=function(){"use strict";function a(c){return c.each(function(a){var c=(e-("x"===g?d.left+d.right:d.top+d.bottom),"x"==g?"y":"x"),k=d3.select(this);b=b||j;var l=k.selectAll("g.nv-distribution").data([a]),m=l.enter().append("g").attr("class","nvd3 nv-distribution");m.append("g");var n=l.select("g");l.attr("transform","translate("+d.left+","+d.top+")");var o=n.selectAll("g.nv-dist").data(function(a){return a},function(a){return a.key});o.enter().append("g"),o.attr("class",function(a,b){return"nv-dist nv-series-"+b}).style("stroke",function(a,b){return i(a,b)});var p=o.selectAll("line.nv-dist"+g).data(function(a){return a.values});p.enter().append("line").attr(g+"1",function(a,c){return b(h(a,c))}).attr(g+"2",function(a,c){return b(h(a,c))}),o.exit().selectAll("line.nv-dist"+g).transition().attr(g+"1",function(a,b){return j(h(a,b))}).attr(g+"2",function(a,b){return j(h(a,b))}).style("stroke-opacity",0).remove(),p.attr("class",function(a,b){return"nv-dist"+g+" nv-dist"+g+"-"+b}).attr(c+"1",0).attr(c+"2",f),p.transition().attr(g+"1",function(a,b){return j(h(a,b))}).attr(g+"2",function(a,b){return j(h(a,b))}),b=j.copy()}),a}var b,d={top:0,right:0,bottom:0,left:0},e=400,f=8,g="x",h=function(a){return a[g]},i=c.utils.defaultColor(),j=d3.scale.linear();return a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(d.top="undefined"!=typeof b.top?b.top:d.top,d.right="undefined"!=typeof b.right?b.right:d.right,d.bottom="undefined"!=typeof b.bottom?b.bottom:d.bottom,d.left="undefined"!=typeof b.left?b.left:d.left,a):d},a.width=function(b){return arguments.length?(e=b,a):e},a.axis=function(b){return arguments.length?(g=b,a):g},a.size=function(b){return arguments.length?(f=b,a):f},a.getData=function(b){return arguments.length?(h=d3.functor(b),a):h},a.scale=function(b){return arguments.length?(j=b,a):j},a.color=function(b){return arguments.length?(i=c.utils.getColor(b),a):i},a},c.models.historicalBarChart=function(){"use strict";function a(c){return c.each(function(r){var y=d3.select(this),z=this,A=(k||parseInt(y.style("width"))||960)-i.left-i.right,B=(l||parseInt(y.style("height"))||400)-i.top-i.bottom;if(a.update=function(){y.transition().duration(w).call(a)},a.container=this,s.disabled=r.map(function(a){return!!a.disabled}),!t){var C;t={};for(C in s)t[C]=s[C]instanceof Array?s[C].slice(0):s[C]}if(!(r&&r.length&&r.filter(function(a){return a.values.length}).length)){var D=y.selectAll(".nv-noData").data([u]);return D.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),D.attr("x",i.left+A/2).attr("y",i.top+B/2).text(function(a){return a}),a}y.selectAll(".nv-noData").remove(),b=e.xScale(),d=e.yScale();var E=y.selectAll("g.nv-wrap.nv-historicalBarChart").data([r]),F=E.enter().append("g").attr("class","nvd3 nv-wrap nv-historicalBarChart").append("g"),G=E.select("g");F.append("g").attr("class","nv-x nv-axis"),F.append("g").attr("class","nv-y nv-axis"),F.append("g").attr("class","nv-barsWrap"),F.append("g").attr("class","nv-legendWrap"),m&&(h.width(A),G.select(".nv-legendWrap").datum(r).call(h),i.top!=h.height()&&(i.top=h.height(),B=(l||parseInt(y.style("height"))||400)-i.top-i.bottom),E.select(".nv-legendWrap").attr("transform","translate(0,"+-i.top+")")),E.attr("transform","translate("+i.left+","+i.top+")"),p&&G.select(".nv-y.nv-axis").attr("transform","translate("+A+",0)"),e.width(A).height(B).color(r.map(function(a,b){return a.color||j(a,b)}).filter(function(a,b){return!r[b].disabled}));var H=G.select(".nv-barsWrap").datum(r.filter(function(a){return!a.disabled}));H.transition().call(e),n&&(f.scale(b).tickSize(-B,0),G.select(".nv-x.nv-axis").attr("transform","translate(0,"+d.range()[0]+")"),G.select(".nv-x.nv-axis").transition().call(f)),o&&(g.scale(d).ticks(B/36).tickSize(-A,0),G.select(".nv-y.nv-axis").transition().call(g)),h.dispatch.on("legendClick",function(b){b.disabled=!b.disabled,r.filter(function(a){return!a.disabled}).length||r.map(function(a){return a.disabled=!1,E.selectAll(".nv-series").classed("disabled",!1),a}),s.disabled=r.map(function(a){return!!a.disabled}),v.stateChange(s),c.transition().call(a)}),h.dispatch.on("legendDblclick",function(b){r.forEach(function(a){a.disabled=!0}),b.disabled=!1,s.disabled=r.map(function(a){return!!a.disabled}),v.stateChange(s),a.update()}),v.on("tooltipShow",function(a){q&&x(a,z.parentNode)}),v.on("changeState",function(b){"undefined"!=typeof b.disabled&&(r.forEach(function(a,c){a.disabled=b.disabled[c]}),s.disabled=b.disabled),a.update()})}),a}var b,d,e=c.models.historicalBar(),f=c.models.axis(),g=c.models.axis(),h=c.models.legend(),i={top:30,right:90,bottom:50,left:90},j=c.utils.defaultColor(),k=null,l=null,m=!1,n=!0,o=!0,p=!1,q=!0,r=function(a,b,c){return"<h3>"+a+"</h3>"+"<p>"+c+" at "+b+"</p>"},s={},t=null,u="No Data Available.",v=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState"),w=250;f.orient("bottom").tickPadding(7),g.orient(p?"right":"left");var x=function(b,d){if(d){var h=d3.select(d).select("svg"),i=h.node()?h.attr("viewBox"):null;if(i){i=i.split(" ");var j=parseInt(h.style("width"))/i[2];b.pos[0]=b.pos[0]*j,b.pos[1]=b.pos[1]*j}}var k=b.pos[0]+(d.offsetLeft||0),l=b.pos[1]+(d.offsetTop||0),m=f.tickFormat()(e.x()(b.point,b.pointIndex)),n=g.tickFormat()(e.y()(b.point,b.pointIndex)),o=r(b.series.key,m,n,b,a);c.tooltip.show([k,l],o,null,null,d)};return e.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+i.left,a.pos[1]+i.top],v.tooltipShow(a)}),e.dispatch.on("elementMouseout.tooltip",function(a){v.tooltipHide(a)}),v.on("tooltipHide",function(){q&&c.tooltip.cleanup()}),a.dispatch=v,a.bars=e,a.legend=h,a.xAxis=f,a.yAxis=g,d3.rebind(a,e,"defined","isArea","x","y","size","xScale","yScale","xDomain","yDomain","xRange","yRange","forceX","forceY","interactive","clipEdge","clipVoronoi","id","interpolate","highlightPoint","clearHighlights","interactive"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(i.top="undefined"!=typeof b.top?b.top:i.top,i.right="undefined"!=typeof b.right?b.right:i.right,i.bottom="undefined"!=typeof b.bottom?b.bottom:i.bottom,i.left="undefined"!=typeof b.left?b.left:i.left,a):i},a.width=function(b){return arguments.length?(k=b,a):k},a.height=function(b){return arguments.length?(l=b,a):l},a.color=function(b){return arguments.length?(j=c.utils.getColor(b),h.color(j),a):j},a.showLegend=function(b){return arguments.length?(m=b,a):m},a.showXAxis=function(b){return arguments.length?(n=b,a):n},a.showYAxis=function(b){return arguments.length?(o=b,a):o},a.rightAlignYAxis=function(b){return arguments.length?(p=b,g.orient(b?"right":"left"),a):p},a.tooltips=function(b){return arguments.length?(q=b,a):q},a.tooltipContent=function(b){return arguments.length?(r=b,a):r},a.state=function(b){return arguments.length?(s=b,a):s},a.defaultState=function(b){return arguments.length?(t=b,a):t},a.noData=function(b){return arguments.length?(u=b,a):u},a.transitionDuration=function(b){return arguments.length?(w=b,a):w},a},c.models.indentedTree=function(){"use strict";function a(b){return b.each(function(b){function c(b,d,e){return d3.event.stopPropagation(),d3.event.shiftKey&&!e?(d3.event.shiftKey=!1,b.values&&b.values.forEach(function(a){(a.values||a._values)&&c(a,0,!0)}),!0):g(b)?(b.values?(b._values=b.values,b.values=null):(b.values=b._values,b._values=null),a.update(),void 0):!0}function d(a){return a._values&&a._values.length?n:a.values&&a.values.length?o:""}function f(a){return a._values&&a._values.length}function g(a){var b=a.values||a._values;return b&&b.length}var s=1,t=d3.select(this),u=d3.layout.tree().children(function(a){return a.values}).size([e,k]);a.update=function(){t.transition().duration(600).call(a)},b[0]||(b[0]={key:j});var v=u.nodes(b[0]),w=d3.select(this).selectAll("div").data([[v]]),x=w.enter().append("div").attr("class","nvd3 nv-wrap nv-indentedtree"),y=x.append("table"),z=w.select("table").attr("width","100%").attr("class",m);if(h){var A=y.append("thead"),B=A.append("tr");l.forEach(function(a){B.append("th").attr("width",a.width?a.width:"10%").style("text-align","numeric"==a.type?"right":"left").append("span").text(a.label)})}var C=z.selectAll("tbody").data(function(a){return a});C.enter().append("tbody"),s=d3.max(v,function(a){return a.depth}),u.size([e,s*k]);var D=C.selectAll("tr").data(function(a){return a.filter(function(a){return i&&!a.children?i(a):!0})},function(a){return a.id||a.id||++r});D.exit().remove(),D.select("img.nv-treeicon").attr("src",d).classed("folded",f);var E=D.enter().append("tr");l.forEach(function(a,b){var e=E.append("td").style("padding-left",function(a){return(b?0:a.depth*k+12+(d(a)?0:16))+"px"},"important").style("text-align","numeric"==a.type?"right":"left");0==b&&e.append("img").classed("nv-treeicon",!0).classed("nv-folded",f).attr("src",d).style("width","14px").style("height","14px").style("padding","0 1px").style("display",function(a){return d(a)?"inline-block":"none"}).on("click",c),e.each(function(c){!b&&q(c)?d3.select(this).append("a").attr("href",q).attr("class",d3.functor(a.classes)).append("span"):d3.select(this).append("span"),d3.select(this).select("span").attr("class",d3.functor(a.classes)).text(function(b){return a.format?a.format(b):b[a.key]||"-"})}),a.showCount&&(e.append("span").attr("class","nv-childrenCount"),D.selectAll("span.nv-childrenCount").text(function(a){return a.values&&a.values.length||a._values&&a._values.length?"("+(a.values&&a.values.filter(function(a){return i?i(a):!0}).length||a._values&&a._values.filter(function(a){return i?i(a):!0}).length||0)+")":""}))}),D.order().on("click",function(a){p.elementClick({row:this,data:a,pos:[a.x,a.y]})}).on("dblclick",function(a){p.elementDblclick({row:this,data:a,pos:[a.x,a.y]})}).on("mouseover",function(a){p.elementMouseover({row:this,data:a,pos:[a.x,a.y]})}).on("mouseout",function(a){p.elementMouseout({row:this,data:a,pos:[a.x,a.y]})})}),a}var b={top:0,right:0,bottom:0,left:0},d=960,e=500,f=c.utils.defaultColor(),g=Math.floor(1e4*Math.random()),h=!0,i=!1,j="No Data Available.",k=20,l=[{key:"key",label:"Name",type:"text"}],m=null,n="images/grey-plus.png",o="images/grey-minus.png",p=d3.dispatch("elementClick","elementDblclick","elementMouseover","elementMouseout"),q=function(a){return a.url},r=0;return a.options=c.utils.optionsFunc.bind(a),a.margin=function(c){return arguments.length?(b.top="undefined"!=typeof c.top?c.top:b.top,b.right="undefined"!=typeof c.right?c.right:b.right,b.bottom="undefined"!=typeof c.bottom?c.bottom:b.bottom,b.left="undefined"!=typeof c.left?c.left:b.left,a):b},a.width=function(b){return arguments.length?(d=b,a):d},a.height=function(b){return arguments.length?(e=b,a):e},a.color=function(b){return arguments.length?(f=c.utils.getColor(b),scatter.color(f),a):f},a.id=function(b){return arguments.length?(g=b,a):g},a.header=function(b){return arguments.length?(h=b,a):h},a.noData=function(b){return arguments.length?(j=b,a):j},a.filterZero=function(b){return arguments.length?(i=b,a):i},a.columns=function(b){return arguments.length?(l=b,a):l},a.tableClass=function(b){return arguments.length?(m=b,a):m},a.iconOpen=function(b){return arguments.length?(n=b,a):n},a.iconClose=function(b){return arguments.length?(o=b,a):o},a.getUrl=function(b){return arguments.length?(q=b,a):q},a},c.models.legend=function(){"use strict";function a(m){return m.each(function(a){var m=d-b.left-b.right,n=d3.select(this),o=n.selectAll("g.nv-legend").data([a]);o.enter().append("g").attr("class","nvd3 nv-legend").append("g");var p=o.select("g");o.attr("transform","translate("+b.left+","+b.top+")");var q=p.selectAll(".nv-series").data(function(a){return a}),r=q.enter().append("g").attr("class","nv-series").on("mouseover",function(a,b){l.legendMouseover(a,b)}).on("mouseout",function(a,b){l.legendMouseout(a,b)}).on("click",function(b,c){l.legendClick(b,c),j&&(k?(a.forEach(function(a){a.disabled=!0}),b.disabled=!1):(b.disabled=!b.disabled,a.every(function(a){return a.disabled})&&a.forEach(function(a){a.disabled=!1})),l.stateChange({disabled:a.map(function(a){return!!a.disabled})}))}).on("dblclick",function(b,c){l.legendDblclick(b,c),j&&(a.forEach(function(a){a.disabled=!0}),b.disabled=!1,l.stateChange({disabled:a.map(function(a){return!!a.disabled})}))});if(r.append("circle").style("stroke-width",2).attr("class","nv-legend-symbol").attr("r",5),r.append("text").attr("text-anchor","start").attr("class","nv-legend-text").attr("dy",".32em").attr("dx","8"),q.classed("disabled",function(a){return a.disabled}),q.exit().remove(),q.select("circle").style("fill",function(a,b){return a.color||g(a,b)}).style("stroke",function(a,b){return a.color||g(a,b)}),q.select("text").text(f),h){var s=[];q.each(function(){var a,b=d3.select(this).select("text");try{a=b.node().getComputedTextLength()}catch(d){a=c.utils.calcApproxTextWidth(b)}s.push(a+28)});for(var t=0,u=0,v=[];m>u&&t<s.length;)v[t]=s[t],u+=s[t++];for(0===t&&(t=1);u>m&&t>1;){v=[],t--;for(var w=0;w<s.length;w++)s[w]>(v[w%t]||0)&&(v[w%t]=s[w]);u=v.reduce(function(a,b){return a+b})}for(var x=[],y=0,z=0;t>y;y++)x[y]=z,z+=v[y];q.attr("transform",function(a,b){return"translate("+x[b%t]+","+(5+20*Math.floor(b/t))+")"}),i?p.attr("transform","translate("+(d-b.right-u)+","+b.top+")"):p.attr("transform","translate(0,"+b.top+")"),e=b.top+b.bottom+20*Math.ceil(s.length/t)}else{var A,B=5,C=5,D=0;q.attr("transform",function(){var a=d3.select(this).select("text").node().getComputedTextLength()+28;return A=C,d<b.left+b.right+A+a&&(C=A=5,B+=20),C+=a,C>D&&(D=C),"translate("+A+","+B+")"}),p.attr("transform","translate("+(d-b.right-D)+","+b.top+")"),e=b.top+b.bottom+B+15}}),a}var b={top:5,right:0,bottom:5,left:0},d=400,e=20,f=function(a){return a.key},g=c.utils.defaultColor(),h=!0,i=!0,j=!0,k=!1,l=d3.dispatch("legendClick","legendDblclick","legendMouseover","legendMouseout","stateChange");return a.dispatch=l,a.options=c.utils.optionsFunc.bind(a),a.margin=function(c){return arguments.length?(b.top="undefined"!=typeof c.top?c.top:b.top,b.right="undefined"!=typeof c.right?c.right:b.right,b.bottom="undefined"!=typeof c.bottom?c.bottom:b.bottom,b.left="undefined"!=typeof c.left?c.left:b.left,a):b},a.width=function(b){return arguments.length?(d=b,a):d},a.height=function(b){return arguments.length?(e=b,a):e},a.key=function(b){return arguments.length?(f=b,a):f},a.color=function(b){return arguments.length?(g=c.utils.getColor(b),a):g},a.align=function(b){return arguments.length?(h=b,a):h},a.rightAlign=function(b){return arguments.length?(i=b,a):i},a.updateState=function(b){return arguments.length?(j=b,a):j},a.radioButtonMode=function(b){return arguments.length?(k=b,a):k},a},c.models.line=function(){"use strict";function a(r){return r.each(function(a){var r=g-f.left-f.right,s=h-f.top-f.bottom,t=d3.select(this);b=e.xScale(),d=e.yScale(),p=p||b,q=q||d;var u=t.selectAll("g.nv-wrap.nv-line").data([a]),v=u.enter().append("g").attr("class","nvd3 nv-wrap nv-line"),w=v.append("defs"),x=v.append("g"),y=u.select("g");x.append("g").attr("class","nv-groups"),x.append("g").attr("class","nv-scatterWrap"),u.attr("transform","translate("+f.left+","+f.top+")"),e.width(r).height(s);var z=u.select(".nv-scatterWrap");z.transition().call(e),w.append("clipPath").attr("id","nv-edge-clip-"+e.id()).append("rect"),u.select("#nv-edge-clip-"+e.id()+" rect").attr("width",r).attr("height",s),y.attr("clip-path",n?"url(#nv-edge-clip-"+e.id()+")":""),z.attr("clip-path",n?"url(#nv-edge-clip-"+e.id()+")":"");
var A=u.select(".nv-groups").selectAll(".nv-group").data(function(a){return a},function(a){return a.key});A.enter().append("g").style("stroke-opacity",1e-6).style("fill-opacity",1e-6),A.exit().remove(),A.attr("class",function(a,b){return"nv-group nv-series-"+b}).classed("hover",function(a){return a.hover}).style("fill",function(a,b){return i(a,b)}).style("stroke",function(a,b){return i(a,b)}),A.transition().style("stroke-opacity",1).style("fill-opacity",.5);var B=A.selectAll("path.nv-area").data(function(a){return m(a)?[a]:[]});B.enter().append("path").attr("class","nv-area").attr("d",function(a){return d3.svg.area().interpolate(o).defined(l).x(function(a,b){return c.utils.NaNtoZero(p(j(a,b)))}).y0(function(a,b){return c.utils.NaNtoZero(q(k(a,b)))}).y1(function(){return q(d.domain()[0]<=0?d.domain()[1]>=0?0:d.domain()[1]:d.domain()[0])}).apply(this,[a.values])}),A.exit().selectAll("path.nv-area").remove(),B.transition().attr("d",function(a){return d3.svg.area().interpolate(o).defined(l).x(function(a,d){return c.utils.NaNtoZero(b(j(a,d)))}).y0(function(a,b){return c.utils.NaNtoZero(d(k(a,b)))}).y1(function(){return d(d.domain()[0]<=0?d.domain()[1]>=0?0:d.domain()[1]:d.domain()[0])}).apply(this,[a.values])});var C=A.selectAll("path.nv-line").data(function(a){return[a.values]});C.enter().append("path").attr("class","nv-line").attr("d",d3.svg.line().interpolate(o).defined(l).x(function(a,b){return c.utils.NaNtoZero(p(j(a,b)))}).y(function(a,b){return c.utils.NaNtoZero(q(k(a,b)))})),C.transition().attr("d",d3.svg.line().interpolate(o).defined(l).x(function(a,d){return c.utils.NaNtoZero(b(j(a,d)))}).y(function(a,b){return c.utils.NaNtoZero(d(k(a,b)))})),p=b.copy(),q=d.copy()}),a}var b,d,e=c.models.scatter(),f={top:0,right:0,bottom:0,left:0},g=960,h=500,i=c.utils.defaultColor(),j=function(a){return a.x},k=function(a){return a.y},l=function(a,b){return!isNaN(k(a,b))&&null!==k(a,b)},m=function(a){return a.area},n=!1,o="linear";e.size(16).sizeDomain([16,256]);var p,q;return a.dispatch=e.dispatch,a.scatter=e,d3.rebind(a,e,"id","interactive","size","xScale","yScale","zScale","xDomain","yDomain","xRange","yRange","sizeDomain","forceX","forceY","forceSize","clipVoronoi","useVoronoi","clipRadius","padData","highlightPoint","clearHighlights"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(f.top="undefined"!=typeof b.top?b.top:f.top,f.right="undefined"!=typeof b.right?b.right:f.right,f.bottom="undefined"!=typeof b.bottom?b.bottom:f.bottom,f.left="undefined"!=typeof b.left?b.left:f.left,a):f},a.width=function(b){return arguments.length?(g=b,a):g},a.height=function(b){return arguments.length?(h=b,a):h},a.x=function(b){return arguments.length?(j=b,e.x(b),a):j},a.y=function(b){return arguments.length?(k=b,e.y(b),a):k},a.clipEdge=function(b){return arguments.length?(n=b,a):n},a.color=function(b){return arguments.length?(i=c.utils.getColor(b),e.color(i),a):i},a.interpolate=function(b){return arguments.length?(o=b,a):o},a.defined=function(b){return arguments.length?(l=b,a):l},a.isArea=function(b){return arguments.length?(m=d3.functor(b),a):m},a},c.models.lineChart=function(){"use strict";function a(t){return t.each(function(t){var A=d3.select(this),B=this,C=(l||parseInt(A.style("width"))||960)-j.left-j.right,D=(m||parseInt(A.style("height"))||400)-j.top-j.bottom;if(a.update=function(){A.transition().duration(y).call(a)},a.container=this,u.disabled=t.map(function(a){return!!a.disabled}),!v){var E;v={};for(E in u)v[E]=u[E]instanceof Array?u[E].slice(0):u[E]}if(!(t&&t.length&&t.filter(function(a){return a.values.length}).length)){var F=A.selectAll(".nv-noData").data([w]);return F.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),F.attr("x",j.left+C/2).attr("y",j.top+D/2).text(function(a){return a}),a}A.selectAll(".nv-noData").remove(),b=e.xScale(),d=e.yScale();var G=A.selectAll("g.nv-wrap.nv-lineChart").data([t]),H=G.enter().append("g").attr("class","nvd3 nv-wrap nv-lineChart").append("g"),I=G.select("g");H.append("rect").style("opacity",0),H.append("g").attr("class","nv-x nv-axis"),H.append("g").attr("class","nv-y nv-axis"),H.append("g").attr("class","nv-linesWrap"),H.append("g").attr("class","nv-legendWrap"),H.append("g").attr("class","nv-interactive"),I.select("rect").attr("width",C).attr("height",D),n&&(h.width(C),I.select(".nv-legendWrap").datum(t).call(h),j.top!=h.height()&&(j.top=h.height(),D=(m||parseInt(A.style("height"))||400)-j.top-j.bottom),G.select(".nv-legendWrap").attr("transform","translate(0,"+-j.top+")")),G.attr("transform","translate("+j.left+","+j.top+")"),q&&I.select(".nv-y.nv-axis").attr("transform","translate("+C+",0)"),r&&(i.width(C).height(D).margin({left:j.left,top:j.top}).svgContainer(A).xScale(b),G.select(".nv-interactive").call(i)),e.width(C).height(D).color(t.map(function(a,b){return a.color||k(a,b)}).filter(function(a,b){return!t[b].disabled}));var J=I.select(".nv-linesWrap").datum(t.filter(function(a){return!a.disabled}));J.transition().call(e),o&&(f.scale(b).ticks(C/100).tickSize(-D,0),I.select(".nv-x.nv-axis").attr("transform","translate(0,"+d.range()[0]+")"),I.select(".nv-x.nv-axis").transition().call(f)),p&&(g.scale(d).ticks(D/36).tickSize(-C,0),I.select(".nv-y.nv-axis").transition().call(g)),h.dispatch.on("stateChange",function(b){u=b,x.stateChange(u),a.update()}),i.dispatch.on("elementMousemove",function(b){e.clearHighlights();var d,h,l,m=[];if(t.filter(function(a,b){return a.seriesIndex=b,!a.disabled}).forEach(function(f,g){h=c.interactiveBisect(f.values,b.pointXValue,a.x()),e.highlightPoint(g,h,!0);var i=f.values[h];"undefined"!=typeof i&&("undefined"==typeof d&&(d=i),"undefined"==typeof l&&(l=a.xScale()(a.x()(i,h))),m.push({key:f.key,value:a.y()(i,h),color:k(f,f.seriesIndex)}))}),m.length>2){var n=a.yScale().invert(b.mouseY),o=Math.abs(a.yScale().domain()[0]-a.yScale().domain()[1]),p=.03*o,q=c.nearestValueIndex(m.map(function(a){return a.value}),n,p);null!==q&&(m[q].highlight=!0)}var r=f.tickFormat()(a.x()(d,h));i.tooltip.position({left:l+j.left,top:b.mouseY+j.top}).chartContainer(B.parentNode).enabled(s).valueFormatter(function(a){return g.tickFormat()(a)}).data({value:r,series:m})(),i.renderGuideLine(l)}),i.dispatch.on("elementMouseout",function(){x.tooltipHide(),e.clearHighlights()}),x.on("tooltipShow",function(a){s&&z(a,B.parentNode)}),x.on("changeState",function(b){"undefined"!=typeof b.disabled&&(t.forEach(function(a,c){a.disabled=b.disabled[c]}),u.disabled=b.disabled),a.update()})}),a}var b,d,e=c.models.line(),f=c.models.axis(),g=c.models.axis(),h=c.models.legend(),i=c.interactiveGuideline(),j={top:30,right:20,bottom:50,left:60},k=c.utils.defaultColor(),l=null,m=null,n=!0,o=!0,p=!0,q=!1,r=!1,s=!0,t=function(a,b,c){return"<h3>"+a+"</h3>"+"<p>"+c+" at "+b+"</p>"},u={},v=null,w="No Data Available.",x=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState"),y=250;f.orient("bottom").tickPadding(7),g.orient(q?"right":"left");var z=function(b,d){var h=b.pos[0]+(d.offsetLeft||0),i=b.pos[1]+(d.offsetTop||0),j=f.tickFormat()(e.x()(b.point,b.pointIndex)),k=g.tickFormat()(e.y()(b.point,b.pointIndex)),l=t(b.series.key,j,k,b,a);c.tooltip.show([h,i],l,null,null,d)};return e.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+j.left,a.pos[1]+j.top],x.tooltipShow(a)}),e.dispatch.on("elementMouseout.tooltip",function(a){x.tooltipHide(a)}),x.on("tooltipHide",function(){s&&c.tooltip.cleanup()}),a.dispatch=x,a.lines=e,a.legend=h,a.xAxis=f,a.yAxis=g,a.interactiveLayer=i,d3.rebind(a,e,"defined","isArea","x","y","size","xScale","yScale","xDomain","yDomain","xRange","yRange","forceX","forceY","interactive","clipEdge","clipVoronoi","useVoronoi","id","interpolate"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(j.top="undefined"!=typeof b.top?b.top:j.top,j.right="undefined"!=typeof b.right?b.right:j.right,j.bottom="undefined"!=typeof b.bottom?b.bottom:j.bottom,j.left="undefined"!=typeof b.left?b.left:j.left,a):j},a.width=function(b){return arguments.length?(l=b,a):l},a.height=function(b){return arguments.length?(m=b,a):m},a.color=function(b){return arguments.length?(k=c.utils.getColor(b),h.color(k),a):k},a.showLegend=function(b){return arguments.length?(n=b,a):n},a.showXAxis=function(b){return arguments.length?(o=b,a):o},a.showYAxis=function(b){return arguments.length?(p=b,a):p},a.rightAlignYAxis=function(b){return arguments.length?(q=b,g.orient(b?"right":"left"),a):q},a.useInteractiveGuideline=function(b){return arguments.length?(r=b,b===!0&&(a.interactive(!1),a.useVoronoi(!1)),a):r},a.tooltips=function(b){return arguments.length?(s=b,a):s},a.tooltipContent=function(b){return arguments.length?(t=b,a):t},a.state=function(b){return arguments.length?(u=b,a):u},a.defaultState=function(b){return arguments.length?(v=b,a):v},a.noData=function(b){return arguments.length?(w=b,a):w},a.transitionDuration=function(b){return arguments.length?(y=b,a):y},a},c.models.linePlusBarChart=function(){"use strict";function a(c){return c.each(function(c){var o=d3.select(this),p=this,t=(m||parseInt(o.style("width"))||960)-l.left-l.right,z=(n||parseInt(o.style("height"))||400)-l.top-l.bottom;if(a.update=function(){o.transition().call(a)},u.disabled=c.map(function(a){return!!a.disabled}),!v){var A;v={};for(A in u)v[A]=u[A]instanceof Array?u[A].slice(0):u[A]}if(!(c&&c.length&&c.filter(function(a){return a.values.length}).length)){var B=o.selectAll(".nv-noData").data([w]);return B.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),B.attr("x",l.left+t/2).attr("y",l.top+z/2).text(function(a){return a}),a}o.selectAll(".nv-noData").remove();var C=c.filter(function(a){return!a.disabled&&a.bar}),D=c.filter(function(a){return!a.bar});b=D.filter(function(a){return!a.disabled}).length&&D.filter(function(a){return!a.disabled})[0].values.length?f.xScale():g.xScale(),d=g.yScale(),e=f.yScale();var E=d3.select(this).selectAll("g.nv-wrap.nv-linePlusBar").data([c]),F=E.enter().append("g").attr("class","nvd3 nv-wrap nv-linePlusBar").append("g"),G=E.select("g");F.append("g").attr("class","nv-x nv-axis"),F.append("g").attr("class","nv-y1 nv-axis"),F.append("g").attr("class","nv-y2 nv-axis"),F.append("g").attr("class","nv-barsWrap"),F.append("g").attr("class","nv-linesWrap"),F.append("g").attr("class","nv-legendWrap"),r&&(k.width(t/2),G.select(".nv-legendWrap").datum(c.map(function(a){return a.originalKey=void 0===a.originalKey?a.key:a.originalKey,a.key=a.originalKey+(a.bar?" (left axis)":" (right axis)"),a})).call(k),l.top!=k.height()&&(l.top=k.height(),z=(n||parseInt(o.style("height"))||400)-l.top-l.bottom),G.select(".nv-legendWrap").attr("transform","translate("+t/2+","+-l.top+")")),E.attr("transform","translate("+l.left+","+l.top+")"),f.width(t).height(z).color(c.map(function(a,b){return a.color||q(a,b)}).filter(function(a,b){return!c[b].disabled&&!c[b].bar})),g.width(t).height(z).color(c.map(function(a,b){return a.color||q(a,b)}).filter(function(a,b){return!c[b].disabled&&c[b].bar}));var H=G.select(".nv-barsWrap").datum(C.length?C:[{values:[]}]),I=G.select(".nv-linesWrap").datum(D[0]&&!D[0].disabled?D:[{values:[]}]);d3.transition(H).call(g),d3.transition(I).call(f),h.scale(b).ticks(t/100).tickSize(-z,0),G.select(".nv-x.nv-axis").attr("transform","translate(0,"+d.range()[0]+")"),d3.transition(G.select(".nv-x.nv-axis")).call(h),i.scale(d).ticks(z/36).tickSize(-t,0),d3.transition(G.select(".nv-y1.nv-axis")).style("opacity",C.length?1:0).call(i),j.scale(e).ticks(z/36).tickSize(C.length?0:-t,0),G.select(".nv-y2.nv-axis").style("opacity",D.length?1:0).attr("transform","translate("+t+",0)"),d3.transition(G.select(".nv-y2.nv-axis")).call(j),k.dispatch.on("stateChange",function(b){u=b,x.stateChange(u),a.update()}),x.on("tooltipShow",function(a){s&&y(a,p.parentNode)}),x.on("changeState",function(b){"undefined"!=typeof b.disabled&&(c.forEach(function(a,c){a.disabled=b.disabled[c]}),u.disabled=b.disabled),a.update()})}),a}var b,d,e,f=c.models.line(),g=c.models.historicalBar(),h=c.models.axis(),i=c.models.axis(),j=c.models.axis(),k=c.models.legend(),l={top:30,right:60,bottom:50,left:60},m=null,n=null,o=function(a){return a.x},p=function(a){return a.y},q=c.utils.defaultColor(),r=!0,s=!0,t=function(a,b,c){return"<h3>"+a+"</h3>"+"<p>"+c+" at "+b+"</p>"},u={},v=null,w="No Data Available.",x=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState");g.padData(!0),f.clipEdge(!1).padData(!0),h.orient("bottom").tickPadding(7).highlightZero(!1),i.orient("left"),j.orient("right");var y=function(b,d){var e=b.pos[0]+(d.offsetLeft||0),g=b.pos[1]+(d.offsetTop||0),k=h.tickFormat()(f.x()(b.point,b.pointIndex)),l=(b.series.bar?i:j).tickFormat()(f.y()(b.point,b.pointIndex)),m=t(b.series.key,k,l,b,a);c.tooltip.show([e,g],m,b.value<0?"n":"s",null,d)};return f.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+l.left,a.pos[1]+l.top],x.tooltipShow(a)}),f.dispatch.on("elementMouseout.tooltip",function(a){x.tooltipHide(a)}),g.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+l.left,a.pos[1]+l.top],x.tooltipShow(a)}),g.dispatch.on("elementMouseout.tooltip",function(a){x.tooltipHide(a)}),x.on("tooltipHide",function(){s&&c.tooltip.cleanup()}),a.dispatch=x,a.legend=k,a.lines=f,a.bars=g,a.xAxis=h,a.y1Axis=i,a.y2Axis=j,d3.rebind(a,f,"defined","size","clipVoronoi","interpolate"),a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(o=b,f.x(b),g.x(b),a):o},a.y=function(b){return arguments.length?(p=b,f.y(b),g.y(b),a):p},a.margin=function(b){return arguments.length?(l.top="undefined"!=typeof b.top?b.top:l.top,l.right="undefined"!=typeof b.right?b.right:l.right,l.bottom="undefined"!=typeof b.bottom?b.bottom:l.bottom,l.left="undefined"!=typeof b.left?b.left:l.left,a):l},a.width=function(b){return arguments.length?(m=b,a):m},a.height=function(b){return arguments.length?(n=b,a):n},a.color=function(b){return arguments.length?(q=c.utils.getColor(b),k.color(q),a):q},a.showLegend=function(b){return arguments.length?(r=b,a):r},a.tooltips=function(b){return arguments.length?(s=b,a):s},a.tooltipContent=function(b){return arguments.length?(t=b,a):t},a.state=function(b){return arguments.length?(u=b,a):u},a.defaultState=function(b){return arguments.length?(v=b,a):v},a.noData=function(b){return arguments.length?(w=b,a):w},a},c.models.lineWithFocusChart=function(){"use strict";function a(c){return c.each(function(c){function x(a){var b=+("e"==a),c=b?1:-1,d=I/3;return"M"+.5*c+","+d+"A6,6 0 0 "+b+" "+6.5*c+","+(d+6)+"V"+(2*d-6)+"A6,6 0 0 "+b+" "+.5*c+","+2*d+"Z"+"M"+2.5*c+","+(d+8)+"V"+(2*d-8)+"M"+4.5*c+","+(d+8)+"V"+(2*d-8)}function C(){n.empty()||n.extent(v),Q.data([n.empty()?e.domain():v]).each(function(a){var c=e(a[0])-b.range()[0],d=b.range()[1]-e(a[1]);d3.select(this).select(".left").attr("width",0>c?0:c),d3.select(this).select(".right").attr("x",e(a[1])).attr("width",0>d?0:d)})}function D(){v=n.empty()?null:n.extent();var a=n.empty()?e.domain():n.extent();if(!(Math.abs(a[0]-a[1])<=1)){z.brush({extent:a,brush:n}),C();var b=M.select(".nv-focus .nv-linesWrap").datum(c.filter(function(a){return!a.disabled}).map(function(b){return{key:b.key,values:b.values.filter(function(b,c){return g.x()(b,c)>=a[0]&&g.x()(b,c)<=a[1]})}}));b.transition().duration(A).call(g),M.select(".nv-focus .nv-x.nv-axis").transition().duration(A).call(i),M.select(".nv-focus .nv-y.nv-axis").transition().duration(A).call(j)}}var E=d3.select(this),F=this,G=(r||parseInt(E.style("width"))||960)-o.left-o.right,H=(s||parseInt(E.style("height"))||400)-o.top-o.bottom-t,I=t-p.top-p.bottom;if(a.update=function(){E.transition().duration(A).call(a)},a.container=this,!(c&&c.length&&c.filter(function(a){return a.values.length}).length)){var J=E.selectAll(".nv-noData").data([y]);return J.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),J.attr("x",o.left+G/2).attr("y",o.top+H/2).text(function(a){return a}),a}E.selectAll(".nv-noData").remove(),b=g.xScale(),d=g.yScale(),e=h.xScale(),f=h.yScale();var K=E.selectAll("g.nv-wrap.nv-lineWithFocusChart").data([c]),L=K.enter().append("g").attr("class","nvd3 nv-wrap nv-lineWithFocusChart").append("g"),M=K.select("g");L.append("g").attr("class","nv-legendWrap");var N=L.append("g").attr("class","nv-focus");N.append("g").attr("class","nv-x nv-axis"),N.append("g").attr("class","nv-y nv-axis"),N.append("g").attr("class","nv-linesWrap");var O=L.append("g").attr("class","nv-context");O.append("g").attr("class","nv-x nv-axis"),O.append("g").attr("class","nv-y nv-axis"),O.append("g").attr("class","nv-linesWrap"),O.append("g").attr("class","nv-brushBackground"),O.append("g").attr("class","nv-x nv-brush"),u&&(m.width(G),M.select(".nv-legendWrap").datum(c).call(m),o.top!=m.height()&&(o.top=m.height(),H=(s||parseInt(E.style("height"))||400)-o.top-o.bottom-t),M.select(".nv-legendWrap").attr("transform","translate(0,"+-o.top+")")),K.attr("transform","translate("+o.left+","+o.top+")"),g.width(G).height(H).color(c.map(function(a,b){return a.color||q(a,b)}).filter(function(a,b){return!c[b].disabled})),h.defined(g.defined()).width(G).height(I).color(c.map(function(a,b){return a.color||q(a,b)}).filter(function(a,b){return!c[b].disabled})),M.select(".nv-context").attr("transform","translate(0,"+(H+o.bottom+p.top)+")");var P=M.select(".nv-context .nv-linesWrap").datum(c.filter(function(a){return!a.disabled}));d3.transition(P).call(h),i.scale(b).ticks(G/100).tickSize(-H,0),j.scale(d).ticks(H/36).tickSize(-G,0),M.select(".nv-focus .nv-x.nv-axis").attr("transform","translate(0,"+H+")"),n.x(e).on("brush",function(){var b=a.transitionDuration();a.transitionDuration(0),D(),a.transitionDuration(b)}),v&&n.extent(v);var Q=M.select(".nv-brushBackground").selectAll("g").data([v||n.extent()]),R=Q.enter().append("g");R.append("rect").attr("class","left").attr("x",0).attr("y",0).attr("height",I),R.append("rect").attr("class","right").attr("x",0).attr("y",0).attr("height",I);var S=M.select(".nv-x.nv-brush").call(n);S.selectAll("rect").attr("height",I),S.selectAll(".resize").append("path").attr("d",x),D(),k.scale(e).ticks(G/100).tickSize(-I,0),M.select(".nv-context .nv-x.nv-axis").attr("transform","translate(0,"+f.range()[0]+")"),d3.transition(M.select(".nv-context .nv-x.nv-axis")).call(k),l.scale(f).ticks(I/36).tickSize(-G,0),d3.transition(M.select(".nv-context .nv-y.nv-axis")).call(l),M.select(".nv-context .nv-x.nv-axis").attr("transform","translate(0,"+f.range()[0]+")"),m.dispatch.on("stateChange",function(){a.update()}),z.on("tooltipShow",function(a){w&&B(a,F.parentNode)})}),a}var b,d,e,f,g=c.models.line(),h=c.models.line(),i=c.models.axis(),j=c.models.axis(),k=c.models.axis(),l=c.models.axis(),m=c.models.legend(),n=d3.svg.brush(),o={top:30,right:30,bottom:30,left:60},p={top:0,right:30,bottom:20,left:60},q=c.utils.defaultColor(),r=null,s=null,t=100,u=!0,v=null,w=!0,x=function(a,b,c){return"<h3>"+a+"</h3>"+"<p>"+c+" at "+b+"</p>"},y="No Data Available.",z=d3.dispatch("tooltipShow","tooltipHide","brush"),A=250;g.clipEdge(!0),h.interactive(!1),i.orient("bottom").tickPadding(5),j.orient("left"),k.orient("bottom").tickPadding(5),l.orient("left");var B=function(b,d){var e=b.pos[0]+(d.offsetLeft||0),f=b.pos[1]+(d.offsetTop||0),h=i.tickFormat()(g.x()(b.point,b.pointIndex)),k=j.tickFormat()(g.y()(b.point,b.pointIndex)),l=x(b.series.key,h,k,b,a);c.tooltip.show([e,f],l,null,null,d)};return g.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+o.left,a.pos[1]+o.top],z.tooltipShow(a)}),g.dispatch.on("elementMouseout.tooltip",function(a){z.tooltipHide(a)}),z.on("tooltipHide",function(){w&&c.tooltip.cleanup()}),a.dispatch=z,a.legend=m,a.lines=g,a.lines2=h,a.xAxis=i,a.yAxis=j,a.x2Axis=k,a.y2Axis=l,d3.rebind(a,g,"defined","isArea","size","xDomain","yDomain","xRange","yRange","forceX","forceY","interactive","clipEdge","clipVoronoi","id"),a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(g.x(b),h.x(b),a):g.x},a.y=function(b){return arguments.length?(g.y(b),h.y(b),a):g.y},a.margin=function(b){return arguments.length?(o.top="undefined"!=typeof b.top?b.top:o.top,o.right="undefined"!=typeof b.right?b.right:o.right,o.bottom="undefined"!=typeof b.bottom?b.bottom:o.bottom,o.left="undefined"!=typeof b.left?b.left:o.left,a):o},a.margin2=function(b){return arguments.length?(p=b,a):p},a.width=function(b){return arguments.length?(r=b,a):r},a.height=function(b){return arguments.length?(s=b,a):s},a.height2=function(b){return arguments.length?(t=b,a):t},a.color=function(b){return arguments.length?(q=c.utils.getColor(b),m.color(q),a):q},a.showLegend=function(b){return arguments.length?(u=b,a):u},a.tooltips=function(b){return arguments.length?(w=b,a):w},a.tooltipContent=function(b){return arguments.length?(x=b,a):x},a.interpolate=function(b){return arguments.length?(g.interpolate(b),h.interpolate(b),a):g.interpolate()},a.noData=function(b){return arguments.length?(y=b,a):y},a.xTickFormat=function(b){return arguments.length?(i.tickFormat(b),k.tickFormat(b),a):i.tickFormat()},a.yTickFormat=function(b){return arguments.length?(j.tickFormat(b),l.tickFormat(b),a):j.tickFormat()},a.brushExtent=function(b){return arguments.length?(v=b,a):v},a.transitionDuration=function(b){return arguments.length?(A=b,a):A},a},c.models.linePlusBarWithFocusChart=function(){"use strict";function a(c){return c.each(function(c){function G(a){var b=+("e"==a),c=b?1:-1,d=R/3;return"M"+.5*c+","+d+"A6,6 0 0 "+b+" "+6.5*c+","+(d+6)+"V"+(2*d-6)+"A6,6 0 0 "+b+" "+.5*c+","+2*d+"Z"+"M"+2.5*c+","+(d+8)+"V"+(2*d-8)+"M"+4.5*c+","+(d+8)+"V"+(2*d-8)}function L(){u.empty()||u.extent(E),cb.data([u.empty()?e.domain():E]).each(function(a){var b=e(a[0])-e.range()[0],c=e.range()[1]-e(a[1]);d3.select(this).select(".left").attr("width",0>b?0:b),d3.select(this).select(".right").attr("x",e(a[1])).attr("width",0>c?0:c)})}function M(){E=u.empty()?null:u.extent(),b=u.empty()?e.domain():u.extent(),I.brush({extent:b,brush:u}),L(),l.width(P).height(Q).color(c.map(function(a,b){return a.color||C(a,b)}).filter(function(a,b){return!c[b].disabled&&c[b].bar})),j.width(P).height(Q).color(c.map(function(a,b){return a.color||C(a,b)}).filter(function(a,b){return!c[b].disabled&&!c[b].bar}));var a=Z.select(".nv-focus .nv-barsWrap").datum(T.length?T.map(function(a){return{key:a.key,values:a.values.filter(function(a,c){return l.x()(a,c)>=b[0]&&l.x()(a,c)<=b[1]})}}):[{values:[]}]),h=Z.select(".nv-focus .nv-linesWrap").datum(U[0].disabled?[{values:[]}]:U.map(function(a){return{key:a.key,values:a.values.filter(function(a,c){return j.x()(a,c)>=b[0]&&j.x()(a,c)<=b[1]})}}));d=T.length?l.xScale():j.xScale(),n.scale(d).ticks(P/100).tickSize(-Q,0),n.domain([Math.ceil(b[0]),Math.floor(b[1])]),Z.select(".nv-x.nv-axis").transition().duration(J).call(n),a.transition().duration(J).call(l),h.transition().duration(J).call(j),Z.select(".nv-focus .nv-x.nv-axis").attr("transform","translate(0,"+f.range()[0]+")"),p.scale(f).ticks(Q/36).tickSize(-P,0),Z.select(".nv-focus .nv-y1.nv-axis").style("opacity",T.length?1:0),q.scale(g).ticks(Q/36).tickSize(T.length?0:-P,0),Z.select(".nv-focus .nv-y2.nv-axis").style("opacity",U.length?1:0).attr("transform","translate("+d.range()[1]+",0)"),Z.select(".nv-focus .nv-y1.nv-axis").transition().duration(J).call(p),Z.select(".nv-focus .nv-y2.nv-axis").transition().duration(J).call(q)}var N=d3.select(this),O=this,P=(x||parseInt(N.style("width"))||960)-v.left-v.right,Q=(y||parseInt(N.style("height"))||400)-v.top-v.bottom-z,R=z-w.top-w.bottom;if(a.update=function(){N.transition().duration(J).call(a)},a.container=this,!(c&&c.length&&c.filter(function(a){return a.values.length}).length)){var S=N.selectAll(".nv-noData").data([H]);return S.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),S.attr("x",v.left+P/2).attr("y",v.top+Q/2).text(function(a){return a}),a}N.selectAll(".nv-noData").remove();var T=c.filter(function(a){return!a.disabled&&a.bar}),U=c.filter(function(a){return!a.bar});d=l.xScale(),e=o.scale(),f=l.yScale(),g=j.yScale(),h=m.yScale(),i=k.yScale();var V=c.filter(function(a){return!a.disabled&&a.bar}).map(function(a){return a.values.map(function(a,b){return{x:A(a,b),y:B(a,b)}})}),W=c.filter(function(a){return!a.disabled&&!a.bar}).map(function(a){return a.values.map(function(a,b){return{x:A(a,b),y:B(a,b)}})});d.range([0,P]),e.domain(d3.extent(d3.merge(V.concat(W)),function(a){return a.x})).range([0,P]);var X=N.selectAll("g.nv-wrap.nv-linePlusBar").data([c]),Y=X.enter().append("g").attr("class","nvd3 nv-wrap nv-linePlusBar").append("g"),Z=X.select("g");Y.append("g").attr("class","nv-legendWrap");var $=Y.append("g").attr("class","nv-focus");$.append("g").attr("class","nv-x nv-axis"),$.append("g").attr("class","nv-y1 nv-axis"),$.append("g").attr("class","nv-y2 nv-axis"),$.append("g").attr("class","nv-barsWrap"),$.append("g").attr("class","nv-linesWrap");var _=Y.append("g").attr("class","nv-context");_.append("g").attr("class","nv-x nv-axis"),_.append("g").attr("class","nv-y1 nv-axis"),_.append("g").attr("class","nv-y2 nv-axis"),_.append("g").attr("class","nv-barsWrap"),_.append("g").attr("class","nv-linesWrap"),_.append("g").attr("class","nv-brushBackground"),_.append("g").attr("class","nv-x nv-brush"),D&&(t.width(P/2),Z.select(".nv-legendWrap").datum(c.map(function(a){return a.originalKey=void 0===a.originalKey?a.key:a.originalKey,a.key=a.originalKey+(a.bar?" (left axis)":" (right axis)"),a})).call(t),v.top!=t.height()&&(v.top=t.height(),Q=(y||parseInt(N.style("height"))||400)-v.top-v.bottom-z),Z.select(".nv-legendWrap").attr("transform","translate("+P/2+","+-v.top+")")),X.attr("transform","translate("+v.left+","+v.top+")"),m.width(P).height(R).color(c.map(function(a,b){return a.color||C(a,b)}).filter(function(a,b){return!c[b].disabled&&c[b].bar})),k.width(P).height(R).color(c.map(function(a,b){return a.color||C(a,b)}).filter(function(a,b){return!c[b].disabled&&!c[b].bar}));var ab=Z.select(".nv-context .nv-barsWrap").datum(T.length?T:[{values:[]}]),bb=Z.select(".nv-context .nv-linesWrap").datum(U[0].disabled?[{values:[]}]:U);Z.select(".nv-context").attr("transform","translate(0,"+(Q+v.bottom+w.top)+")"),ab.transition().call(m),bb.transition().call(k),u.x(e).on("brush",M),E&&u.extent(E);var cb=Z.select(".nv-brushBackground").selectAll("g").data([E||u.extent()]),db=cb.enter().append("g");db.append("rect").attr("class","left").attr("x",0).attr("y",0).attr("height",R),db.append("rect").attr("class","right").attr("x",0).attr("y",0).attr("height",R);var eb=Z.select(".nv-x.nv-brush").call(u);eb.selectAll("rect").attr("height",R),eb.selectAll(".resize").append("path").attr("d",G),o.ticks(P/100).tickSize(-R,0),Z.select(".nv-context .nv-x.nv-axis").attr("transform","translate(0,"+h.range()[0]+")"),Z.select(".nv-context .nv-x.nv-axis").transition().call(o),r.scale(h).ticks(R/36).tickSize(-P,0),Z.select(".nv-context .nv-y1.nv-axis").style("opacity",T.length?1:0).attr("transform","translate(0,"+e.range()[0]+")"),Z.select(".nv-context .nv-y1.nv-axis").transition().call(r),s.scale(i).ticks(R/36).tickSize(T.length?0:-P,0),Z.select(".nv-context .nv-y2.nv-axis").style("opacity",U.length?1:0).attr("transform","translate("+e.range()[1]+",0)"),Z.select(".nv-context .nv-y2.nv-axis").transition().call(s),t.dispatch.on("stateChange",function(){a.update()}),I.on("tooltipShow",function(a){F&&K(a,O.parentNode)}),M()}),a}var b,d,e,f,g,h,i,j=c.models.line(),k=c.models.line(),l=c.models.historicalBar(),m=c.models.historicalBar(),n=c.models.axis(),o=c.models.axis(),p=c.models.axis(),q=c.models.axis(),r=c.models.axis(),s=c.models.axis(),t=c.models.legend(),u=d3.svg.brush(),v={top:30,right:30,bottom:30,left:60},w={top:0,right:30,bottom:20,left:60},x=null,y=null,z=100,A=function(a){return a.x},B=function(a){return a.y},C=c.utils.defaultColor(),D=!0,E=null,F=!0,G=function(a,b,c){return"<h3>"+a+"</h3>"+"<p>"+c+" at "+b+"</p>"},H="No Data Available.",I=d3.dispatch("tooltipShow","tooltipHide","brush"),J=0;j.clipEdge(!0),k.interactive(!1),n.orient("bottom").tickPadding(5),p.orient("left"),q.orient("right"),o.orient("bottom").tickPadding(5),r.orient("left"),s.orient("right");var K=function(d,e){b&&(d.pointIndex+=Math.ceil(b[0]));var f=d.pos[0]+(e.offsetLeft||0),g=d.pos[1]+(e.offsetTop||0),h=n.tickFormat()(j.x()(d.point,d.pointIndex)),i=(d.series.bar?p:q).tickFormat()(j.y()(d.point,d.pointIndex)),k=G(d.series.key,h,i,d,a);c.tooltip.show([f,g],k,d.value<0?"n":"s",null,e)};return j.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+v.left,a.pos[1]+v.top],I.tooltipShow(a)}),j.dispatch.on("elementMouseout.tooltip",function(a){I.tooltipHide(a)}),l.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+v.left,a.pos[1]+v.top],I.tooltipShow(a)}),l.dispatch.on("elementMouseout.tooltip",function(a){I.tooltipHide(a)}),I.on("tooltipHide",function(){F&&c.tooltip.cleanup()}),a.dispatch=I,a.legend=t,a.lines=j,a.lines2=k,a.bars=l,a.bars2=m,a.xAxis=n,a.x2Axis=o,a.y1Axis=p,a.y2Axis=q,a.y3Axis=r,a.y4Axis=s,d3.rebind(a,j,"defined","size","clipVoronoi","interpolate"),a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(A=b,j.x(b),l.x(b),a):A},a.y=function(b){return arguments.length?(B=b,j.y(b),l.y(b),a):B},a.margin=function(b){return arguments.length?(v.top="undefined"!=typeof b.top?b.top:v.top,v.right="undefined"!=typeof b.right?b.right:v.right,v.bottom="undefined"!=typeof b.bottom?b.bottom:v.bottom,v.left="undefined"!=typeof b.left?b.left:v.left,a):v},a.width=function(b){return arguments.length?(x=b,a):x},a.height=function(b){return arguments.length?(y=b,a):y},a.color=function(b){return arguments.length?(C=c.utils.getColor(b),t.color(C),a):C},a.showLegend=function(b){return arguments.length?(D=b,a):D},a.tooltips=function(b){return arguments.length?(F=b,a):F},a.tooltipContent=function(b){return arguments.length?(G=b,a):G},a.noData=function(b){return arguments.length?(H=b,a):H},a.brushExtent=function(b){return arguments.length?(E=b,a):E},a},c.models.multiBar=function(){"use strict";function a(c){return c.each(function(a){var c=k-j.left-j.right,B=l-j.top-j.bottom,C=d3.select(this);w&&a.length&&(w=[{values:a[0].values.map(function(a){return{x:a.x,y:0,series:a.series,size:.01}})}]),t&&(a=d3.layout.stack().offset(u).values(function(a){return a.values}).y(q)(!a.length&&w?w:a)),a.forEach(function(a,b){a.values.forEach(function(a){a.series=b})}),t&&a[0].values.map(function(b,c){var d=0,e=0;a.map(function(a){var b=a.values[c];b.size=Math.abs(b.y),b.y<0?(b.y1=e,e-=b.size):(b.y1=b.size+d,d+=b.size)})});var D=d&&e?[]:a.map(function(a){return a.values.map(function(a,b){return{x:p(a,b),y:q(a,b),y0:a.y0,y1:a.y1}})});m.domain(d||d3.merge(D).map(function(a){return a.x})).rangeBands(f||[0,c],z),n.domain(e||d3.extent(d3.merge(D).map(function(a){return t?a.y>0?a.y1:a.y1+a.y:a.y}).concat(r))).range(g||[B,0]),m.domain()[0]===m.domain()[1]&&(m.domain()[0]?m.domain([m.domain()[0]-.01*m.domain()[0],m.domain()[1]+.01*m.domain()[1]]):m.domain([-1,1])),n.domain()[0]===n.domain()[1]&&(n.domain()[0]?n.domain([n.domain()[0]+.01*n.domain()[0],n.domain()[1]-.01*n.domain()[1]]):n.domain([-1,1])),h=h||m,i=i||n;var E=C.selectAll("g.nv-wrap.nv-multibar").data([a]),F=E.enter().append("g").attr("class","nvd3 nv-wrap nv-multibar"),G=F.append("defs"),H=F.append("g"),I=E.select("g");H.append("g").attr("class","nv-groups"),E.attr("transform","translate("+j.left+","+j.top+")"),G.append("clipPath").attr("id","nv-edge-clip-"+o).append("rect"),E.select("#nv-edge-clip-"+o+" rect").attr("width",c).attr("height",B),I.attr("clip-path",s?"url(#nv-edge-clip-"+o+")":"");var J=E.select(".nv-groups").selectAll(".nv-group").data(function(a){return a},function(a,b){return b});J.enter().append("g").style("stroke-opacity",1e-6).style("fill-opacity",1e-6),J.exit().transition().selectAll("rect.nv-bar").delay(function(b,c){return c*y/a[0].values.length}).attr("y",function(a){return t?i(a.y0):i(0)}).attr("height",0).remove(),J.attr("class",function(a,b){return"nv-group nv-series-"+b
}).classed("hover",function(a){return a.hover}).style("fill",function(a,b){return v(a,b)}).style("stroke",function(a,b){return v(a,b)}),J.transition().style("stroke-opacity",1).style("fill-opacity",.75);var K=J.selectAll("rect.nv-bar").data(function(b){return w&&!a.length?w.values:b.values});K.exit().remove(),K.enter().append("rect").attr("class",function(a,b){return q(a,b)<0?"nv-bar negative":"nv-bar positive"}).attr("x",function(b,c,d){return t?0:d*m.rangeBand()/a.length}).attr("y",function(a){return i(t?a.y0:0)}).attr("height",0).attr("width",m.rangeBand()/(t?1:a.length)).attr("transform",function(a,b){return"translate("+m(p(a,b))+",0)"}),K.style("fill",function(a,b,c){return v(a,c,b)}).style("stroke",function(a,b,c){return v(a,c,b)}).on("mouseover",function(b,c){d3.select(this).classed("hover",!0),A.elementMouseover({value:q(b,c),point:b,series:a[b.series],pos:[m(p(b,c))+m.rangeBand()*(t?a.length/2:b.series+.5)/a.length,n(q(b,c)+(t?b.y0:0))],pointIndex:c,seriesIndex:b.series,e:d3.event})}).on("mouseout",function(b,c){d3.select(this).classed("hover",!1),A.elementMouseout({value:q(b,c),point:b,series:a[b.series],pointIndex:c,seriesIndex:b.series,e:d3.event})}).on("click",function(b,c){A.elementClick({value:q(b,c),point:b,series:a[b.series],pos:[m(p(b,c))+m.rangeBand()*(t?a.length/2:b.series+.5)/a.length,n(q(b,c)+(t?b.y0:0))],pointIndex:c,seriesIndex:b.series,e:d3.event}),d3.event.stopPropagation()}).on("dblclick",function(b,c){A.elementDblClick({value:q(b,c),point:b,series:a[b.series],pos:[m(p(b,c))+m.rangeBand()*(t?a.length/2:b.series+.5)/a.length,n(q(b,c)+(t?b.y0:0))],pointIndex:c,seriesIndex:b.series,e:d3.event}),d3.event.stopPropagation()}),K.attr("class",function(a,b){return q(a,b)<0?"nv-bar negative":"nv-bar positive"}).transition().attr("transform",function(a,b){return"translate("+m(p(a,b))+",0)"}),x&&(b||(b=a.map(function(){return!0})),K.style("fill",function(a,c,d){return d3.rgb(x(a,c)).darker(b.map(function(a,b){return b}).filter(function(a,c){return!b[c]})[d]).toString()}).style("stroke",function(a,c,d){return d3.rgb(x(a,c)).darker(b.map(function(a,b){return b}).filter(function(a,c){return!b[c]})[d]).toString()})),t?K.transition().delay(function(b,c){return c*y/a[0].values.length}).attr("y",function(a){return n(t?a.y1:0)}).attr("height",function(a){return Math.max(Math.abs(n(a.y+(t?a.y0:0))-n(t?a.y0:0)),1)}).attr("x",function(b){return t?0:b.series*m.rangeBand()/a.length}).attr("width",m.rangeBand()/(t?1:a.length)):K.transition().delay(function(b,c){return c*y/a[0].values.length}).attr("x",function(b){return b.series*m.rangeBand()/a.length}).attr("width",m.rangeBand()/a.length).attr("y",function(a,b){return q(a,b)<0?n(0):n(0)-n(q(a,b))<1?n(0)-1:n(q(a,b))||0}).attr("height",function(a,b){return Math.max(Math.abs(n(q(a,b))-n(0)),1)||0}),h=m.copy(),i=n.copy()}),a}var b,d,e,f,g,h,i,j={top:0,right:0,bottom:0,left:0},k=960,l=500,m=d3.scale.ordinal(),n=d3.scale.linear(),o=Math.floor(1e4*Math.random()),p=function(a){return a.x},q=function(a){return a.y},r=[0],s=!0,t=!1,u="zero",v=c.utils.defaultColor(),w=!1,x=null,y=1200,z=.1,A=d3.dispatch("chartClick","elementClick","elementDblClick","elementMouseover","elementMouseout");return a.dispatch=A,a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(p=b,a):p},a.y=function(b){return arguments.length?(q=b,a):q},a.margin=function(b){return arguments.length?(j.top="undefined"!=typeof b.top?b.top:j.top,j.right="undefined"!=typeof b.right?b.right:j.right,j.bottom="undefined"!=typeof b.bottom?b.bottom:j.bottom,j.left="undefined"!=typeof b.left?b.left:j.left,a):j},a.width=function(b){return arguments.length?(k=b,a):k},a.height=function(b){return arguments.length?(l=b,a):l},a.xScale=function(b){return arguments.length?(m=b,a):m},a.yScale=function(b){return arguments.length?(n=b,a):n},a.xDomain=function(b){return arguments.length?(d=b,a):d},a.yDomain=function(b){return arguments.length?(e=b,a):e},a.xRange=function(b){return arguments.length?(f=b,a):f},a.yRange=function(b){return arguments.length?(g=b,a):g},a.forceY=function(b){return arguments.length?(r=b,a):r},a.stacked=function(b){return arguments.length?(t=b,a):t},a.stackOffset=function(b){return arguments.length?(u=b,a):u},a.clipEdge=function(b){return arguments.length?(s=b,a):s},a.color=function(b){return arguments.length?(v=c.utils.getColor(b),a):v},a.barColor=function(b){return arguments.length?(x=c.utils.getColor(b),a):x},a.disabled=function(c){return arguments.length?(b=c,a):b},a.id=function(b){return arguments.length?(o=b,a):o},a.hideable=function(b){return arguments.length?(w=b,a):w},a.delay=function(b){return arguments.length?(y=b,a):y},a.groupSpacing=function(b){return arguments.length?(z=b,a):z},a},c.models.multiBarChart=function(){"use strict";function a(c){return c.each(function(c){var w=d3.select(this),E=this,F=(k||parseInt(w.style("width"))||960)-j.left-j.right,G=(l||parseInt(w.style("height"))||400)-j.top-j.bottom;if(a.update=function(){w.transition().duration(C).call(a)},a.container=this,x.disabled=c.map(function(a){return!!a.disabled}),!y){var H;y={};for(H in x)y[H]=x[H]instanceof Array?x[H].slice(0):x[H]}if(!(c&&c.length&&c.filter(function(a){return a.values.length}).length)){var I=w.selectAll(".nv-noData").data([z]);return I.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),I.attr("x",j.left+F/2).attr("y",j.top+G/2).text(function(a){return a}),a}w.selectAll(".nv-noData").remove(),b=e.xScale(),d=e.yScale();var J=w.selectAll("g.nv-wrap.nv-multiBarWithLegend").data([c]),K=J.enter().append("g").attr("class","nvd3 nv-wrap nv-multiBarWithLegend").append("g"),L=J.select("g");if(K.append("g").attr("class","nv-x nv-axis"),K.append("g").attr("class","nv-y nv-axis"),K.append("g").attr("class","nv-barsWrap"),K.append("g").attr("class","nv-legendWrap"),K.append("g").attr("class","nv-controlsWrap"),o&&(h.width(F-B()),e.barColor()&&c.forEach(function(a,b){a.color=d3.rgb("#ccc").darker(1.5*b).toString()}),L.select(".nv-legendWrap").datum(c).call(h),j.top!=h.height()&&(j.top=h.height(),G=(l||parseInt(w.style("height"))||400)-j.top-j.bottom),L.select(".nv-legendWrap").attr("transform","translate("+B()+","+-j.top+")")),n){var M=[{key:"Grouped",disabled:e.stacked()},{key:"Stacked",disabled:!e.stacked()}];i.width(B()).color(["#444","#444","#444"]),L.select(".nv-controlsWrap").datum(M).attr("transform","translate(0,"+-j.top+")").call(i)}J.attr("transform","translate("+j.left+","+j.top+")"),r&&L.select(".nv-y.nv-axis").attr("transform","translate("+F+",0)"),e.disabled(c.map(function(a){return a.disabled})).width(F).height(G).color(c.map(function(a,b){return a.color||m(a,b)}).filter(function(a,b){return!c[b].disabled}));var N=L.select(".nv-barsWrap").datum(c.filter(function(a){return!a.disabled}));if(N.transition().call(e),p){f.scale(b).ticks(F/100).tickSize(-G,0),L.select(".nv-x.nv-axis").attr("transform","translate(0,"+d.range()[0]+")"),L.select(".nv-x.nv-axis").transition().call(f);var O=L.select(".nv-x.nv-axis > g").selectAll("g");if(O.selectAll("line, text").style("opacity",1),t){var P=function(a,b){return"translate("+a+","+b+")"},Q=5,R=17;O.selectAll("text").attr("transform",function(a,b,c){return P(0,0==c%2?Q:R)});var S=d3.selectAll(".nv-x.nv-axis .nv-wrap g g text")[0].length;L.selectAll(".nv-x.nv-axis .nv-axisMaxMin text").attr("transform",function(a,b){return P(0,0===b||0!==S%2?R:Q)})}s&&O.filter(function(a,b){return 0!==b%Math.ceil(c[0].values.length/(F/100))}).selectAll("text, line").style("opacity",0),u&&O.selectAll(".tick text").attr("transform","rotate("+u+" 0,0)").style("text-anchor",u>0?"start":"end"),L.select(".nv-x.nv-axis").selectAll("g.nv-axisMaxMin text").style("opacity",1)}q&&(g.scale(d).ticks(G/36).tickSize(-F,0),L.select(".nv-y.nv-axis").transition().call(g)),h.dispatch.on("stateChange",function(b){x=b,A.stateChange(x),a.update()}),i.dispatch.on("legendClick",function(b){if(b.disabled){switch(M=M.map(function(a){return a.disabled=!0,a}),b.disabled=!1,b.key){case"Grouped":e.stacked(!1);break;case"Stacked":e.stacked(!0)}x.stacked=e.stacked(),A.stateChange(x),a.update()}}),A.on("tooltipShow",function(a){v&&D(a,E.parentNode)}),A.on("changeState",function(b){"undefined"!=typeof b.disabled&&(c.forEach(function(a,c){a.disabled=b.disabled[c]}),x.disabled=b.disabled),"undefined"!=typeof b.stacked&&(e.stacked(b.stacked),x.stacked=b.stacked),a.update()})}),a}var b,d,e=c.models.multiBar(),f=c.models.axis(),g=c.models.axis(),h=c.models.legend(),i=c.models.legend(),j={top:30,right:20,bottom:50,left:60},k=null,l=null,m=c.utils.defaultColor(),n=!0,o=!0,p=!0,q=!0,r=!1,s=!0,t=!1,u=0,v=!0,w=function(a,b,c){return"<h3>"+a+"</h3>"+"<p>"+c+" on "+b+"</p>"},x={stacked:!1},y=null,z="No Data Available.",A=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState"),B=function(){return n?180:0},C=250;e.stacked(!1),f.orient("bottom").tickPadding(7).highlightZero(!0).showMaxMin(!1).tickFormat(function(a){return a}),g.orient(r?"right":"left").tickFormat(d3.format(",.1f")),i.updateState(!1);var D=function(b,d){var h=b.pos[0]+(d.offsetLeft||0),i=b.pos[1]+(d.offsetTop||0),j=f.tickFormat()(e.x()(b.point,b.pointIndex)),k=g.tickFormat()(e.y()(b.point,b.pointIndex)),l=w(b.series.key,j,k,b,a);c.tooltip.show([h,i],l,b.value<0?"n":"s",null,d)};return e.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+j.left,a.pos[1]+j.top],A.tooltipShow(a)}),e.dispatch.on("elementMouseout.tooltip",function(a){A.tooltipHide(a)}),A.on("tooltipHide",function(){v&&c.tooltip.cleanup()}),a.dispatch=A,a.multibar=e,a.legend=h,a.xAxis=f,a.yAxis=g,d3.rebind(a,e,"x","y","xDomain","yDomain","xRange","yRange","forceX","forceY","clipEdge","id","stacked","stackOffset","delay","barColor","groupSpacing"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(j.top="undefined"!=typeof b.top?b.top:j.top,j.right="undefined"!=typeof b.right?b.right:j.right,j.bottom="undefined"!=typeof b.bottom?b.bottom:j.bottom,j.left="undefined"!=typeof b.left?b.left:j.left,a):j},a.width=function(b){return arguments.length?(k=b,a):k},a.height=function(b){return arguments.length?(l=b,a):l},a.color=function(b){return arguments.length?(m=c.utils.getColor(b),h.color(m),a):m},a.showControls=function(b){return arguments.length?(n=b,a):n},a.showLegend=function(b){return arguments.length?(o=b,a):o},a.showXAxis=function(b){return arguments.length?(p=b,a):p},a.showYAxis=function(b){return arguments.length?(q=b,a):q},a.rightAlignYAxis=function(b){return arguments.length?(r=b,g.orient(b?"right":"left"),a):r},a.reduceXTicks=function(b){return arguments.length?(s=b,a):s},a.rotateLabels=function(b){return arguments.length?(u=b,a):u},a.staggerLabels=function(b){return arguments.length?(t=b,a):t},a.tooltip=function(b){return arguments.length?(w=b,a):w},a.tooltips=function(b){return arguments.length?(v=b,a):v},a.tooltipContent=function(b){return arguments.length?(w=b,a):w},a.state=function(b){return arguments.length?(x=b,a):x},a.defaultState=function(b){return arguments.length?(y=b,a):y},a.noData=function(b){return arguments.length?(z=b,a):z},a.transitionDuration=function(b){return arguments.length?(C=b,a):C},a},c.models.multiBarHorizontal=function(){"use strict";function a(c){return c.each(function(a){var c=k-j.left-j.right,m=l-j.top-j.bottom;d3.select(this),u&&(a=d3.layout.stack().offset("zero").values(function(a){return a.values}).y(q)(a)),a.forEach(function(a,b){a.values.forEach(function(a){a.series=b})}),u&&a[0].values.map(function(b,c){var d=0,e=0;a.map(function(a){var b=a.values[c];b.size=Math.abs(b.y),b.y<0?(b.y1=e-b.size,e-=b.size):(b.y1=d,d+=b.size)})});var z=d&&e?[]:a.map(function(a){return a.values.map(function(a,b){return{x:p(a,b),y:q(a,b),y0:a.y0,y1:a.y1}})});n.domain(d||d3.merge(z).map(function(a){return a.x})).rangeBands(f||[0,m],.1),o.domain(e||d3.extent(d3.merge(z).map(function(a){return u?a.y>0?a.y1+a.y:a.y1:a.y}).concat(r))),v&&!u?o.range(g||[o.domain()[0]<0?x:0,c-(o.domain()[1]>0?x:0)]):o.range(g||[0,c]),h=h||n,i=i||d3.scale.linear().domain(o.domain()).range([o(0),o(0)]);var B=d3.select(this).selectAll("g.nv-wrap.nv-multibarHorizontal").data([a]),C=B.enter().append("g").attr("class","nvd3 nv-wrap nv-multibarHorizontal");C.append("defs");var D=C.append("g");B.select("g"),D.append("g").attr("class","nv-groups"),B.attr("transform","translate("+j.left+","+j.top+")");var E=B.select(".nv-groups").selectAll(".nv-group").data(function(a){return a},function(a,b){return b});E.enter().append("g").style("stroke-opacity",1e-6).style("fill-opacity",1e-6),E.exit().transition().style("stroke-opacity",1e-6).style("fill-opacity",1e-6).remove(),E.attr("class",function(a,b){return"nv-group nv-series-"+b}).classed("hover",function(a){return a.hover}).style("fill",function(a,b){return s(a,b)}).style("stroke",function(a,b){return s(a,b)}),E.transition().style("stroke-opacity",1).style("fill-opacity",.75);var F=E.selectAll("g.nv-bar").data(function(a){return a.values});F.exit().remove();var G=F.enter().append("g").attr("transform",function(b,c,d){return"translate("+i(u?b.y0:0)+","+(u?0:d*n.rangeBand()/a.length+n(p(b,c)))+")"});G.append("rect").attr("width",0).attr("height",n.rangeBand()/(u?1:a.length)),F.on("mouseover",function(b,c){d3.select(this).classed("hover",!0),A.elementMouseover({value:q(b,c),point:b,series:a[b.series],pos:[o(q(b,c)+(u?b.y0:0)),n(p(b,c))+n.rangeBand()*(u?a.length/2:b.series+.5)/a.length],pointIndex:c,seriesIndex:b.series,e:d3.event})}).on("mouseout",function(b,c){d3.select(this).classed("hover",!1),A.elementMouseout({value:q(b,c),point:b,series:a[b.series],pointIndex:c,seriesIndex:b.series,e:d3.event})}).on("click",function(b,c){A.elementClick({value:q(b,c),point:b,series:a[b.series],pos:[n(p(b,c))+n.rangeBand()*(u?a.length/2:b.series+.5)/a.length,o(q(b,c)+(u?b.y0:0))],pointIndex:c,seriesIndex:b.series,e:d3.event}),d3.event.stopPropagation()}).on("dblclick",function(b,c){A.elementDblClick({value:q(b,c),point:b,series:a[b.series],pos:[n(p(b,c))+n.rangeBand()*(u?a.length/2:b.series+.5)/a.length,o(q(b,c)+(u?b.y0:0))],pointIndex:c,seriesIndex:b.series,e:d3.event}),d3.event.stopPropagation()}),G.append("text"),v&&!u?(F.select("text").attr("text-anchor",function(a,b){return q(a,b)<0?"end":"start"}).attr("y",n.rangeBand()/(2*a.length)).attr("dy",".32em").text(function(a,b){return y(q(a,b))}),F.transition().select("text").attr("x",function(a,b){return q(a,b)<0?-4:o(q(a,b))-o(0)+4})):F.selectAll("text").text(""),w&&!u?(G.append("text").classed("nv-bar-label",!0),F.select("text.nv-bar-label").attr("text-anchor",function(a,b){return q(a,b)<0?"start":"end"}).attr("y",n.rangeBand()/(2*a.length)).attr("dy",".32em").text(function(a,b){return p(a,b)}),F.transition().select("text.nv-bar-label").attr("x",function(a,b){return q(a,b)<0?o(0)-o(q(a,b))+4:-4})):F.selectAll("text.nv-bar-label").text(""),F.attr("class",function(a,b){return q(a,b)<0?"nv-bar negative":"nv-bar positive"}),t&&(b||(b=a.map(function(){return!0})),F.style("fill",function(a,c,d){return d3.rgb(t(a,c)).darker(b.map(function(a,b){return b}).filter(function(a,c){return!b[c]})[d]).toString()}).style("stroke",function(a,c,d){return d3.rgb(t(a,c)).darker(b.map(function(a,b){return b}).filter(function(a,c){return!b[c]})[d]).toString()})),u?F.transition().attr("transform",function(a,b){return"translate("+o(a.y1)+","+n(p(a,b))+")"}).select("rect").attr("width",function(a,b){return Math.abs(o(q(a,b)+a.y0)-o(a.y0))}).attr("height",n.rangeBand()):F.transition().attr("transform",function(b,c){return"translate("+(q(b,c)<0?o(q(b,c)):o(0))+","+(b.series*n.rangeBand()/a.length+n(p(b,c)))+")"}).select("rect").attr("height",n.rangeBand()/a.length).attr("width",function(a,b){return Math.max(Math.abs(o(q(a,b))-o(0)),1)}),h=n.copy(),i=o.copy()}),a}var b,d,e,f,g,h,i,j={top:0,right:0,bottom:0,left:0},k=960,l=500,m=Math.floor(1e4*Math.random()),n=d3.scale.ordinal(),o=d3.scale.linear(),p=function(a){return a.x},q=function(a){return a.y},r=[0],s=c.utils.defaultColor(),t=null,u=!1,v=!1,w=!1,x=60,y=d3.format(",.2f"),z=1200,A=d3.dispatch("chartClick","elementClick","elementDblClick","elementMouseover","elementMouseout");return a.dispatch=A,a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(p=b,a):p},a.y=function(b){return arguments.length?(q=b,a):q},a.margin=function(b){return arguments.length?(j.top="undefined"!=typeof b.top?b.top:j.top,j.right="undefined"!=typeof b.right?b.right:j.right,j.bottom="undefined"!=typeof b.bottom?b.bottom:j.bottom,j.left="undefined"!=typeof b.left?b.left:j.left,a):j},a.width=function(b){return arguments.length?(k=b,a):k},a.height=function(b){return arguments.length?(l=b,a):l},a.xScale=function(b){return arguments.length?(n=b,a):n},a.yScale=function(b){return arguments.length?(o=b,a):o},a.xDomain=function(b){return arguments.length?(d=b,a):d},a.yDomain=function(b){return arguments.length?(e=b,a):e},a.xRange=function(b){return arguments.length?(f=b,a):f},a.yRange=function(b){return arguments.length?(g=b,a):g},a.forceY=function(b){return arguments.length?(r=b,a):r},a.stacked=function(b){return arguments.length?(u=b,a):u},a.color=function(b){return arguments.length?(s=c.utils.getColor(b),a):s},a.barColor=function(b){return arguments.length?(t=c.utils.getColor(b),a):t},a.disabled=function(c){return arguments.length?(b=c,a):b},a.id=function(b){return arguments.length?(m=b,a):m},a.delay=function(b){return arguments.length?(z=b,a):z},a.showValues=function(b){return arguments.length?(v=b,a):v},a.showBarLabels=function(b){return arguments.length?(w=b,a):w},a.valueFormat=function(b){return arguments.length?(y=b,a):y},a.valuePadding=function(b){return arguments.length?(x=b,a):x},a},c.models.multiBarHorizontalChart=function(){"use strict";function a(c){return c.each(function(c){var r=d3.select(this),t=this,B=(k||parseInt(r.style("width"))||960)-j.left-j.right,C=(l||parseInt(r.style("height"))||400)-j.top-j.bottom;if(a.update=function(){r.transition().duration(z).call(a)},a.container=this,u.disabled=c.map(function(a){return!!a.disabled}),!v){var D;v={};for(D in u)v[D]=u[D]instanceof Array?u[D].slice(0):u[D]}if(!(c&&c.length&&c.filter(function(a){return a.values.length}).length)){var E=r.selectAll(".nv-noData").data([w]);return E.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),E.attr("x",j.left+B/2).attr("y",j.top+C/2).text(function(a){return a}),a}r.selectAll(".nv-noData").remove(),b=e.xScale(),d=e.yScale();var F=r.selectAll("g.nv-wrap.nv-multiBarHorizontalChart").data([c]),G=F.enter().append("g").attr("class","nvd3 nv-wrap nv-multiBarHorizontalChart").append("g"),H=F.select("g");if(G.append("g").attr("class","nv-x nv-axis"),G.append("g").attr("class","nv-y nv-axis").append("g").attr("class","nv-zeroLine").append("line"),G.append("g").attr("class","nv-barsWrap"),G.append("g").attr("class","nv-legendWrap"),G.append("g").attr("class","nv-controlsWrap"),o&&(h.width(B-y()),e.barColor()&&c.forEach(function(a,b){a.color=d3.rgb("#ccc").darker(1.5*b).toString()}),H.select(".nv-legendWrap").datum(c).call(h),j.top!=h.height()&&(j.top=h.height(),C=(l||parseInt(r.style("height"))||400)-j.top-j.bottom),H.select(".nv-legendWrap").attr("transform","translate("+y()+","+-j.top+")")),n){var I=[{key:"Grouped",disabled:e.stacked()},{key:"Stacked",disabled:!e.stacked()}];i.width(y()).color(["#444","#444","#444"]),H.select(".nv-controlsWrap").datum(I).attr("transform","translate(0,"+-j.top+")").call(i)}F.attr("transform","translate("+j.left+","+j.top+")"),e.disabled(c.map(function(a){return a.disabled})).width(B).height(C).color(c.map(function(a,b){return a.color||m(a,b)}).filter(function(a,b){return!c[b].disabled}));var J=H.select(".nv-barsWrap").datum(c.filter(function(a){return!a.disabled}));if(J.transition().call(e),p){f.scale(b).ticks(C/24).tickSize(-B,0),H.select(".nv-x.nv-axis").transition().call(f);var K=H.select(".nv-x.nv-axis").selectAll("g");K.selectAll("line, text")}q&&(g.scale(d).ticks(B/100).tickSize(-C,0),H.select(".nv-y.nv-axis").attr("transform","translate(0,"+C+")"),H.select(".nv-y.nv-axis").transition().call(g)),H.select(".nv-zeroLine line").attr("x1",d(0)).attr("x2",d(0)).attr("y1",0).attr("y2",-C),h.dispatch.on("stateChange",function(b){u=b,x.stateChange(u),a.update()}),i.dispatch.on("legendClick",function(b){if(b.disabled){switch(I=I.map(function(a){return a.disabled=!0,a}),b.disabled=!1,b.key){case"Grouped":e.stacked(!1);break;case"Stacked":e.stacked(!0)}u.stacked=e.stacked(),x.stateChange(u),a.update()}}),x.on("tooltipShow",function(a){s&&A(a,t.parentNode)}),x.on("changeState",function(b){"undefined"!=typeof b.disabled&&(c.forEach(function(a,c){a.disabled=b.disabled[c]}),u.disabled=b.disabled),"undefined"!=typeof b.stacked&&(e.stacked(b.stacked),u.stacked=b.stacked),a.update()})}),a}var b,d,e=c.models.multiBarHorizontal(),f=c.models.axis(),g=c.models.axis(),h=c.models.legend().height(30),i=c.models.legend().height(30),j={top:30,right:20,bottom:50,left:60},k=null,l=null,m=c.utils.defaultColor(),n=!0,o=!0,p=!0,q=!0,r=!1,s=!0,t=function(a,b,c){return"<h3>"+a+" - "+b+"</h3>"+"<p>"+c+"</p>"},u={stacked:r},v=null,w="No Data Available.",x=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState"),y=function(){return n?180:0},z=250;e.stacked(r),f.orient("left").tickPadding(5).highlightZero(!1).showMaxMin(!1).tickFormat(function(a){return a}),g.orient("bottom").tickFormat(d3.format(",.1f")),i.updateState(!1);var A=function(b,d){var h=b.pos[0]+(d.offsetLeft||0),i=b.pos[1]+(d.offsetTop||0),j=f.tickFormat()(e.x()(b.point,b.pointIndex)),k=g.tickFormat()(e.y()(b.point,b.pointIndex)),l=t(b.series.key,j,k,b,a);c.tooltip.show([h,i],l,b.value<0?"e":"w",null,d)};return e.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+j.left,a.pos[1]+j.top],x.tooltipShow(a)}),e.dispatch.on("elementMouseout.tooltip",function(a){x.tooltipHide(a)}),x.on("tooltipHide",function(){s&&c.tooltip.cleanup()}),a.dispatch=x,a.multibar=e,a.legend=h,a.xAxis=f,a.yAxis=g,d3.rebind(a,e,"x","y","xDomain","yDomain","xRange","yRange","forceX","forceY","clipEdge","id","delay","showValues","showBarLabels","valueFormat","stacked","barColor"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(j.top="undefined"!=typeof b.top?b.top:j.top,j.right="undefined"!=typeof b.right?b.right:j.right,j.bottom="undefined"!=typeof b.bottom?b.bottom:j.bottom,j.left="undefined"!=typeof b.left?b.left:j.left,a):j},a.width=function(b){return arguments.length?(k=b,a):k},a.height=function(b){return arguments.length?(l=b,a):l},a.color=function(b){return arguments.length?(m=c.utils.getColor(b),h.color(m),a):m},a.showControls=function(b){return arguments.length?(n=b,a):n},a.showLegend=function(b){return arguments.length?(o=b,a):o},a.showXAxis=function(b){return arguments.length?(p=b,a):p},a.showYAxis=function(b){return arguments.length?(q=b,a):q},a.tooltip=function(b){return arguments.length?(t=b,a):t},a.tooltips=function(b){return arguments.length?(s=b,a):s},a.tooltipContent=function(b){return arguments.length?(t=b,a):t},a.state=function(b){return arguments.length?(u=b,a):u},a.defaultState=function(b){return arguments.length?(v=b,a):v},a.noData=function(b){return arguments.length?(w=b,a):w},a.transitionDuration=function(b){return arguments.length?(z=b,a):z},a},c.models.multiChart=function(){"use strict";function a(c){return c.each(function(c){var l=d3.select(this),A=this;a.update=function(){l.transition().call(a)},a.container=this;var B=(h||parseInt(l.style("width"))||960)-f.left-f.right,C=(i||parseInt(l.style("height"))||400)-f.top-f.bottom,D=c.filter(function(a){return!a.disabled&&"line"==a.type&&1==a.yAxis}),E=c.filter(function(a){return!a.disabled&&"line"==a.type&&2==a.yAxis}),F=c.filter(function(a){return!a.disabled&&"bar"==a.type&&1==a.yAxis}),G=c.filter(function(a){return!a.disabled&&"bar"==a.type&&2==a.yAxis}),H=c.filter(function(a){return!a.disabled&&"area"==a.type&&1==a.yAxis}),I=c.filter(function(a){return!a.disabled&&"area"==a.type&&2==a.yAxis}),J=c.filter(function(a){return!a.disabled&&1==a.yAxis}).map(function(a){return a.values.map(function(a){return{x:a.x,y:a.y}})}),K=c.filter(function(a){return!a.disabled&&2==a.yAxis}).map(function(a){return a.values.map(function(a){return{x:a.x,y:a.y}})});b.domain(d3.extent(d3.merge(J.concat(K)),function(a){return a.x})).range([0,B]);var L=l.selectAll("g.wrap.multiChart").data([c]),M=L.enter().append("g").attr("class","wrap nvd3 multiChart").append("g");M.append("g").attr("class","x axis"),M.append("g").attr("class","y1 axis"),M.append("g").attr("class","y2 axis"),M.append("g").attr("class","lines1Wrap"),M.append("g").attr("class","lines2Wrap"),M.append("g").attr("class","bars1Wrap"),M.append("g").attr("class","bars2Wrap"),M.append("g").attr("class","stack1Wrap"),M.append("g").attr("class","stack2Wrap"),M.append("g").attr("class","legendWrap");var N=L.select("g");j&&(x.width(B/2),N.select(".legendWrap").datum(c.map(function(a){return a.originalKey=void 0===a.originalKey?a.key:a.originalKey,a.key=a.originalKey+(1==a.yAxis?"":" (right axis)"),a})).call(x),f.top!=x.height()&&(f.top=x.height(),C=(i||parseInt(l.style("height"))||400)-f.top-f.bottom),N.select(".legendWrap").attr("transform","translate("+B/2+","+-f.top+")")),o.width(B).height(C).interpolate("monotone").color(c.map(function(a,b){return a.color||g[b%g.length]}).filter(function(a,b){return!c[b].disabled&&1==c[b].yAxis&&"line"==c[b].type})),p.width(B).height(C).interpolate("monotone").color(c.map(function(a,b){return a.color||g[b%g.length]}).filter(function(a,b){return!c[b].disabled&&2==c[b].yAxis&&"line"==c[b].type})),q.width(B).height(C).color(c.map(function(a,b){return a.color||g[b%g.length]}).filter(function(a,b){return!c[b].disabled&&1==c[b].yAxis&&"bar"==c[b].type})),r.width(B).height(C).color(c.map(function(a,b){return a.color||g[b%g.length]}).filter(function(a,b){return!c[b].disabled&&2==c[b].yAxis&&"bar"==c[b].type})),s.width(B).height(C).color(c.map(function(a,b){return a.color||g[b%g.length]}).filter(function(a,b){return!c[b].disabled&&1==c[b].yAxis&&"area"==c[b].type})),t.width(B).height(C).color(c.map(function(a,b){return a.color||g[b%g.length]}).filter(function(a,b){return!c[b].disabled&&2==c[b].yAxis&&"area"==c[b].type})),N.attr("transform","translate("+f.left+","+f.top+")");var O=N.select(".lines1Wrap").datum(D),P=N.select(".bars1Wrap").datum(F),Q=N.select(".stack1Wrap").datum(H),R=N.select(".lines2Wrap").datum(E),S=N.select(".bars2Wrap").datum(G),T=N.select(".stack2Wrap").datum(I),U=H.length?H.map(function(a){return a.values}).reduce(function(a,b){return a.map(function(a,c){return{x:a.x,y:a.y+b[c].y}})}).concat([{x:0,y:0}]):[],V=I.length?I.map(function(a){return a.values}).reduce(function(a,b){return a.map(function(a,c){return{x:a.x,y:a.y+b[c].y}})}).concat([{x:0,y:0}]):[];m.domain(d||d3.extent(d3.merge(J).concat(U),function(a){return a.y})).range([0,C]),n.domain(e||d3.extent(d3.merge(K).concat(V),function(a){return a.y})).range([0,C]),o.yDomain(m.domain()),q.yDomain(m.domain()),s.yDomain(m.domain()),p.yDomain(n.domain()),r.yDomain(n.domain()),t.yDomain(n.domain()),H.length&&d3.transition(Q).call(s),I.length&&d3.transition(T).call(t),F.length&&d3.transition(P).call(q),G.length&&d3.transition(S).call(r),D.length&&d3.transition(O).call(o),E.length&&d3.transition(R).call(p),u.ticks(B/100).tickSize(-C,0),N.select(".x.axis").attr("transform","translate(0,"+C+")"),d3.transition(N.select(".x.axis")).call(u),v.ticks(C/36).tickSize(-B,0),d3.transition(N.select(".y1.axis")).call(v),w.ticks(C/36).tickSize(-B,0),d3.transition(N.select(".y2.axis")).call(w),N.select(".y2.axis").style("opacity",K.length?1:0).attr("transform","translate("+b.range()[1]+",0)"),x.dispatch.on("stateChange",function(){a.update()}),y.on("tooltipShow",function(a){k&&z(a,A.parentNode)})}),a}var b,d,e,f={top:30,right:20,bottom:50,left:60},g=d3.scale.category20().range(),h=null,i=null,j=!0,k=!0,l=function(a,b,c){return"<h3>"+a+"</h3>"+"<p>"+c+" at "+b+"</p>"},b=d3.scale.linear(),m=d3.scale.linear(),n=d3.scale.linear(),o=c.models.line().yScale(m),p=c.models.line().yScale(n),q=c.models.multiBar().stacked(!1).yScale(m),r=c.models.multiBar().stacked(!1).yScale(n),s=c.models.stackedArea().yScale(m),t=c.models.stackedArea().yScale(n),u=c.models.axis().scale(b).orient("bottom").tickPadding(5),v=c.models.axis().scale(m).orient("left"),w=c.models.axis().scale(n).orient("right"),x=c.models.legend().height(30),y=d3.dispatch("tooltipShow","tooltipHide"),z=function(b,d){var e=b.pos[0]+(d.offsetLeft||0),f=b.pos[1]+(d.offsetTop||0),g=u.tickFormat()(o.x()(b.point,b.pointIndex)),h=(2==b.series.yAxis?w:v).tickFormat()(o.y()(b.point,b.pointIndex)),i=l(b.series.key,g,h,b,a);c.tooltip.show([e,f],i,void 0,void 0,d.offsetParent)};return o.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+f.left,a.pos[1]+f.top],y.tooltipShow(a)}),o.dispatch.on("elementMouseout.tooltip",function(a){y.tooltipHide(a)}),p.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+f.left,a.pos[1]+f.top],y.tooltipShow(a)}),p.dispatch.on("elementMouseout.tooltip",function(a){y.tooltipHide(a)}),q.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+f.left,a.pos[1]+f.top],y.tooltipShow(a)}),q.dispatch.on("elementMouseout.tooltip",function(a){y.tooltipHide(a)}),r.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+f.left,a.pos[1]+f.top],y.tooltipShow(a)}),r.dispatch.on("elementMouseout.tooltip",function(a){y.tooltipHide(a)}),s.dispatch.on("tooltipShow",function(a){return Math.round(100*s.y()(a.point))?(a.pos=[a.pos[0]+f.left,a.pos[1]+f.top],y.tooltipShow(a),void 0):(setTimeout(function(){d3.selectAll(".point.hover").classed("hover",!1)},0),!1)}),s.dispatch.on("tooltipHide",function(a){y.tooltipHide(a)}),t.dispatch.on("tooltipShow",function(a){return Math.round(100*t.y()(a.point))?(a.pos=[a.pos[0]+f.left,a.pos[1]+f.top],y.tooltipShow(a),void 0):(setTimeout(function(){d3.selectAll(".point.hover").classed("hover",!1)},0),!1)}),t.dispatch.on("tooltipHide",function(a){y.tooltipHide(a)}),o.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+f.left,a.pos[1]+f.top],y.tooltipShow(a)}),o.dispatch.on("elementMouseout.tooltip",function(a){y.tooltipHide(a)}),p.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+f.left,a.pos[1]+f.top],y.tooltipShow(a)}),p.dispatch.on("elementMouseout.tooltip",function(a){y.tooltipHide(a)}),y.on("tooltipHide",function(){k&&c.tooltip.cleanup()}),a.dispatch=y,a.lines1=o,a.lines2=p,a.bars1=q,a.bars2=r,a.stack1=s,a.stack2=t,a.xAxis=u,a.yAxis1=v,a.yAxis2=w,a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(getX=b,o.x(b),q.x(b),a):getX},a.y=function(b){return arguments.length?(getY=b,o.y(b),q.y(b),a):getY},a.yDomain1=function(b){return arguments.length?(d=b,a):d},a.yDomain2=function(b){return arguments.length?(e=b,a):e},a.margin=function(b){return arguments.length?(f=b,a):f},a.width=function(b){return arguments.length?(h=b,a):h},a.height=function(b){return arguments.length?(i=b,a):i},a.color=function(b){return arguments.length?(g=b,x.color(b),a):g},a.showLegend=function(b){return arguments.length?(j=b,a):j},a.tooltips=function(b){return arguments.length?(k=b,a):k},a.tooltipContent=function(b){return arguments.length?(l=b,a):l},a},c.models.ohlcBar=function(){"use strict";function a(c){return c.each(function(a){var c=h-g.left-g.right,w=i-g.top-g.bottom,y=d3.select(this);k.domain(b||d3.extent(a[0].values.map(m).concat(s))),u?k.range(e||[.5*c/a[0].values.length,c*(a[0].values.length-.5)/a[0].values.length]):k.range(e||[0,c]),l.domain(d||[d3.min(a[0].values.map(r).concat(t)),d3.max(a[0].values.map(q).concat(t))]).range(f||[w,0]),k.domain()[0]===k.domain()[1]&&(k.domain()[0]?k.domain([k.domain()[0]-.01*k.domain()[0],k.domain()[1]+.01*k.domain()[1]]):k.domain([-1,1])),l.domain()[0]===l.domain()[1]&&(l.domain()[0]?l.domain([l.domain()[0]+.01*l.domain()[0],l.domain()[1]-.01*l.domain()[1]]):l.domain([-1,1]));var z=d3.select(this).selectAll("g.nv-wrap.nv-ohlcBar").data([a[0].values]),A=z.enter().append("g").attr("class","nvd3 nv-wrap nv-ohlcBar"),B=A.append("defs"),C=A.append("g"),D=z.select("g");
C.append("g").attr("class","nv-ticks"),z.attr("transform","translate("+g.left+","+g.top+")"),y.on("click",function(a,b){x.chartClick({data:a,index:b,pos:d3.event,id:j})}),B.append("clipPath").attr("id","nv-chart-clip-path-"+j).append("rect"),z.select("#nv-chart-clip-path-"+j+" rect").attr("width",c).attr("height",w),D.attr("clip-path",v?"url(#nv-chart-clip-path-"+j+")":"");var E=z.select(".nv-ticks").selectAll(".nv-tick").data(function(a){return a});E.exit().remove(),E.enter().append("path").attr("class",function(a,b,c){return(o(a,b)>p(a,b)?"nv-tick negative":"nv-tick positive")+" nv-tick-"+c+"-"+b}).attr("d",function(b,d){var e=.9*(c/a[0].values.length);return"m0,0l0,"+(l(o(b,d))-l(q(b,d)))+"l"+-e/2+",0l"+e/2+",0l0,"+(l(r(b,d))-l(o(b,d)))+"l0,"+(l(p(b,d))-l(r(b,d)))+"l"+e/2+",0l"+-e/2+",0z"}).attr("transform",function(a,b){return"translate("+k(m(a,b))+","+l(q(a,b))+")"}).on("mouseover",function(b,c){d3.select(this).classed("hover",!0),x.elementMouseover({point:b,series:a[0],pos:[k(m(b,c)),l(n(b,c))],pointIndex:c,seriesIndex:0,e:d3.event})}).on("mouseout",function(b,c){d3.select(this).classed("hover",!1),x.elementMouseout({point:b,series:a[0],pointIndex:c,seriesIndex:0,e:d3.event})}).on("click",function(a,b){x.elementClick({value:n(a,b),data:a,index:b,pos:[k(m(a,b)),l(n(a,b))],e:d3.event,id:j}),d3.event.stopPropagation()}).on("dblclick",function(a,b){x.elementDblClick({value:n(a,b),data:a,index:b,pos:[k(m(a,b)),l(n(a,b))],e:d3.event,id:j}),d3.event.stopPropagation()}),E.attr("class",function(a,b,c){return(o(a,b)>p(a,b)?"nv-tick negative":"nv-tick positive")+" nv-tick-"+c+"-"+b}),d3.transition(E).attr("transform",function(a,b){return"translate("+k(m(a,b))+","+l(q(a,b))+")"}).attr("d",function(b,d){var e=.9*(c/a[0].values.length);return"m0,0l0,"+(l(o(b,d))-l(q(b,d)))+"l"+-e/2+",0l"+e/2+",0l0,"+(l(r(b,d))-l(o(b,d)))+"l0,"+(l(p(b,d))-l(r(b,d)))+"l"+e/2+",0l"+-e/2+",0z"})}),a}var b,d,e,f,g={top:0,right:0,bottom:0,left:0},h=960,i=500,j=Math.floor(1e4*Math.random()),k=d3.scale.linear(),l=d3.scale.linear(),m=function(a){return a.x},n=function(a){return a.y},o=function(a){return a.open},p=function(a){return a.close},q=function(a){return a.high},r=function(a){return a.low},s=[],t=[],u=!1,v=!0,w=c.utils.defaultColor(),x=d3.dispatch("chartClick","elementClick","elementDblClick","elementMouseover","elementMouseout");return a.dispatch=x,a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(m=b,a):m},a.y=function(b){return arguments.length?(n=b,a):n},a.open=function(b){return arguments.length?(o=b,a):o},a.close=function(b){return arguments.length?(p=b,a):p},a.high=function(b){return arguments.length?(q=b,a):q},a.low=function(b){return arguments.length?(r=b,a):r},a.margin=function(b){return arguments.length?(g.top="undefined"!=typeof b.top?b.top:g.top,g.right="undefined"!=typeof b.right?b.right:g.right,g.bottom="undefined"!=typeof b.bottom?b.bottom:g.bottom,g.left="undefined"!=typeof b.left?b.left:g.left,a):g},a.width=function(b){return arguments.length?(h=b,a):h},a.height=function(b){return arguments.length?(i=b,a):i},a.xScale=function(b){return arguments.length?(k=b,a):k},a.yScale=function(b){return arguments.length?(l=b,a):l},a.xDomain=function(c){return arguments.length?(b=c,a):b},a.yDomain=function(b){return arguments.length?(d=b,a):d},a.xRange=function(b){return arguments.length?(e=b,a):e},a.yRange=function(b){return arguments.length?(f=b,a):f},a.forceX=function(b){return arguments.length?(s=b,a):s},a.forceY=function(b){return arguments.length?(t=b,a):t},a.padData=function(b){return arguments.length?(u=b,a):u},a.clipEdge=function(b){return arguments.length?(v=b,a):v},a.color=function(b){return arguments.length?(w=c.utils.getColor(b),a):w},a.id=function(b){return arguments.length?(j=b,a):j},a},c.models.pie=function(){"use strict";function a(c){return c.each(function(a){function c(a){a.endAngle=isNaN(a.endAngle)?0:a.endAngle,a.startAngle=isNaN(a.startAngle)?0:a.startAngle,r||(a.innerRadius=0);var b=d3.interpolate(this._current,a);return this._current=b(0),function(a){return E(b(a))}}var i=e-b.left-b.right,l=f-b.top-b.bottom,x=Math.min(i,l)/2,y=x-x/5,z=d3.select(this),A=z.selectAll(".nv-wrap.nv-pie").data(a),B=A.enter().append("g").attr("class","nvd3 nv-wrap nv-pie nv-chart-"+j),C=B.append("g"),D=A.select("g");C.append("g").attr("class","nv-pie"),C.append("g").attr("class","nv-pieLabels"),A.attr("transform","translate("+b.left+","+b.top+")"),D.select(".nv-pie").attr("transform","translate("+i/2+","+l/2+")"),D.select(".nv-pieLabels").attr("transform","translate("+i/2+","+l/2+")"),z.on("click",function(a,b){w.chartClick({data:a,index:b,pos:d3.event,id:j})});var E=d3.svg.arc().outerRadius(y);t&&E.startAngle(t),u&&E.endAngle(u),r&&E.innerRadius(x*v);var F=d3.layout.pie().sort(null).value(function(a){return a.disabled?0:h(a)}),G=A.select(".nv-pie").selectAll(".nv-slice").data(F),H=A.select(".nv-pieLabels").selectAll(".nv-label").data(F);G.exit().remove(),H.exit().remove();var I=G.enter().append("g").attr("class","nv-slice").on("mouseover",function(a,b){d3.select(this).classed("hover",!0),w.elementMouseover({label:g(a.data),value:h(a.data),point:a.data,pointIndex:b,pos:[d3.event.pageX,d3.event.pageY],id:j})}).on("mouseout",function(a,b){d3.select(this).classed("hover",!1),w.elementMouseout({label:g(a.data),value:h(a.data),point:a.data,index:b,id:j})}).on("click",function(a,b){w.elementClick({label:g(a.data),value:h(a.data),point:a.data,index:b,pos:d3.event,id:j}),d3.event.stopPropagation()}).on("dblclick",function(a,b){w.elementDblClick({label:g(a.data),value:h(a.data),point:a.data,index:b,pos:d3.event,id:j}),d3.event.stopPropagation()});if(G.attr("fill",function(a,b){return k(a,b)}).attr("stroke",function(a,b){return k(a,b)}),I.append("path").each(function(a){this._current=a}),G.select("path").transition().attr("d",E).attrTween("d",c),m){var J=d3.svg.arc().innerRadius(0);n&&(J=E),o&&(J=d3.svg.arc().outerRadius(E.outerRadius())),H.enter().append("g").classed("nv-label",!0).each(function(a){var b=d3.select(this);b.attr("transform",function(a){if(s){a.outerRadius=y+10,a.innerRadius=y+15;var b=(a.startAngle+a.endAngle)/2*(180/Math.PI);return(a.startAngle+a.endAngle)/2<Math.PI?b-=90:b+=90,"translate("+J.centroid(a)+") rotate("+b+")"}return a.outerRadius=x+10,a.innerRadius=x+15,"translate("+J.centroid(a)+")"}),b.append("rect").style("stroke","#fff").style("fill","#fff").attr("rx",3).attr("ry",3),b.append("text").style("text-anchor",s?(a.startAngle+a.endAngle)/2<Math.PI?"start":"end":"middle").style("fill","#000")});var K={},L=14,M=140,N=function(a){return Math.floor(a[0]/M)*M+","+Math.floor(a[1]/L)*L};H.transition().attr("transform",function(a){if(s){a.outerRadius=y+10,a.innerRadius=y+15;var b=(a.startAngle+a.endAngle)/2*(180/Math.PI);return(a.startAngle+a.endAngle)/2<Math.PI?b-=90:b+=90,"translate("+J.centroid(a)+") rotate("+b+")"}a.outerRadius=x+10,a.innerRadius=x+15;var c=J.centroid(a),d=N(c);return K[d]&&(c[1]-=L),K[N(c)]=!0,"translate("+c+")"}),H.select(".nv-label text").style("text-anchor",s?(d.startAngle+d.endAngle)/2<Math.PI?"start":"end":"middle").text(function(a){var b=(a.endAngle-a.startAngle)/(2*Math.PI),c={key:g(a.data),value:h(a.data),percent:d3.format("%")(b)};return a.value&&b>q?c[p]:""})}}),a}var b={top:0,right:0,bottom:0,left:0},e=500,f=500,g=function(a){return a.x},h=function(a){return a.y},i=function(a){return a.description},j=Math.floor(1e4*Math.random()),k=c.utils.defaultColor(),l=d3.format(",.2f"),m=!0,n=!0,o=!1,p="key",q=.02,r=!1,s=!1,t=!1,u=!1,v=.5,w=d3.dispatch("chartClick","elementClick","elementDblClick","elementMouseover","elementMouseout");return a.dispatch=w,a.options=c.utils.optionsFunc.bind(a),a.margin=function(c){return arguments.length?(b.top="undefined"!=typeof c.top?c.top:b.top,b.right="undefined"!=typeof c.right?c.right:b.right,b.bottom="undefined"!=typeof c.bottom?c.bottom:b.bottom,b.left="undefined"!=typeof c.left?c.left:b.left,a):b},a.width=function(b){return arguments.length?(e=b,a):e},a.height=function(b){return arguments.length?(f=b,a):f},a.values=function(){return c.log("pie.values() is no longer supported."),a},a.x=function(b){return arguments.length?(g=b,a):g},a.y=function(b){return arguments.length?(h=d3.functor(b),a):h},a.description=function(b){return arguments.length?(i=b,a):i},a.showLabels=function(b){return arguments.length?(m=b,a):m},a.labelSunbeamLayout=function(b){return arguments.length?(s=b,a):s},a.donutLabelsOutside=function(b){return arguments.length?(o=b,a):o},a.pieLabelsOutside=function(b){return arguments.length?(n=b,a):n},a.labelType=function(b){return arguments.length?(p=b,p=p||"key",a):p},a.donut=function(b){return arguments.length?(r=b,a):r},a.donutRatio=function(b){return arguments.length?(v=b,a):v},a.startAngle=function(b){return arguments.length?(t=b,a):t},a.endAngle=function(b){return arguments.length?(u=b,a):u},a.id=function(b){return arguments.length?(j=b,a):j},a.color=function(b){return arguments.length?(k=c.utils.getColor(b),a):k},a.valueFormat=function(b){return arguments.length?(l=b,a):l},a.labelThreshold=function(b){return arguments.length?(q=b,a):q},a},c.models.pieChart=function(){"use strict";function a(c){return c.each(function(c){var i=d3.select(this),j=(f||parseInt(i.style("width"))||960)-e.left-e.right,k=(g||parseInt(i.style("height"))||400)-e.top-e.bottom;if(a.update=function(){i.transition().call(a)},a.container=this,l.disabled=c.map(function(a){return!!a.disabled}),!m){var p;m={};for(p in l)m[p]=l[p]instanceof Array?l[p].slice(0):l[p]}if(!c||!c.length){var q=i.selectAll(".nv-noData").data([n]);return q.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),q.attr("x",e.left+j/2).attr("y",e.top+k/2).text(function(a){return a}),a}i.selectAll(".nv-noData").remove();var r=i.selectAll("g.nv-wrap.nv-pieChart").data([c]),s=r.enter().append("g").attr("class","nvd3 nv-wrap nv-pieChart").append("g"),t=r.select("g");s.append("g").attr("class","nv-pieWrap"),s.append("g").attr("class","nv-legendWrap"),h&&(d.width(j).key(b.x()),r.select(".nv-legendWrap").datum(c).call(d),e.top!=d.height()&&(e.top=d.height(),k=(g||parseInt(i.style("height"))||400)-e.top-e.bottom),r.select(".nv-legendWrap").attr("transform","translate(0,"+-e.top+")")),r.attr("transform","translate("+e.left+","+e.top+")"),b.width(j).height(k);var u=t.select(".nv-pieWrap").datum([c]);d3.transition(u).call(b),d.dispatch.on("stateChange",function(b){l=b,o.stateChange(l),a.update()}),b.dispatch.on("elementMouseout.tooltip",function(a){o.tooltipHide(a)}),o.on("changeState",function(b){"undefined"!=typeof b.disabled&&(c.forEach(function(a,c){a.disabled=b.disabled[c]}),l.disabled=b.disabled),a.update()})}),a}var b=c.models.pie(),d=c.models.legend(),e={top:30,right:20,bottom:20,left:20},f=null,g=null,h=!0,i=c.utils.defaultColor(),j=!0,k=function(a,b){return"<h3>"+a+"</h3>"+"<p>"+b+"</p>"},l={},m=null,n="No Data Available.",o=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState"),p=function(d,e){var f=b.description()(d.point)||b.x()(d.point),g=d.pos[0]+(e&&e.offsetLeft||0),h=d.pos[1]+(e&&e.offsetTop||0),i=b.valueFormat()(b.y()(d.point)),j=k(f,i,d,a);c.tooltip.show([g,h],j,d.value<0?"n":"s",null,e)};return b.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+e.left,a.pos[1]+e.top],o.tooltipShow(a)}),o.on("tooltipShow",function(a){j&&p(a)}),o.on("tooltipHide",function(){j&&c.tooltip.cleanup()}),a.legend=d,a.dispatch=o,a.pie=b,d3.rebind(a,b,"valueFormat","values","x","y","description","id","showLabels","donutLabelsOutside","pieLabelsOutside","labelType","donut","donutRatio","labelThreshold"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(e.top="undefined"!=typeof b.top?b.top:e.top,e.right="undefined"!=typeof b.right?b.right:e.right,e.bottom="undefined"!=typeof b.bottom?b.bottom:e.bottom,e.left="undefined"!=typeof b.left?b.left:e.left,a):e},a.width=function(b){return arguments.length?(f=b,a):f},a.height=function(b){return arguments.length?(g=b,a):g},a.color=function(e){return arguments.length?(i=c.utils.getColor(e),d.color(i),b.color(i),a):i},a.showLegend=function(b){return arguments.length?(h=b,a):h},a.tooltips=function(b){return arguments.length?(j=b,a):j},a.tooltipContent=function(b){return arguments.length?(k=b,a):k},a.state=function(b){return arguments.length?(l=b,a):l},a.defaultState=function(b){return arguments.length?(m=b,a):m},a.noData=function(b){return arguments.length?(n=b,a):n},a},c.models.scatter=function(){"use strict";function a(O){return O.each(function(a){function O(){if(!w)return!1;var b=d3.merge(a.map(function(a,b){return a.values.map(function(a,c){var d=o(a,c),e=p(a,c);return[l(d)+1e-7*Math.random(),m(e)+1e-7*Math.random(),b,c,a]}).filter(function(a,b){return y(a[4],b)})}));if(M===!0){if(C){var c=T.select("defs").selectAll(".nv-point-clips").data([k]).enter();c.append("clipPath").attr("class","nv-point-clips").attr("id","nv-points-clip-"+k);var d=T.select("#nv-points-clip-"+k).selectAll("circle").data(b);d.enter().append("circle").attr("r",D),d.exit().remove(),d.attr("cx",function(a){return a[0]}).attr("cy",function(a){return a[1]}),T.select(".nv-point-paths").attr("clip-path","url(#nv-points-clip-"+k+")")}b.length&&(b.push([l.range()[0]-20,m.range()[0]-20,null,null]),b.push([l.range()[1]+20,m.range()[1]+20,null,null]),b.push([l.range()[0]-20,m.range()[0]+20,null,null]),b.push([l.range()[1]+20,m.range()[1]-20,null,null]));var e=d3.geom.polygon([[-10,-10],[-10,i+10],[h+10,i+10],[h+10,-10]]),f=d3.geom.voronoi(b).map(function(a,c){return{data:e.clip(a),series:b[c][2],point:b[c][3]}}),j=T.select(".nv-point-paths").selectAll("path").data(f);j.enter().append("path").attr("class",function(a,b){return"nv-path-"+b}),j.exit().remove(),j.attr("d",function(a){return 0===a.data.length?"M 0 0":"M"+a.data.join("L")+"Z"});var n=function(b,c){if(N)return 0;var d=a[b.series];if("undefined"!=typeof d){var e=d.values[b.point];c({point:e,series:d,pos:[l(o(e,b.point))+g.left,m(p(e,b.point))+g.top],seriesIndex:b.series,pointIndex:b.point})}};j.on("click",function(a){n(a,L.elementClick)}).on("mouseover",function(a){n(a,L.elementMouseover)}).on("mouseout",function(a){n(a,L.elementMouseout)})}else T.select(".nv-groups").selectAll(".nv-group").selectAll(".nv-point").on("click",function(b,c){if(N||!a[b.series])return 0;var d=a[b.series],e=d.values[c];L.elementClick({point:e,series:d,pos:[l(o(e,c))+g.left,m(p(e,c))+g.top],seriesIndex:b.series,pointIndex:c})}).on("mouseover",function(b,c){if(N||!a[b.series])return 0;var d=a[b.series],e=d.values[c];L.elementMouseover({point:e,series:d,pos:[l(o(e,c))+g.left,m(p(e,c))+g.top],seriesIndex:b.series,pointIndex:c})}).on("mouseout",function(b,c){if(N||!a[b.series])return 0;var d=a[b.series],e=d.values[c];L.elementMouseout({point:e,series:d,seriesIndex:b.series,pointIndex:c})});N=!1}var P=h-g.left-g.right,Q=i-g.top-g.bottom,R=d3.select(this);a.forEach(function(a,b){a.values.forEach(function(a){a.series=b})});var S=E&&F&&I?[]:d3.merge(a.map(function(a){return a.values.map(function(a,b){return{x:o(a,b),y:p(a,b),size:q(a,b)}})}));l.domain(E||d3.extent(S.map(function(a){return a.x}).concat(t))),z&&a[0]?l.range(G||[(P*A+P)/(2*a[0].values.length),P-P*(1+A)/(2*a[0].values.length)]):l.range(G||[0,P]),m.domain(F||d3.extent(S.map(function(a){return a.y}).concat(u))).range(H||[Q,0]),n.domain(I||d3.extent(S.map(function(a){return a.size}).concat(v))).range(J||[16,256]),(l.domain()[0]===l.domain()[1]||m.domain()[0]===m.domain()[1])&&(K=!0),l.domain()[0]===l.domain()[1]&&(l.domain()[0]?l.domain([l.domain()[0]-.01*l.domain()[0],l.domain()[1]+.01*l.domain()[1]]):l.domain([-1,1])),m.domain()[0]===m.domain()[1]&&(m.domain()[0]?m.domain([m.domain()[0]-.01*m.domain()[0],m.domain()[1]+.01*m.domain()[1]]):m.domain([-1,1])),isNaN(l.domain()[0])&&l.domain([-1,1]),isNaN(m.domain()[0])&&m.domain([-1,1]),b=b||l,d=d||m,e=e||n;var T=R.selectAll("g.nv-wrap.nv-scatter").data([a]),U=T.enter().append("g").attr("class","nvd3 nv-wrap nv-scatter nv-chart-"+k+(K?" nv-single-point":"")),V=U.append("defs"),W=U.append("g"),X=T.select("g");W.append("g").attr("class","nv-groups"),W.append("g").attr("class","nv-point-paths"),T.attr("transform","translate("+g.left+","+g.top+")"),V.append("clipPath").attr("id","nv-edge-clip-"+k).append("rect"),T.select("#nv-edge-clip-"+k+" rect").attr("width",P).attr("height",Q),X.attr("clip-path",B?"url(#nv-edge-clip-"+k+")":""),N=!0;var Y=T.select(".nv-groups").selectAll(".nv-group").data(function(a){return a},function(a){return a.key});if(Y.enter().append("g").style("stroke-opacity",1e-6).style("fill-opacity",1e-6),Y.exit().remove(),Y.attr("class",function(a,b){return"nv-group nv-series-"+b}).classed("hover",function(a){return a.hover}),Y.transition().style("fill",function(a,b){return j(a,b)}).style("stroke",function(a,b){return j(a,b)}).style("stroke-opacity",1).style("fill-opacity",.5),s){var Z=Y.selectAll("circle.nv-point").data(function(a){return a.values},x);Z.enter().append("circle").style("fill",function(a){return a.color}).style("stroke",function(a){return a.color}).attr("cx",function(a,d){return c.utils.NaNtoZero(b(o(a,d)))}).attr("cy",function(a,b){return c.utils.NaNtoZero(d(p(a,b)))}).attr("r",function(a,b){return Math.sqrt(n(q(a,b))/Math.PI)}),Z.exit().remove(),Y.exit().selectAll("path.nv-point").transition().attr("cx",function(a,b){return c.utils.NaNtoZero(l(o(a,b)))}).attr("cy",function(a,b){return c.utils.NaNtoZero(m(p(a,b)))}).remove(),Z.each(function(a,b){d3.select(this).classed("nv-point",!0).classed("nv-point-"+b,!0).classed("hover",!1)}),Z.transition().attr("cx",function(a,b){return c.utils.NaNtoZero(l(o(a,b)))}).attr("cy",function(a,b){return c.utils.NaNtoZero(m(p(a,b)))}).attr("r",function(a,b){return Math.sqrt(n(q(a,b))/Math.PI)})}else{var Z=Y.selectAll("path.nv-point").data(function(a){return a.values});Z.enter().append("path").style("fill",function(a){return a.color}).style("stroke",function(a){return a.color}).attr("transform",function(a,c){return"translate("+b(o(a,c))+","+d(p(a,c))+")"}).attr("d",d3.svg.symbol().type(r).size(function(a,b){return n(q(a,b))})),Z.exit().remove(),Y.exit().selectAll("path.nv-point").transition().attr("transform",function(a,b){return"translate("+l(o(a,b))+","+m(p(a,b))+")"}).remove(),Z.each(function(a,b){d3.select(this).classed("nv-point",!0).classed("nv-point-"+b,!0).classed("hover",!1)}),Z.transition().attr("transform",function(a,b){return"translate("+l(o(a,b))+","+m(p(a,b))+")"}).attr("d",d3.svg.symbol().type(r).size(function(a,b){return n(q(a,b))}))}clearTimeout(f),f=setTimeout(O,300),b=l.copy(),d=m.copy(),e=n.copy()}),a}var b,d,e,f,g={top:0,right:0,bottom:0,left:0},h=960,i=500,j=c.utils.defaultColor(),k=Math.floor(1e5*Math.random()),l=d3.scale.linear(),m=d3.scale.linear(),n=d3.scale.linear(),o=function(a){return a.x},p=function(a){return a.y},q=function(a){return a.size||1},r=function(a){return a.shape||"circle"},s=!0,t=[],u=[],v=[],w=!0,x=null,y=function(a){return!a.notActive},z=!1,A=.1,B=!1,C=!0,D=function(){return 25},E=null,F=null,G=null,H=null,I=null,J=null,K=!1,L=d3.dispatch("elementClick","elementMouseover","elementMouseout"),M=!0,N=!1;return a.clearHighlights=function(){d3.selectAll(".nv-chart-"+k+" .nv-point.hover").classed("hover",!1)},a.highlightPoint=function(a,b,c){d3.select(".nv-chart-"+k+" .nv-series-"+a+" .nv-point-"+b).classed("hover",c)},L.on("elementMouseover.point",function(b){w&&a.highlightPoint(b.seriesIndex,b.pointIndex,!0)}),L.on("elementMouseout.point",function(b){w&&a.highlightPoint(b.seriesIndex,b.pointIndex,!1)}),a.dispatch=L,a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(o=d3.functor(b),a):o},a.y=function(b){return arguments.length?(p=d3.functor(b),a):p},a.size=function(b){return arguments.length?(q=d3.functor(b),a):q},a.margin=function(b){return arguments.length?(g.top="undefined"!=typeof b.top?b.top:g.top,g.right="undefined"!=typeof b.right?b.right:g.right,g.bottom="undefined"!=typeof b.bottom?b.bottom:g.bottom,g.left="undefined"!=typeof b.left?b.left:g.left,a):g},a.width=function(b){return arguments.length?(h=b,a):h},a.height=function(b){return arguments.length?(i=b,a):i},a.xScale=function(b){return arguments.length?(l=b,a):l},a.yScale=function(b){return arguments.length?(m=b,a):m},a.zScale=function(b){return arguments.length?(n=b,a):n},a.xDomain=function(b){return arguments.length?(E=b,a):E},a.yDomain=function(b){return arguments.length?(F=b,a):F},a.sizeDomain=function(b){return arguments.length?(I=b,a):I},a.xRange=function(b){return arguments.length?(G=b,a):G},a.yRange=function(b){return arguments.length?(H=b,a):H},a.sizeRange=function(b){return arguments.length?(J=b,a):J},a.forceX=function(b){return arguments.length?(t=b,a):t},a.forceY=function(b){return arguments.length?(u=b,a):u},a.forceSize=function(b){return arguments.length?(v=b,a):v},a.interactive=function(b){return arguments.length?(w=b,a):w},a.pointKey=function(b){return arguments.length?(x=b,a):x},a.pointActive=function(b){return arguments.length?(y=b,a):y},a.padData=function(b){return arguments.length?(z=b,a):z},a.padDataOuter=function(b){return arguments.length?(A=b,a):A},a.clipEdge=function(b){return arguments.length?(B=b,a):B},a.clipVoronoi=function(b){return arguments.length?(C=b,a):C},a.useVoronoi=function(b){return arguments.length?(M=b,M===!1&&(C=!1),a):M},a.clipRadius=function(b){return arguments.length?(D=b,a):D},a.color=function(b){return arguments.length?(j=c.utils.getColor(b),a):j},a.shape=function(b){return arguments.length?(r=b,a):r},a.onlyCircles=function(b){return arguments.length?(s=b,a):s},a.id=function(b){return arguments.length?(k=b,a):k},a.singlePoint=function(b){return arguments.length?(K=b,a):K},a},c.models.scatterChart=function(){"use strict";function a(c){return c.each(function(c){function B(){if(z)return U.select(".nv-point-paths").style("pointer-events","all"),!1;U.select(".nv-point-paths").style("pointer-events","none");var a=d3.mouse(this);n.distortion(y).focus(a[0]),o.distortion(y).focus(a[1]),U.select(".nv-scatterWrap").call(b),u&&U.select(".nv-x.nv-axis").call(d),v&&U.select(".nv-y.nv-axis").call(e),U.select(".nv-distributionX").datum(c.filter(function(a){return!a.disabled})).call(h),U.select(".nv-distributionY").datum(c.filter(function(a){return!a.disabled})).call(i)}var C=d3.select(this),D=this,N=(k||parseInt(C.style("width"))||960)-j.left-j.right,O=(l||parseInt(C.style("height"))||400)-j.top-j.bottom;if(a.update=function(){C.transition().duration(I).call(a)},a.container=this,E.disabled=c.map(function(a){return!!a.disabled}),!F){var P;F={};for(P in E)F[P]=E[P]instanceof Array?E[P].slice(0):E[P]}if(!(c&&c.length&&c.filter(function(a){return a.values.length}).length)){var Q=C.selectAll(".nv-noData").data([H]);return Q.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),Q.attr("x",j.left+N/2).attr("y",j.top+O/2).text(function(a){return a}),a}C.selectAll(".nv-noData").remove(),J=J||n,K=K||o;var R=C.selectAll("g.nv-wrap.nv-scatterChart").data([c]),S=R.enter().append("g").attr("class","nvd3 nv-wrap nv-scatterChart nv-chart-"+b.id()),T=S.append("g"),U=R.select("g");if(T.append("rect").attr("class","nvd3 nv-background"),T.append("g").attr("class","nv-x nv-axis"),T.append("g").attr("class","nv-y nv-axis"),T.append("g").attr("class","nv-scatterWrap"),T.append("g").attr("class","nv-distWrap"),T.append("g").attr("class","nv-legendWrap"),T.append("g").attr("class","nv-controlsWrap"),t){var V=x?N/2:N;f.width(V),R.select(".nv-legendWrap").datum(c).call(f),j.top!=f.height()&&(j.top=f.height(),O=(l||parseInt(C.style("height"))||400)-j.top-j.bottom),R.select(".nv-legendWrap").attr("transform","translate("+(N-V)+","+-j.top+")")}if(x&&(g.width(180).color(["#444"]),U.select(".nv-controlsWrap").datum(M).attr("transform","translate(0,"+-j.top+")").call(g)),R.attr("transform","translate("+j.left+","+j.top+")"),w&&U.select(".nv-y.nv-axis").attr("transform","translate("+N+",0)"),b.width(N).height(O).color(c.map(function(a,b){return a.color||m(a,b)}).filter(function(a,b){return!c[b].disabled})),0!==p&&b.xDomain(null),0!==q&&b.yDomain(null),R.select(".nv-scatterWrap").datum(c.filter(function(a){return!a.disabled})).call(b),0!==p){var W=n.domain()[1]-n.domain()[0];b.xDomain([n.domain()[0]-p*W,n.domain()[1]+p*W])}if(0!==q){var X=o.domain()[1]-o.domain()[0];b.yDomain([o.domain()[0]-q*X,o.domain()[1]+q*X])}(0!==q||0!==p)&&R.select(".nv-scatterWrap").datum(c.filter(function(a){return!a.disabled})).call(b),u&&(d.scale(n).ticks(d.ticks()&&d.ticks().length?d.ticks():N/100).tickSize(-O,0),U.select(".nv-x.nv-axis").attr("transform","translate(0,"+o.range()[0]+")").call(d)),v&&(e.scale(o).ticks(e.ticks()&&e.ticks().length?e.ticks():O/36).tickSize(-N,0),U.select(".nv-y.nv-axis").call(e)),r&&(h.getData(b.x()).scale(n).width(N).color(c.map(function(a,b){return a.color||m(a,b)}).filter(function(a,b){return!c[b].disabled})),T.select(".nv-distWrap").append("g").attr("class","nv-distributionX"),U.select(".nv-distributionX").attr("transform","translate(0,"+o.range()[0]+")").datum(c.filter(function(a){return!a.disabled})).call(h)),s&&(i.getData(b.y()).scale(o).width(O).color(c.map(function(a,b){return a.color||m(a,b)}).filter(function(a,b){return!c[b].disabled})),T.select(".nv-distWrap").append("g").attr("class","nv-distributionY"),U.select(".nv-distributionY").attr("transform","translate("+(w?N:-i.size())+",0)").datum(c.filter(function(a){return!a.disabled})).call(i)),d3.fisheye&&(U.select(".nv-background").attr("width",N).attr("height",O),U.select(".nv-background").on("mousemove",B),U.select(".nv-background").on("click",function(){z=!z}),b.dispatch.on("elementClick.freezeFisheye",function(){z=!z})),g.dispatch.on("legendClick",function(c){c.disabled=!c.disabled,y=c.disabled?0:2.5,U.select(".nv-background").style("pointer-events",c.disabled?"none":"all"),U.select(".nv-point-paths").style("pointer-events",c.disabled?"all":"none"),c.disabled?(n.distortion(y).focus(0),o.distortion(y).focus(0),U.select(".nv-scatterWrap").call(b),U.select(".nv-x.nv-axis").call(d),U.select(".nv-y.nv-axis").call(e)):z=!1,a.update()}),f.dispatch.on("stateChange",function(b){E.disabled=b.disabled,G.stateChange(E),a.update()}),b.dispatch.on("elementMouseover.tooltip",function(a){d3.select(".nv-chart-"+b.id()+" .nv-series-"+a.seriesIndex+" .nv-distx-"+a.pointIndex).attr("y1",function(){return a.pos[1]-O}),d3.select(".nv-chart-"+b.id()+" .nv-series-"+a.seriesIndex+" .nv-disty-"+a.pointIndex).attr("x2",a.pos[0]+h.size()),a.pos=[a.pos[0]+j.left,a.pos[1]+j.top],G.tooltipShow(a)}),G.on("tooltipShow",function(a){A&&L(a,D.parentNode)}),G.on("changeState",function(b){"undefined"!=typeof b.disabled&&(c.forEach(function(a,c){a.disabled=b.disabled[c]}),E.disabled=b.disabled),a.update()}),J=n.copy(),K=o.copy()}),a}var b=c.models.scatter(),d=c.models.axis(),e=c.models.axis(),f=c.models.legend(),g=c.models.legend(),h=c.models.distribution(),i=c.models.distribution(),j={top:30,right:20,bottom:50,left:75},k=null,l=null,m=c.utils.defaultColor(),n=d3.fisheye?d3.fisheye.scale(d3.scale.linear).distortion(0):b.xScale(),o=d3.fisheye?d3.fisheye.scale(d3.scale.linear).distortion(0):b.yScale(),p=0,q=0,r=!1,s=!1,t=!0,u=!0,v=!0,w=!1,x=!!d3.fisheye,y=0,z=!1,A=!0,B=function(a,b){return"<strong>"+b+"</strong>"},C=function(a,b,c){return"<strong>"+c+"</strong>"},D=null,E={},F=null,G=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState"),H="No Data Available.",I=250;b.xScale(n).yScale(o),d.orient("bottom").tickPadding(10),e.orient(w?"right":"left").tickPadding(10),h.axis("x"),i.axis("y"),g.updateState(!1);var J,K,L=function(f,g){var h=f.pos[0]+(g.offsetLeft||0),i=f.pos[1]+(g.offsetTop||0),k=f.pos[0]+(g.offsetLeft||0),l=o.range()[0]+j.top+(g.offsetTop||0),m=n.range()[0]+j.left+(g.offsetLeft||0),p=f.pos[1]+(g.offsetTop||0),q=d.tickFormat()(b.x()(f.point,f.pointIndex)),r=e.tickFormat()(b.y()(f.point,f.pointIndex));null!=B&&c.tooltip.show([k,l],B(f.series.key,q,r,f,a),"n",1,g,"x-nvtooltip"),null!=C&&c.tooltip.show([m,p],C(f.series.key,q,r,f,a),"e",1,g,"y-nvtooltip"),null!=D&&c.tooltip.show([h,i],D(f.series.key,q,r,f,a),f.value<0?"n":"s",null,g)},M=[{key:"Magnify",disabled:!0}];return b.dispatch.on("elementMouseout.tooltip",function(a){G.tooltipHide(a),d3.select(".nv-chart-"+b.id()+" .nv-series-"+a.seriesIndex+" .nv-distx-"+a.pointIndex).attr("y1",0),d3.select(".nv-chart-"+b.id()+" .nv-series-"+a.seriesIndex+" .nv-disty-"+a.pointIndex).attr("x2",i.size())}),G.on("tooltipHide",function(){A&&c.tooltip.cleanup()}),a.dispatch=G,a.scatter=b,a.legend=f,a.controls=g,a.xAxis=d,a.yAxis=e,a.distX=h,a.distY=i,d3.rebind(a,b,"id","interactive","pointActive","x","y","shape","size","xScale","yScale","zScale","xDomain","yDomain","xRange","yRange","sizeDomain","sizeRange","forceX","forceY","forceSize","clipVoronoi","clipRadius","useVoronoi"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(j.top="undefined"!=typeof b.top?b.top:j.top,j.right="undefined"!=typeof b.right?b.right:j.right,j.bottom="undefined"!=typeof b.bottom?b.bottom:j.bottom,j.left="undefined"!=typeof b.left?b.left:j.left,a):j},a.width=function(b){return arguments.length?(k=b,a):k},a.height=function(b){return arguments.length?(l=b,a):l},a.color=function(b){return arguments.length?(m=c.utils.getColor(b),f.color(m),h.color(m),i.color(m),a):m},a.showDistX=function(b){return arguments.length?(r=b,a):r},a.showDistY=function(b){return arguments.length?(s=b,a):s},a.showControls=function(b){return arguments.length?(x=b,a):x},a.showLegend=function(b){return arguments.length?(t=b,a):t},a.showXAxis=function(b){return arguments.length?(u=b,a):u},a.showYAxis=function(b){return arguments.length?(v=b,a):v},a.rightAlignYAxis=function(b){return arguments.length?(w=b,e.orient(b?"right":"left"),a):w},a.fisheye=function(b){return arguments.length?(y=b,a):y},a.xPadding=function(b){return arguments.length?(p=b,a):p},a.yPadding=function(b){return arguments.length?(q=b,a):q},a.tooltips=function(b){return arguments.length?(A=b,a):A},a.tooltipContent=function(b){return arguments.length?(D=b,a):D},a.tooltipXContent=function(b){return arguments.length?(B=b,a):B},a.tooltipYContent=function(b){return arguments.length?(C=b,a):C},a.state=function(b){return arguments.length?(E=b,a):E},a.defaultState=function(b){return arguments.length?(F=b,a):F},a.noData=function(b){return arguments.length?(H=b,a):H},a.transitionDuration=function(b){return arguments.length?(I=b,a):I},a},c.models.scatterPlusLineChart=function(){"use strict";function a(c){return c.each(function(c){function z(){if(x)return S.select(".nv-point-paths").style("pointer-events","all"),!1;S.select(".nv-point-paths").style("pointer-events","none");var a=d3.mouse(this);n.distortion(w).focus(a[0]),o.distortion(w).focus(a[1]),S.select(".nv-scatterWrap").datum(c.filter(function(a){return!a.disabled})).call(b),s&&S.select(".nv-x.nv-axis").call(d),t&&S.select(".nv-y.nv-axis").call(e),S.select(".nv-distributionX").datum(c.filter(function(a){return!a.disabled})).call(h),S.select(".nv-distributionY").datum(c.filter(function(a){return!a.disabled})).call(i)}var A=d3.select(this),B=this,L=(k||parseInt(A.style("width"))||960)-j.left-j.right,M=(l||parseInt(A.style("height"))||400)-j.top-j.bottom;if(a.update=function(){A.transition().duration(G).call(a)},a.container=this,C.disabled=c.map(function(a){return!!a.disabled}),!D){var N;D={};for(N in C)D[N]=C[N]instanceof Array?C[N].slice(0):C[N]}if(!(c&&c.length&&c.filter(function(a){return a.values.length}).length)){var O=A.selectAll(".nv-noData").data([F]);return O.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),O.attr("x",j.left+L/2).attr("y",j.top+M/2).text(function(a){return a}),a}A.selectAll(".nv-noData").remove(),n=b.xScale(),o=b.yScale(),H=H||n,I=I||o;var P=A.selectAll("g.nv-wrap.nv-scatterChart").data([c]),Q=P.enter().append("g").attr("class","nvd3 nv-wrap nv-scatterChart nv-chart-"+b.id()),R=Q.append("g"),S=P.select("g");R.append("rect").attr("class","nvd3 nv-background").style("pointer-events","none"),R.append("g").attr("class","nv-x nv-axis"),R.append("g").attr("class","nv-y nv-axis"),R.append("g").attr("class","nv-scatterWrap"),R.append("g").attr("class","nv-regressionLinesWrap"),R.append("g").attr("class","nv-distWrap"),R.append("g").attr("class","nv-legendWrap"),R.append("g").attr("class","nv-controlsWrap"),P.attr("transform","translate("+j.left+","+j.top+")"),u&&S.select(".nv-y.nv-axis").attr("transform","translate("+L+",0)"),r&&(f.width(L/2),P.select(".nv-legendWrap").datum(c).call(f),j.top!=f.height()&&(j.top=f.height(),M=(l||parseInt(A.style("height"))||400)-j.top-j.bottom),P.select(".nv-legendWrap").attr("transform","translate("+L/2+","+-j.top+")")),v&&(g.width(180).color(["#444"]),S.select(".nv-controlsWrap").datum(K).attr("transform","translate(0,"+-j.top+")").call(g)),b.width(L).height(M).color(c.map(function(a,b){return a.color||m(a,b)
}).filter(function(a,b){return!c[b].disabled})),P.select(".nv-scatterWrap").datum(c.filter(function(a){return!a.disabled})).call(b),P.select(".nv-regressionLinesWrap").attr("clip-path","url(#nv-edge-clip-"+b.id()+")");var T=P.select(".nv-regressionLinesWrap").selectAll(".nv-regLines").data(function(a){return a});T.enter().append("g").attr("class","nv-regLines");var U=T.selectAll(".nv-regLine").data(function(a){return[a]});U.enter().append("line").attr("class","nv-regLine").style("stroke-opacity",0),U.transition().attr("x1",n.range()[0]).attr("x2",n.range()[1]).attr("y1",function(a){return o(n.domain()[0]*a.slope+a.intercept)}).attr("y2",function(a){return o(n.domain()[1]*a.slope+a.intercept)}).style("stroke",function(a,b,c){return m(a,c)}).style("stroke-opacity",function(a){return a.disabled||"undefined"==typeof a.slope||"undefined"==typeof a.intercept?0:1}),s&&(d.scale(n).ticks(d.ticks()?d.ticks():L/100).tickSize(-M,0),S.select(".nv-x.nv-axis").attr("transform","translate(0,"+o.range()[0]+")").call(d)),t&&(e.scale(o).ticks(e.ticks()?e.ticks():M/36).tickSize(-L,0),S.select(".nv-y.nv-axis").call(e)),p&&(h.getData(b.x()).scale(n).width(L).color(c.map(function(a,b){return a.color||m(a,b)}).filter(function(a,b){return!c[b].disabled})),R.select(".nv-distWrap").append("g").attr("class","nv-distributionX"),S.select(".nv-distributionX").attr("transform","translate(0,"+o.range()[0]+")").datum(c.filter(function(a){return!a.disabled})).call(h)),q&&(i.getData(b.y()).scale(o).width(M).color(c.map(function(a,b){return a.color||m(a,b)}).filter(function(a,b){return!c[b].disabled})),R.select(".nv-distWrap").append("g").attr("class","nv-distributionY"),S.select(".nv-distributionY").attr("transform","translate("+(u?L:-i.size())+",0)").datum(c.filter(function(a){return!a.disabled})).call(i)),d3.fisheye&&(S.select(".nv-background").attr("width",L).attr("height",M),S.select(".nv-background").on("mousemove",z),S.select(".nv-background").on("click",function(){x=!x}),b.dispatch.on("elementClick.freezeFisheye",function(){x=!x})),g.dispatch.on("legendClick",function(c){c.disabled=!c.disabled,w=c.disabled?0:2.5,S.select(".nv-background").style("pointer-events",c.disabled?"none":"all"),S.select(".nv-point-paths").style("pointer-events",c.disabled?"all":"none"),c.disabled?(n.distortion(w).focus(0),o.distortion(w).focus(0),S.select(".nv-scatterWrap").call(b),S.select(".nv-x.nv-axis").call(d),S.select(".nv-y.nv-axis").call(e)):x=!1,a.update()}),f.dispatch.on("stateChange",function(b){C=b,E.stateChange(C),a.update()}),b.dispatch.on("elementMouseover.tooltip",function(a){d3.select(".nv-chart-"+b.id()+" .nv-series-"+a.seriesIndex+" .nv-distx-"+a.pointIndex).attr("y1",a.pos[1]-M),d3.select(".nv-chart-"+b.id()+" .nv-series-"+a.seriesIndex+" .nv-disty-"+a.pointIndex).attr("x2",a.pos[0]+h.size()),a.pos=[a.pos[0]+j.left,a.pos[1]+j.top],E.tooltipShow(a)}),E.on("tooltipShow",function(a){y&&J(a,B.parentNode)}),E.on("changeState",function(b){"undefined"!=typeof b.disabled&&(c.forEach(function(a,c){a.disabled=b.disabled[c]}),C.disabled=b.disabled),a.update()}),H=n.copy(),I=o.copy()}),a}var b=c.models.scatter(),d=c.models.axis(),e=c.models.axis(),f=c.models.legend(),g=c.models.legend(),h=c.models.distribution(),i=c.models.distribution(),j={top:30,right:20,bottom:50,left:75},k=null,l=null,m=c.utils.defaultColor(),n=d3.fisheye?d3.fisheye.scale(d3.scale.linear).distortion(0):b.xScale(),o=d3.fisheye?d3.fisheye.scale(d3.scale.linear).distortion(0):b.yScale(),p=!1,q=!1,r=!0,s=!0,t=!0,u=!1,v=!!d3.fisheye,w=0,x=!1,y=!0,z=function(a,b){return"<strong>"+b+"</strong>"},A=function(a,b,c){return"<strong>"+c+"</strong>"},B=function(a,b,c,d){return"<h3>"+a+"</h3>"+"<p>"+d+"</p>"},C={},D=null,E=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState"),F="No Data Available.",G=250;b.xScale(n).yScale(o),d.orient("bottom").tickPadding(10),e.orient(u?"right":"left").tickPadding(10),h.axis("x"),i.axis("y"),g.updateState(!1);var H,I,J=function(f,g){var h=f.pos[0]+(g.offsetLeft||0),i=f.pos[1]+(g.offsetTop||0),k=f.pos[0]+(g.offsetLeft||0),l=o.range()[0]+j.top+(g.offsetTop||0),m=n.range()[0]+j.left+(g.offsetLeft||0),p=f.pos[1]+(g.offsetTop||0),q=d.tickFormat()(b.x()(f.point,f.pointIndex)),r=e.tickFormat()(b.y()(f.point,f.pointIndex));null!=z&&c.tooltip.show([k,l],z(f.series.key,q,r,f,a),"n",1,g,"x-nvtooltip"),null!=A&&c.tooltip.show([m,p],A(f.series.key,q,r,f,a),"e",1,g,"y-nvtooltip"),null!=B&&c.tooltip.show([h,i],B(f.series.key,q,r,f.point.tooltip,f,a),f.value<0?"n":"s",null,g)},K=[{key:"Magnify",disabled:!0}];return b.dispatch.on("elementMouseout.tooltip",function(a){E.tooltipHide(a),d3.select(".nv-chart-"+b.id()+" .nv-series-"+a.seriesIndex+" .nv-distx-"+a.pointIndex).attr("y1",0),d3.select(".nv-chart-"+b.id()+" .nv-series-"+a.seriesIndex+" .nv-disty-"+a.pointIndex).attr("x2",i.size())}),E.on("tooltipHide",function(){y&&c.tooltip.cleanup()}),a.dispatch=E,a.scatter=b,a.legend=f,a.controls=g,a.xAxis=d,a.yAxis=e,a.distX=h,a.distY=i,d3.rebind(a,b,"id","interactive","pointActive","x","y","shape","size","xScale","yScale","zScale","xDomain","yDomain","xRange","yRange","sizeDomain","sizeRange","forceX","forceY","forceSize","clipVoronoi","clipRadius","useVoronoi"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(j.top="undefined"!=typeof b.top?b.top:j.top,j.right="undefined"!=typeof b.right?b.right:j.right,j.bottom="undefined"!=typeof b.bottom?b.bottom:j.bottom,j.left="undefined"!=typeof b.left?b.left:j.left,a):j},a.width=function(b){return arguments.length?(k=b,a):k},a.height=function(b){return arguments.length?(l=b,a):l},a.color=function(b){return arguments.length?(m=c.utils.getColor(b),f.color(m),h.color(m),i.color(m),a):m},a.showDistX=function(b){return arguments.length?(p=b,a):p},a.showDistY=function(b){return arguments.length?(q=b,a):q},a.showControls=function(b){return arguments.length?(v=b,a):v},a.showLegend=function(b){return arguments.length?(r=b,a):r},a.showXAxis=function(b){return arguments.length?(s=b,a):s},a.showYAxis=function(b){return arguments.length?(t=b,a):t},a.rightAlignYAxis=function(b){return arguments.length?(u=b,e.orient(b?"right":"left"),a):u},a.fisheye=function(b){return arguments.length?(w=b,a):w},a.tooltips=function(b){return arguments.length?(y=b,a):y},a.tooltipContent=function(b){return arguments.length?(B=b,a):B},a.tooltipXContent=function(b){return arguments.length?(z=b,a):z},a.tooltipYContent=function(b){return arguments.length?(A=b,a):A},a.state=function(b){return arguments.length?(C=b,a):C},a.defaultState=function(b){return arguments.length?(D=b,a):D},a.noData=function(b){return arguments.length?(F=b,a):F},a.transitionDuration=function(b){return arguments.length?(G=b,a):G},a},c.models.sparkline=function(){"use strict";function a(c){return c.each(function(a){var c=h-g.left-g.right,j=i-g.top-g.bottom,p=d3.select(this);k.domain(b||d3.extent(a,m)).range(e||[0,c]),l.domain(d||d3.extent(a,n)).range(f||[j,0]);var q=p.selectAll("g.nv-wrap.nv-sparkline").data([a]),r=q.enter().append("g").attr("class","nvd3 nv-wrap nv-sparkline");r.append("g"),q.select("g"),q.attr("transform","translate("+g.left+","+g.top+")");var s=q.selectAll("path").data(function(a){return[a]});s.enter().append("path"),s.exit().remove(),s.style("stroke",function(a,b){return a.color||o(a,b)}).attr("d",d3.svg.line().x(function(a,b){return k(m(a,b))}).y(function(a,b){return l(n(a,b))}));var t=q.selectAll("circle.nv-point").data(function(a){function b(b){if(-1!=b){var c=a[b];return c.pointIndex=b,c}return null}var c=a.map(function(a,b){return n(a,b)}),d=b(c.lastIndexOf(l.domain()[1])),e=b(c.indexOf(l.domain()[0])),f=b(c.length-1);return[e,d,f].filter(function(a){return null!=a})});t.enter().append("circle"),t.exit().remove(),t.attr("cx",function(a){return k(m(a,a.pointIndex))}).attr("cy",function(a){return l(n(a,a.pointIndex))}).attr("r",2).attr("class",function(a){return m(a,a.pointIndex)==k.domain()[1]?"nv-point nv-currentValue":n(a,a.pointIndex)==l.domain()[0]?"nv-point nv-minValue":"nv-point nv-maxValue"})}),a}var b,d,e,f,g={top:2,right:0,bottom:2,left:0},h=400,i=32,j=!0,k=d3.scale.linear(),l=d3.scale.linear(),m=function(a){return a.x},n=function(a){return a.y},o=c.utils.getColor(["#000"]);return a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(g.top="undefined"!=typeof b.top?b.top:g.top,g.right="undefined"!=typeof b.right?b.right:g.right,g.bottom="undefined"!=typeof b.bottom?b.bottom:g.bottom,g.left="undefined"!=typeof b.left?b.left:g.left,a):g},a.width=function(b){return arguments.length?(h=b,a):h},a.height=function(b){return arguments.length?(i=b,a):i},a.x=function(b){return arguments.length?(m=d3.functor(b),a):m},a.y=function(b){return arguments.length?(n=d3.functor(b),a):n},a.xScale=function(b){return arguments.length?(k=b,a):k},a.yScale=function(b){return arguments.length?(l=b,a):l},a.xDomain=function(c){return arguments.length?(b=c,a):b},a.yDomain=function(b){return arguments.length?(d=b,a):d},a.xRange=function(b){return arguments.length?(e=b,a):e},a.yRange=function(b){return arguments.length?(f=b,a):f},a.animate=function(b){return arguments.length?(j=b,a):j},a.color=function(b){return arguments.length?(o=c.utils.getColor(b),a):o},a},c.models.sparklinePlus=function(){"use strict";function a(c){return c.each(function(m){function q(){if(!j){var a=A.selectAll(".nv-hoverValue").data(i),c=a.enter().append("g").attr("class","nv-hoverValue").style("stroke-opacity",0).style("fill-opacity",0);a.exit().transition().duration(250).style("stroke-opacity",0).style("fill-opacity",0).remove(),a.attr("transform",function(a){return"translate("+b(e.x()(m[a],a))+",0)"}).transition().duration(250).style("stroke-opacity",1).style("fill-opacity",1),i.length&&(c.append("line").attr("x1",0).attr("y1",-f.top).attr("x2",0).attr("y2",u),c.append("text").attr("class","nv-xValue").attr("x",-6).attr("y",-f.top).attr("text-anchor","end").attr("dy",".9em"),A.select(".nv-hoverValue .nv-xValue").text(k(e.x()(m[i[0]],i[0]))),c.append("text").attr("class","nv-yValue").attr("x",6).attr("y",-f.top).attr("text-anchor","start").attr("dy",".9em"),A.select(".nv-hoverValue .nv-yValue").text(l(e.y()(m[i[0]],i[0]))))}}function r(){function a(a,b){for(var c=Math.abs(e.x()(a[0],0)-b),d=0,f=0;f<a.length;f++)Math.abs(e.x()(a[f],f)-b)<c&&(c=Math.abs(e.x()(a[f],f)-b),d=f);return d}if(!j){var c=d3.mouse(this)[0]-f.left;i=[a(m,Math.round(b.invert(c)))],q()}}var s=d3.select(this),t=(g||parseInt(s.style("width"))||960)-f.left-f.right,u=(h||parseInt(s.style("height"))||400)-f.top-f.bottom;if(a.update=function(){a(c)},a.container=this,!m||!m.length){var v=s.selectAll(".nv-noData").data([p]);return v.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),v.attr("x",f.left+t/2).attr("y",f.top+u/2).text(function(a){return a}),a}s.selectAll(".nv-noData").remove();var w=e.y()(m[m.length-1],m.length-1);b=e.xScale(),d=e.yScale();var x=s.selectAll("g.nv-wrap.nv-sparklineplus").data([m]),y=x.enter().append("g").attr("class","nvd3 nv-wrap nv-sparklineplus"),z=y.append("g"),A=x.select("g");z.append("g").attr("class","nv-sparklineWrap"),z.append("g").attr("class","nv-valueWrap"),z.append("g").attr("class","nv-hoverArea"),x.attr("transform","translate("+f.left+","+f.top+")");var B=A.select(".nv-sparklineWrap");e.width(t).height(u),B.call(e);var C=A.select(".nv-valueWrap"),D=C.selectAll(".nv-currentValue").data([w]);D.enter().append("text").attr("class","nv-currentValue").attr("dx",o?-8:8).attr("dy",".9em").style("text-anchor",o?"end":"start"),D.attr("x",t+(o?f.right:0)).attr("y",n?function(a){return d(a)}:0).style("fill",e.color()(m[m.length-1],m.length-1)).text(l(w)),z.select(".nv-hoverArea").append("rect").on("mousemove",r).on("click",function(){j=!j}).on("mouseout",function(){i=[],q()}),A.select(".nv-hoverArea rect").attr("transform",function(){return"translate("+-f.left+","+-f.top+")"}).attr("width",t+f.left+f.right).attr("height",u+f.top)}),a}var b,d,e=c.models.sparkline(),f={top:15,right:100,bottom:10,left:50},g=null,h=null,i=[],j=!1,k=d3.format(",r"),l=d3.format(",.2f"),m=!0,n=!0,o=!1,p="No Data Available.";return a.sparkline=e,d3.rebind(a,e,"x","y","xScale","yScale","color"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(f.top="undefined"!=typeof b.top?b.top:f.top,f.right="undefined"!=typeof b.right?b.right:f.right,f.bottom="undefined"!=typeof b.bottom?b.bottom:f.bottom,f.left="undefined"!=typeof b.left?b.left:f.left,a):f},a.width=function(b){return arguments.length?(g=b,a):g},a.height=function(b){return arguments.length?(h=b,a):h},a.xTickFormat=function(b){return arguments.length?(k=b,a):k},a.yTickFormat=function(b){return arguments.length?(l=b,a):l},a.showValue=function(b){return arguments.length?(m=b,a):m},a.alignValue=function(b){return arguments.length?(n=b,a):n},a.rightAlignValue=function(b){return arguments.length?(o=b,a):o},a.noData=function(b){return arguments.length?(p=b,a):p},a},c.models.stackedArea=function(){"use strict";function a(c){return c.each(function(c){var l=f-e.left-e.right,s=g-e.top-e.bottom,t=d3.select(this);b=q.xScale(),d=q.yScale();var u=c;c.forEach(function(a,b){a.seriesIndex=b,a.values=a.values.map(function(a,c){return a.index=c,a.seriesIndex=b,a})});var v=c.filter(function(a){return!a.disabled});c=d3.layout.stack().order(n).offset(m).values(function(a){return a.values}).x(j).y(k).out(function(a,b,c){var d=0===k(a)?0:c;a.display={y:d,y0:b}})(v);var w=t.selectAll("g.nv-wrap.nv-stackedarea").data([c]),x=w.enter().append("g").attr("class","nvd3 nv-wrap nv-stackedarea"),y=x.append("defs"),z=x.append("g"),A=w.select("g");z.append("g").attr("class","nv-areaWrap"),z.append("g").attr("class","nv-scatterWrap"),w.attr("transform","translate("+e.left+","+e.top+")"),q.width(l).height(s).x(j).y(function(a){return a.display.y+a.display.y0}).forceY([0]).color(c.map(function(a){return a.color||h(a,a.seriesIndex)}));var B=A.select(".nv-scatterWrap").datum(c);B.call(q),y.append("clipPath").attr("id","nv-edge-clip-"+i).append("rect"),w.select("#nv-edge-clip-"+i+" rect").attr("width",l).attr("height",s),A.attr("clip-path",p?"url(#nv-edge-clip-"+i+")":"");var C=d3.svg.area().x(function(a,c){return b(j(a,c))}).y0(function(a){return d(a.display.y0)}).y1(function(a){return d(a.display.y+a.display.y0)}).interpolate(o),D=d3.svg.area().x(function(a,c){return b(j(a,c))}).y0(function(a){return d(a.display.y0)}).y1(function(a){return d(a.display.y0)}),E=A.select(".nv-areaWrap").selectAll("path.nv-area").data(function(a){return a});E.enter().append("path").attr("class",function(a,b){return"nv-area nv-area-"+b}).attr("d",function(a){return D(a.values,a.seriesIndex)}).on("mouseover",function(a){d3.select(this).classed("hover",!0),r.areaMouseover({point:a,series:a.key,pos:[d3.event.pageX,d3.event.pageY],seriesIndex:a.seriesIndex})}).on("mouseout",function(a){d3.select(this).classed("hover",!1),r.areaMouseout({point:a,series:a.key,pos:[d3.event.pageX,d3.event.pageY],seriesIndex:a.seriesIndex})}).on("click",function(a){d3.select(this).classed("hover",!1),r.areaClick({point:a,series:a.key,pos:[d3.event.pageX,d3.event.pageY],seriesIndex:a.seriesIndex})}),E.exit().remove(),E.style("fill",function(a){return a.color||h(a,a.seriesIndex)}).style("stroke",function(a){return a.color||h(a,a.seriesIndex)}),E.transition().attr("d",function(a,b){return C(a.values,b)}),q.dispatch.on("elementMouseover.area",function(a){A.select(".nv-chart-"+i+" .nv-area-"+a.seriesIndex).classed("hover",!0)}),q.dispatch.on("elementMouseout.area",function(a){A.select(".nv-chart-"+i+" .nv-area-"+a.seriesIndex).classed("hover",!1)}),a.d3_stackedOffset_stackPercent=function(a){var b,c,d,e=a.length,f=a[0].length,g=1/e,h=[];for(c=0;f>c;++c){for(b=0,d=0;b<u.length;b++)d+=k(u[b].values[c]);if(d)for(b=0;e>b;b++)a[b][c][1]/=d;else for(b=0;e>b;b++)a[b][c][1]=g}for(c=0;f>c;++c)h[c]=0;return h}}),a}var b,d,e={top:0,right:0,bottom:0,left:0},f=960,g=500,h=c.utils.defaultColor(),i=Math.floor(1e5*Math.random()),j=function(a){return a.x},k=function(a){return a.y},l="stack",m="zero",n="default",o="linear",p=!1,q=c.models.scatter(),r=d3.dispatch("tooltipShow","tooltipHide","areaClick","areaMouseover","areaMouseout");return q.size(2.2).sizeDomain([2.2,2.2]),q.dispatch.on("elementClick.area",function(a){r.areaClick(a)}),q.dispatch.on("elementMouseover.tooltip",function(a){a.pos=[a.pos[0]+e.left,a.pos[1]+e.top],r.tooltipShow(a)}),q.dispatch.on("elementMouseout.tooltip",function(a){r.tooltipHide(a)}),a.dispatch=r,a.scatter=q,d3.rebind(a,q,"interactive","size","xScale","yScale","zScale","xDomain","yDomain","xRange","yRange","sizeDomain","forceX","forceY","forceSize","clipVoronoi","useVoronoi","clipRadius","highlightPoint","clearHighlights"),a.options=c.utils.optionsFunc.bind(a),a.x=function(b){return arguments.length?(j=d3.functor(b),a):j},a.y=function(b){return arguments.length?(k=d3.functor(b),a):k},a.margin=function(b){return arguments.length?(e.top="undefined"!=typeof b.top?b.top:e.top,e.right="undefined"!=typeof b.right?b.right:e.right,e.bottom="undefined"!=typeof b.bottom?b.bottom:e.bottom,e.left="undefined"!=typeof b.left?b.left:e.left,a):e},a.width=function(b){return arguments.length?(f=b,a):f},a.height=function(b){return arguments.length?(g=b,a):g},a.clipEdge=function(b){return arguments.length?(p=b,a):p},a.color=function(b){return arguments.length?(h=c.utils.getColor(b),a):h},a.offset=function(b){return arguments.length?(m=b,a):m},a.order=function(b){return arguments.length?(n=b,a):n},a.style=function(b){if(!arguments.length)return l;switch(l=b){case"stack":a.offset("zero"),a.order("default");break;case"stream":a.offset("wiggle"),a.order("inside-out");break;case"stream-center":a.offset("silhouette"),a.order("inside-out");break;case"expand":a.offset("expand"),a.order("default");break;case"stack_percent":a.offset(a.d3_stackedOffset_stackPercent),a.order("default")}return a},a.interpolate=function(b){return arguments.length?(o=b,a):o},a},c.models.stackedAreaChart=function(){"use strict";function a(v){return v.each(function(v){var G=d3.select(this),H=this,I=(l||parseInt(G.style("width"))||960)-k.left-k.right,J=(m||parseInt(G.style("height"))||400)-k.top-k.bottom;if(a.update=function(){G.transition().duration(E).call(a)},a.container=this,x.disabled=v.map(function(a){return!!a.disabled}),!y){var K;y={};for(K in x)y[K]=x[K]instanceof Array?x[K].slice(0):x[K]}if(!(v&&v.length&&v.filter(function(a){return a.values.length}).length)){var L=G.selectAll(".nv-noData").data([z]);return L.enter().append("text").attr("class","nvd3 nv-noData").attr("dy","-.7em").style("text-anchor","middle"),L.attr("x",k.left+I/2).attr("y",k.top+J/2).text(function(a){return a}),a}G.selectAll(".nv-noData").remove(),b=e.xScale(),d=e.yScale();var M=G.selectAll("g.nv-wrap.nv-stackedAreaChart").data([v]),N=M.enter().append("g").attr("class","nvd3 nv-wrap nv-stackedAreaChart").append("g"),O=M.select("g");if(N.append("rect").style("opacity",0),N.append("g").attr("class","nv-x nv-axis"),N.append("g").attr("class","nv-y nv-axis"),N.append("g").attr("class","nv-stackedWrap"),N.append("g").attr("class","nv-legendWrap"),N.append("g").attr("class","nv-controlsWrap"),N.append("g").attr("class","nv-interactive"),O.select("rect").attr("width",I).attr("height",J),p){var P=o?I-B:I;h.width(P),O.select(".nv-legendWrap").datum(v).call(h),k.top!=h.height()&&(k.top=h.height(),J=(m||parseInt(G.style("height"))||400)-k.top-k.bottom),O.select(".nv-legendWrap").attr("transform","translate("+(I-P)+","+-k.top+")")}if(o){var Q=[{key:D.stacked||"Stacked",metaKey:"Stacked",disabled:"stack"!=e.style(),style:"stack"},{key:D.stream||"Stream",metaKey:"Stream",disabled:"stream"!=e.style(),style:"stream"},{key:D.expanded||"Expanded",metaKey:"Expanded",disabled:"expand"!=e.style(),style:"expand"},{key:D.stack_percent||"Stack %",metaKey:"Stack_Percent",disabled:"stack_percent"!=e.style(),style:"stack_percent"}];B=260*(C.length/3),Q=Q.filter(function(a){return-1!==C.indexOf(a.metaKey)}),i.width(B).color(["#444","#444","#444"]),O.select(".nv-controlsWrap").datum(Q).call(i),k.top!=Math.max(i.height(),h.height())&&(k.top=Math.max(i.height(),h.height()),J=(m||parseInt(G.style("height"))||400)-k.top-k.bottom),O.select(".nv-controlsWrap").attr("transform","translate(0,"+-k.top+")")}M.attr("transform","translate("+k.left+","+k.top+")"),s&&O.select(".nv-y.nv-axis").attr("transform","translate("+I+",0)"),t&&(j.width(I).height(J).margin({left:k.left,top:k.top}).svgContainer(G).xScale(b),M.select(".nv-interactive").call(j)),e.width(I).height(J);var R=O.select(".nv-stackedWrap").datum(v);R.transition().call(e),q&&(f.scale(b).ticks(I/100).tickSize(-J,0),O.select(".nv-x.nv-axis").attr("transform","translate(0,"+J+")"),O.select(".nv-x.nv-axis").transition().duration(0).call(f)),r&&(g.scale(d).ticks("wiggle"==e.offset()?0:J/36).tickSize(-I,0).setTickFormat("expand"==e.style()||"stack_percent"==e.style()?d3.format("%"):w),O.select(".nv-y.nv-axis").transition().duration(0).call(g)),e.dispatch.on("areaClick.toggle",function(b){1===v.filter(function(a){return!a.disabled}).length?v.forEach(function(a){a.disabled=!1}):v.forEach(function(a,c){a.disabled=c!=b.seriesIndex}),x.disabled=v.map(function(a){return!!a.disabled}),A.stateChange(x),a.update()}),h.dispatch.on("stateChange",function(b){x.disabled=b.disabled,A.stateChange(x),a.update()}),i.dispatch.on("legendClick",function(b){b.disabled&&(Q=Q.map(function(a){return a.disabled=!0,a}),b.disabled=!1,e.style(b.style),x.style=e.style(),A.stateChange(x),a.update())}),j.dispatch.on("elementMousemove",function(b){e.clearHighlights();var d,h,i,l=[];if(v.filter(function(a,b){return a.seriesIndex=b,!a.disabled}).forEach(function(f,g){h=c.interactiveBisect(f.values,b.pointXValue,a.x()),e.highlightPoint(g,h,!0);var j=f.values[h];if("undefined"!=typeof j){"undefined"==typeof d&&(d=j),"undefined"==typeof i&&(i=a.xScale()(a.x()(j,h)));var k="expand"==e.style()?j.display.y:a.y()(j,h);l.push({key:f.key,value:k,color:n(f,f.seriesIndex),stackedValue:j.display})}}),l.reverse(),l.length>2){var m=a.yScale().invert(b.mouseY),o=null;l.forEach(function(a,b){m=Math.abs(m);var c=Math.abs(a.stackedValue.y0),d=Math.abs(a.stackedValue.y);return m>=c&&d+c>=m?(o=b,void 0):void 0}),null!=o&&(l[o].highlight=!0)}var p=f.tickFormat()(a.x()(d,h)),q="expand"==e.style()?function(a){return d3.format(".1%")(a)}:function(a){return g.tickFormat()(a)};j.tooltip.position({left:i+k.left,top:b.mouseY+k.top}).chartContainer(H.parentNode).enabled(u).valueFormatter(q).data({value:p,series:l})(),j.renderGuideLine(i)}),j.dispatch.on("elementMouseout",function(){A.tooltipHide(),e.clearHighlights()}),A.on("tooltipShow",function(a){u&&F(a,H.parentNode)}),A.on("changeState",function(b){"undefined"!=typeof b.disabled&&(v.forEach(function(a,c){a.disabled=b.disabled[c]}),x.disabled=b.disabled),"undefined"!=typeof b.style&&e.style(b.style),a.update()})}),a}var b,d,e=c.models.stackedArea(),f=c.models.axis(),g=c.models.axis(),h=c.models.legend(),i=c.models.legend(),j=c.interactiveGuideline(),k={top:30,right:25,bottom:50,left:60},l=null,m=null,n=c.utils.defaultColor(),o=!0,p=!0,q=!0,r=!0,s=!1,t=!1,u=!0,v=function(a,b,c){return"<h3>"+a+"</h3>"+"<p>"+c+" on "+b+"</p>"},w=d3.format(",.2f"),x={style:e.style()},y=null,z="No Data Available.",A=d3.dispatch("tooltipShow","tooltipHide","stateChange","changeState"),B=250,C=["Stacked","Stream","Expanded"],D={},E=250;f.orient("bottom").tickPadding(7),g.orient(s?"right":"left"),i.updateState(!1);var F=function(b,d){var h=b.pos[0]+(d.offsetLeft||0),i=b.pos[1]+(d.offsetTop||0),j=f.tickFormat()(e.x()(b.point,b.pointIndex)),k=g.tickFormat()(e.y()(b.point,b.pointIndex)),l=v(b.series.key,j,k,b,a);c.tooltip.show([h,i],l,b.value<0?"n":"s",null,d)};return e.dispatch.on("tooltipShow",function(a){a.pos=[a.pos[0]+k.left,a.pos[1]+k.top],A.tooltipShow(a)}),e.dispatch.on("tooltipHide",function(a){A.tooltipHide(a)}),A.on("tooltipHide",function(){u&&c.tooltip.cleanup()}),a.dispatch=A,a.stacked=e,a.legend=h,a.controls=i,a.xAxis=f,a.yAxis=g,a.interactiveLayer=j,d3.rebind(a,e,"x","y","size","xScale","yScale","xDomain","yDomain","xRange","yRange","sizeDomain","interactive","useVoronoi","offset","order","style","clipEdge","forceX","forceY","forceSize","interpolate"),a.options=c.utils.optionsFunc.bind(a),a.margin=function(b){return arguments.length?(k.top="undefined"!=typeof b.top?b.top:k.top,k.right="undefined"!=typeof b.right?b.right:k.right,k.bottom="undefined"!=typeof b.bottom?b.bottom:k.bottom,k.left="undefined"!=typeof b.left?b.left:k.left,a):k},a.width=function(b){return arguments.length?(l=b,a):l},a.height=function(b){return arguments.length?(m=b,a):m},a.color=function(b){return arguments.length?(n=c.utils.getColor(b),h.color(n),e.color(n),a):n},a.showControls=function(b){return arguments.length?(o=b,a):o},a.showLegend=function(b){return arguments.length?(p=b,a):p},a.showXAxis=function(b){return arguments.length?(q=b,a):q},a.showYAxis=function(b){return arguments.length?(r=b,a):r},a.rightAlignYAxis=function(b){return arguments.length?(s=b,g.orient(b?"right":"left"),a):s},a.useInteractiveGuideline=function(b){return arguments.length?(t=b,b===!0&&(a.interactive(!1),a.useVoronoi(!1)),a):t},a.tooltip=function(b){return arguments.length?(v=b,a):v},a.tooltips=function(b){return arguments.length?(u=b,a):u},a.tooltipContent=function(b){return arguments.length?(v=b,a):v},a.state=function(b){return arguments.length?(x=b,a):x},a.defaultState=function(b){return arguments.length?(y=b,a):y},a.noData=function(b){return arguments.length?(z=b,a):z},a.transitionDuration=function(b){return arguments.length?(E=b,a):E},a.controlsData=function(b){return arguments.length?(C=b,a):C},a.controlLabels=function(b){return arguments.length?"object"!=typeof b?D:(D=b,a):D},g.setTickFormat=g.tickFormat,g.tickFormat=function(a){return arguments.length?(w=a,g):w},a}}();
/*jshint unused: false */
/*global window, $, document */

(function() {
  "use strict";
  var isCoordinator;

  window.isCoordinator = function() {
    if (isCoordinator === undefined) {
      $.ajax(
        "cluster/amICoordinator",
        {
          async: false,
          success: function(d) {
            isCoordinator = d;
          }
        }
      );
    }
    return isCoordinator;
  };

  window.versionHelper = {
    fromString: function (s) {
      var parts = s.replace(/-[a-zA-Z0-9_\-]*$/g, '').split('.');
      return {
        major: parseInt(parts[0], 10) || 0,
        minor: parseInt(parts[1], 10) || 0,
        patch: parseInt(parts[2], 10) || 0,
        toString: function() {
          return this.major + "." + this.minor + "." + this.patch;
        }
      };
    },
    toString: function (v) {
      return v.major + '.' + v.minor + '.' + v.patch;
    }
  };

  window.arangoHelper = {
    lastNotificationMessage: null,

    CollectionTypes: {},
    systemAttributes: function () {
      return {
        '_id' : true,
        '_rev' : true,
        '_key' : true,
        '_bidirectional' : true,
        '_vertices' : true,
        '_from' : true,
        '_to' : true,
        '$id' : true
      };
    },

    setCheckboxStatus: function(id) {
      $.each($(id).find('ul').find('li'), function(key, element) {
         if (!$(element).hasClass("nav-header")) {
           if ($(element).find('input').attr('checked')) {
             if ($(element).find('i').hasClass('css-round-label')) {
               $(element).find('i').addClass('fa-dot-circle-o');
             }
             else {
               $(element).find('i').addClass('fa-check-square-o');
             }
           }
           else {
             if ($(element).find('i').hasClass('css-round-label')) {
               $(element).find('i').addClass('fa-circle-o');
             }
             else {
               $(element).find('i').addClass('fa-square-o');
             }
           }
         }
      });
    },

    calculateCenterDivHeight: function() {
      var navigation = $('.navbar').height();
      var footer = $('.footer').height();
      var windowHeight = $(window).height();

      return windowHeight - footer - navigation - 110;
    },

    fixTooltips: function (selector, placement) {
      $(selector).tooltip({
        placement: placement,
        hide: false,
        show: false
      });
    },

    currentDatabase: function () {
      var returnVal = false;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/database/current",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          returnVal = data.result.name;
        },
        error: function() {
          returnVal = false;
        }
      });
      return returnVal;
    },

    allHotkeys: {
      global: {
        name: "Site wide",
        content: [{
          label: "scroll up",
          letter: "j"
        },{
          label: "scroll down",
          letter: "k"
        }]
      },
      jsoneditor: {
        name: "AQL editor",
        content: [{
          label: "Submit",
          letter: "Ctrl + Return"
        },{
          label: "Toggle comments",
          letter: "Ctrl + Shift + C"
        },{
          label: "Undo",
          letter: "Ctrl + Z"
        },{
          label: "Redo",
          letter: "Ctrl + Shift + Z"
        }]
      },
      doceditor: {
        name: "Document editor",
        content: [{
          label: "Insert",
          letter: "Ctrl + Insert"
        },{
          label: "Save",
          letter: "Ctrl + Return, CMD + Return"
        },{
          label: "Append",
          letter: "Ctrl + Shift + Insert"
        },{
          label: "Duplicate",
          letter: "Ctrl + D"
        },{
          label: "Remove",
          letter: "Ctrl + Delete"
        }]
      },
      modals: {
        name: "Modal",
        content: [{
          label: "Submit",
          letter: "Return"
        },{
          label: "Close",
          letter: "Esc"
        },{
          label: "Navigate buttons",
          letter: "Arrow keys"
        },{
          label: "Navigate content",
          letter: "Tab"
        }]
      }
    },

    hotkeysFunctions: {
      scrollDown: function () {
        window.scrollBy(0,180);
      },
      scrollUp: function () {
        window.scrollBy(0,-180);
      },
      showHotkeysModal: function () {
        var buttons = [],
        content = window.arangoHelper.allHotkeys;

        window.modalView.show("modalHotkeys.ejs", "Keyboard Shortcuts", buttons, content);
      }
    },

    enableKeyboardHotkeys: function (enable) {
      var hotkeys = window.arangoHelper.hotkeysFunctions;
      if (enable === true) {
        $(document).on('keydown', null, 'j', hotkeys.scrollDown);
        $(document).on('keydown', null, 'k', hotkeys.scrollUp);
      }
    },

    databaseAllowed: function () {
      var currentDB = this.currentDatabase(),
      returnVal = false;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_db/"+ encodeURIComponent(currentDB) + "/_api/database/",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function() {
          returnVal = true;
        },
        error: function() {
          returnVal = false;
        }
      });
      return returnVal;
    },

    arangoNotification: function (title, content, info) {
      window.App.notificationList.add({title:title, content: content, info: info});
    },

    arangoError: function (title, content, info) {
      window.App.notificationList.add({title:title, content: content, info: info});
    },

    getRandomToken: function () {
      return Math.round(new Date().getTime());
    },

    isSystemAttribute: function (val) {
      var a = this.systemAttributes();
      return a[val];
    },

    isSystemCollection: function (val) {
      return val.name.substr(0, 1) === '_';
      // the below code is completely inappropriate as it will
      // load the collection just for the check whether it
      // is a system collection. as a consequence, the below
      // code would load ALL collections when the web interface
      // is called
      /*
         var returnVal = false;
         $.ajax({
type: "GET",
url: "/_api/collection/" + encodeURIComponent(val) + "/properties",
contentType: "application/json",
processData: false,
async: false,
success: function(data) {
returnVal = data.isSystem;
},
error: function(data) {
returnVal = false;
}
});
return returnVal;
*/
    },

    setDocumentStore : function (a) {
      this.arangoDocumentStore = a;
    },

    collectionApiType: function (identifier, refresh) {
      // set "refresh" to disable caching collection type
      if (refresh || this.CollectionTypes[identifier] === undefined) {
        this.CollectionTypes[identifier] = this.arangoDocumentStore
        .getCollectionInfo(identifier).type;
      }
      if (this.CollectionTypes[identifier] === 3) {
        return "edge";
      }
      return "document";
    },

    collectionType: function (val) {
      if (! val || val.name === '') {
        return "-";
      }
      var type;
      if (val.type === 2) {
        type = "document";
      }
      else if (val.type === 3) {
        type = "edge";
      }
      else {
        type = "unknown";
      }

      if (this.isSystemCollection(val)) {
        type += " (system)";
      }

      return type;
    },

    formatDT: function (dt) {
      var pad = function (n) {
        return n < 10 ? '0' + n : n;
      };

      return dt.getUTCFullYear() + '-'
      + pad(dt.getUTCMonth() + 1) + '-'
      + pad(dt.getUTCDate()) + ' '
      + pad(dt.getUTCHours()) + ':'
      + pad(dt.getUTCMinutes()) + ':'
      + pad(dt.getUTCSeconds());
    },

    escapeHtml: function (val) {
      // HTML-escape a string
      return String(val).replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
    }

  };
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterCollection = Backbone.Model.extend({
    defaults: {
      "name": "",
      "status": "ok"
    },

    idAttribute: "name",

    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status")
      };
    }
  });
}());


/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterCoordinator = Backbone.Model.extend({

    defaults: {
      "name": "",
      "url": "",
      "status": "ok"
    },

    idAttribute: "name",
    /*
    url: "/_admin/aardvark/cluster/Coordinators";

    updateUrl: function() {
      this.url = window.getNewRoute("Coordinators");
    },
    */
    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status"),
        url: this.get("url")
      };
    }

  });
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterDatabase = Backbone.Model.extend({

    defaults: {
      "name": "",
      "status": "ok"
    },

    idAttribute: "name",

    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status")
      };
    }
    /*
    url: "/_admin/aardvark/cluster/Databases";

    updateUrl: function() {
      this.url = window.getNewRoute("Databases");
    }
    */

  });
}());

/*global window, Backbone, $, _*/
(function() {
  "use strict";

  window.ClusterPlan = Backbone.Model.extend({

    defaults: {
    },

    url: "cluster/plan",

    idAttribute: "config",

    getVersion: function() {
      var v = this.get("version");
      return v || "2.0";
    },

    getCoordinator: function() {
      if (this._coord) {
        return this._coord[
          this._lastStableCoord
        ];
      }
      var tmpList = [];
      var i,j,r,l;
      r = this.get("runInfo");
      if (!r) {
        return;
      }
      j = r.length-1;
      while (j > 0) {
        if(r[j].isStartServers) {
          l = r[j];
          if (l.endpoints) {
            for (i = 0; i < l.endpoints.length;i++) {
              if (l.roles[i] === "Coordinator") {
                tmpList.push(l.endpoints[i]
                  .replace("tcp://","http://")
                  .replace("ssl://", "https://")
                );
              }
            }
          }
        }
        j--;
      }
      this._coord = tmpList;
      this._lastStableCoord = Math.floor(Math.random() * this._coord.length);
    },

    rotateCoordinator: function() {
      var last = this._lastStableCoord, next;
      if (this._coord.length > 1) {
        do {
          next = Math.floor(Math.random() * this._coord.length);
        } while (next === last);
        this._lastStableCoord = next;
      }
    },

    isAlive : function() {
      var result = false;
      $.ajax({
        cache: false,
        type: "GET",
        async: false, // sequential calls!
        url: "cluster/healthcheck",
        success: function(data) {
          result = data;
        },
        error: function(data) {
        }
      });
      return result;
    },

    storeCredentials: function(name, passwd) {
      var self = this;
      $.ajax({
        url: "cluster/plan/credentials",
        type: "PUT",
        data: JSON.stringify({
          user: name,
          passwd: passwd
        }),
        async: false
      }).done(function() {
        self.fetch();
      });
    },

    isSymmetricSetup: function() {
      var config = this.get("config");
      var count = _.size(config.dispatchers);
      return count === config.numberOfCoordinators
        && count === config.numberOfDBservers;
    },

    isTestSetup: function() {
      return _.size(this.get("config").dispatchers) === 1;
    },

    cleanUp: function() {
      $.ajax({
        url: "cluster/plan/cleanUp",
        type: "DELETE",
        async: false
      });
    }

  });
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterServer = Backbone.Model.extend({
    defaults: {
      name: "",
      address: "",
      role: "",
      status: "ok"
    },

    idAttribute: "name",
    /*
    url: "/_admin/aardvark/cluster/DBServers";

    updateUrl: function() {
      this.url = window.getNewRoute("DBServers");
    },
    */
    forList: function() {
      return {
        name: this.get("name"),
        address: this.get("address"),
        status: this.get("status")
      };
    }

  });
}());


/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterShard = Backbone.Model.extend({
    defaults: {
    },

    idAttribute: "name",

    forList: function() {
      return {
        server: this.get("name"),
        shards: this.get("shards")
      };
    }
    /*
    url: "/_admin/aardvark/cluster/Shards";

    updateUrl: function() {
      this.url = window.getNewRoute("Shards");
    }
    */
  });
}());


/*global window, Backbone, $, _*/
(function() {

  "use strict";

  window.ClusterType = Backbone.Model.extend({

    defaults: {
      "type": "testPlan"
    }
  });
}());

/*global window, Backbone, console */
(function() {
  "use strict";

  window.AutomaticRetryCollection = Backbone.Collection.extend({

    _retryCount: 0,


    checkRetries: function() {
      var self = this;
      this.updateUrl();
      if (this._retryCount > 10) {
        window.setTimeout(function() {
          self._retryCount = 0;
        }, 10000);
        window.App.clusterUnreachable();
        return false;
      }
      return true;
    },

    successFullTry: function() {
      this._retryCount = 0;
    },

    failureTry: function(retry, ignore, err) {
      if (err.status === 401) {
        window.App.requestAuth();
      } else {
        window.App.clusterPlan.rotateCoordinator();
        this._retryCount++;
        retry();
      }
    }

  });
}());

/*global Backbone, window */
window.ClusterStatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,

  url: "/_admin/statistics",

  updateUrl: function() {
    this.url = window.App.getNewRoute("statistics");
  },

  initialize: function() {
    window.App.registerForUpdate(this);
  },

  // The callback has to be invokeable for each result individually
  fetch: function(callback, errCB) {
    this.forEach(function (m) {
      m.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: function() {
          errCB(m);
        }
      }).done(function() {
        callback(m);
      });
    });
  }
});

/*global window, Backbone */
(function() {
  "use strict";
  window.ClusterCollections = window.AutomaticRetryCollection.extend({
    model: window.ClusterCollection,

    updateUrl: function() {
      this.url = window.App.getNewRoute(this.dbname + "/Collections");
    },

    url: function() {
      return "/_admin/aardvark/cluster/"
        + this.dbname + "/"
        + "Collections";
    },

    initialize: function() {
      this.isUpdating = false;
      this.timer = null;
      this.interval = 1000;
      window.App.registerForUpdate(this);
    },

    getList: function(db, callback) {
      if (db === undefined) {
        return;
      }
      this.dbname = db;
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, db, callback))
      }).done(function() {
        callback(self.map(function(m) {
          return m.forList();
        }));
      });
    },

    stopUpdating: function () {
      window.clearTimeout(this.timer);
      this.isUpdating = false;
    },

    startUpdating: function () {
      if (this.isUpdating) {
        return;
      }
      this.isUpdating = true;
      var self = this;
      this.timer = window.setInterval(function() {
        self.updateUrl();
        self.fetch({
          beforeSend: window.App.addAuth.bind(window.App)
        });
      }, this.interval);
    }

  });
}());




/*global window, Backbone, console */
(function() {
  "use strict";
  window.ClusterCoordinators = window.AutomaticRetryCollection.extend({
    model: window.ClusterCoordinator,

    url: "/_admin/aardvark/cluster/Coordinators",

    updateUrl: function() {
      this.url = window.App.getNewRoute("Coordinators");
    },

    initialize: function() {
      window.App.registerForUpdate(this);
    },

    statusClass: function(s) {
      switch (s) {
        case "ok":
          return "success";
        case "warning":
          return "warning";
        case "critical":
          return "danger";
        case "missing":
          return "inactive";
        default:
          return "danger";
      }
    },

    getStatuses: function(cb, nextStep) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb, nextStep))
      }).done(function() {
        self.successFullTry();
        self.forEach(function(m) {
          cb(self.statusClass(m.get("status")), m.get("address"));
        });
        nextStep();
      });
    },

    byAddress: function (res, callback) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.byAddress.bind(self, res, callback))
      }).done(function() {
        self.successFullTry();
        res = res || {};
        self.forEach(function(m) {
          var addr = m.get("address");
          addr = addr.split(":")[0];
          res[addr] = res[addr] || {};
          res[addr].coords = res[addr].coords || [];
          res[addr].coords.push(m);
        });
        callback(res);
      });
    },

    checkConnection: function(callback) {
      var self = this;
      if(!this.checkRetries()) {
        return;
      }
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.checkConnection.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        callback();
      });
    },

    getList: function() {
      throw "Do not use coordinator.getList";
      /*
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      return this.map(function(m) {
        return m.forList();
      });
      */
    },

    getOverview: function() {
      throw "Do not use coordinator.getOverview";
      /*
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      var res = {
        plan: 0,
        having: 0,
        status: "ok"
      },
      updateStatus = function(to) {
        if (res.status === "critical") {
          return;
        }
        res.status = to;
      };
      this.each(function(m) {
        res.plan++;
        switch (m.get("status")) {
          case "ok":
            res.having++;
            break;
          case "warning":
            res.having++;
            updateStatus("warning");
            break;
          case "critical":
            updateStatus("critical");
            break;
          default:
            console.debug("Undefined server state occurred. This is still in development");
        }
      });
      return res;
      */
    }
  });
}());



/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterDatabases = window.AutomaticRetryCollection.extend({

    model: window.ClusterDatabase,

    url: "/_admin/aardvark/cluster/Databases",

    updateUrl: function() {
      this.url = window.App.getNewRoute("Databases");
    },

    initialize: function() {
      window.App.registerForUpdate(this);
    },

    getList: function(callback) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        callback(self.map(function(m) {
          return m.forList();
        }));
      });
    }

  });
}());

/*global window, Backbone, _, console */
(function() {

  "use strict";

  window.ClusterServers = window.AutomaticRetryCollection.extend({

    model: window.ClusterServer,

    url: "/_admin/aardvark/cluster/DBServers",

    updateUrl: function() {
      this.url = window.App.getNewRoute("DBServers");
    },

    initialize: function() {
      window.App.registerForUpdate(this);
    },

    statusClass: function(s) {
      switch (s) {
        case "ok":
          return "success";
        case "warning":
          return "warning";
        case "critical":
          return "danger";
        case "missing":
          return "inactive";
        default:
          return "danger";
      }
    },

    getStatuses: function(cb) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this,
        completed = function() {
          self.successFullTry();
          self._retryCount = 0;
          self.forEach(function(m) {
            cb(self.statusClass(m.get("status")), m.get("address"));
          });
        };
      // This is the first function called in
      // Each update loop
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb))
      }).done(completed);
    },

    byAddress: function (res, callback) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.byAddress.bind(self, res, callback))
      }).done(function() {
        self.successFullTry();
        res = res || {};
        self.forEach(function(m) {
          var addr = m.get("address");
          addr = addr.split(":")[0];
          res[addr] = res[addr] || {};
          res[addr].dbs = res[addr].dbs || [];
          res[addr].dbs.push(m);
        });
        callback(res);
      });
    },

    getList: function(callback) {
      throw "Do not use";
      /*
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        var res = [];
        _.each(self.where({role: "primary"}), function(m) {
          var e = {};
          e.primary = m.forList();
          if (m.get("secondary")) {
            e.secondary = self.get(m.get("secondary")).forList();
          }
          res.push(e);
        });
        callback(res);
      });
      */
    },

    getOverview: function() {
      throw "Do not use DbServer.getOverview";
      /*
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      var res = {
        plan: 0,
        having: 0,
        status: "ok"
      },
      self = this,
      updateStatus = function(to) {
        if (res.status === "critical") {
          return;
        }
        res.status = to;
      };
      _.each(this.where({role: "primary"}), function(m) {
        res.plan++;
        switch (m.get("status")) {
          case "ok":
            res.having++;
            break;
          case "warning":
            res.having++;
            updateStatus("warning");
            break;
          case "critical":
            var bkp = self.get(m.get("secondary"));
            if (!bkp || bkp.get("status") === "critical") {
              updateStatus("critical");
            } else {
              if (bkp.get("status") === "ok") {
                res.having++;
                updateStatus("warning");
              }
            }
            break;
          default:
            console.debug("Undefined server state occurred. This is still in development");
        }
      });
      return res;
      */
    }
  });

}());


/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterShards = window.AutomaticRetryCollection.extend({

    model: window.ClusterShard,

    updateUrl: function() {
      this.url = window.App.getNewRoute(
        this.dbname + "/" + this.colname + "/Shards"
      );
    },

    url: function() {
      return "/_admin/aardvark/cluster/"
        + this.dbname + "/"
        + this.colname + "/"
        + "Shards";
    },

    initialize: function() {
      this.isUpdating = false;
      this.timer = null;
      this.interval = 1000;
      window.App.registerForUpdate(this);
    },

    getList: function(dbname, colname, callback) {
      if (dbname === undefined || colname === undefined) {
        return;
      }
      this.dbname = dbname;
      this.colname = colname;
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(
          self, dbname, colname, callback)
        )
      }).done(function() {
        callback(self.map(function(m) {
          return m.forList();
        }));
      });
    },

    stopUpdating: function () {
      window.clearTimeout(this.timer);
      this.isUpdating = false;
    },

    startUpdating: function () {
      if (this.isUpdating) {
        return;
      }
      this.isUpdating = true;
      var self = this;
      this.timer = window.setInterval(function() {
        self.updateUrl();
        self.fetch({
          beforeSend: window.App.addAuth.bind(window.App)
        });
      }, this.interval);
    }

  });
}());




/*global window, Backbone, arangoHelper, _ */

window.arangoDocumentModel = Backbone.Model.extend({
  initialize: function () {
    'use strict';
  },
  urlRoot: "/_api/document",
  defaults: {
    _id: "",
    _rev: "",
    _key: ""
  },
  getSorted: function () {
    'use strict';
    var self = this;
    var keys = Object.keys(self.attributes).sort(function (l, r) {
      var l1 = arangoHelper.isSystemAttribute(l);
      var r1 = arangoHelper.isSystemAttribute(r);

      if (l1 !== r1) {
        if (l1) {
          return -1;
        }
        return 1;
      }

      return l < r ? -1 : 1;
    });

    var sorted = {};
    _.each(keys, function (k) {
      sorted[k] = self.attributes[k];
    });
    return sorted;
  }
});

/*global window, Backbone */

window.Statistics = Backbone.Model.extend({
  defaults: {
  },

  url: function() {
    'use strict';
    return "/_admin/statistics";
  }
});

/*global window, Backbone */

window.StatisticsDescription = Backbone.Model.extend({
  defaults: {
    "figures" : "",
    "groups" : ""
  },
  url: function() {
    'use strict';

    return "/_admin/statistics-description";
  }

});

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, window, $, _ */
(function () {

  "use strict";

  window.PaginatedCollection = Backbone.Collection.extend({
    page: 0,
    pagesize: 10,
    totalAmount: 0,

    getPage: function() {
      return this.page + 1;
    },

    setPage: function(counter) {
      if (counter >= this.getLastPageNumber()) {
        this.page = this.getLastPageNumber()-1;
        return;
      }
      if (counter < 1) {
        this.page = 0;
        return;
      }
      this.page = counter - 1;

    },

    getLastPageNumber: function() {
      return Math.max(Math.ceil(this.totalAmount / this.pagesize), 1);
    },

    getOffset: function() {
      return this.page * this.pagesize;
    },

    getPageSize: function() {
      return this.pagesize;
    },

    setPageSize: function(newPagesize) {
      if (newPagesize === "all") {
        this.pagesize = 'all';
      }
      else {
        try {
          newPagesize = parseInt(newPagesize, 10);
          this.pagesize = newPagesize;
        }
        catch (ignore) {
        }
      }
    },

    setToFirst: function() {
      this.page = 0;
    },

    setToLast: function() {
      this.setPage(this.getLastPageNumber());
    },

    setToPrev: function() {
      this.setPage(this.getPage() - 1);

    },

    setToNext: function() {
      this.setPage(this.getPage() + 1);
    },

    setTotal: function(total) {
      this.totalAmount = total;
    },

    getTotal: function() {
      return this.totalAmount;
    },

    setTotalMinusOne: function() {
      this.totalAmount--;
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, window */
window.StatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,
  url: "/_admin/statistics"
});

/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, arangoDocumentModel, _, arangoHelper, $*/
(function() {
  "use strict";

  window.arangoDocuments = window.PaginatedCollection.extend({
    collectionID: 1,

    filters: [],

    MAX_SORT: 12000,

    lastQuery: {},

    sortAttribute: "_key",

    url: '/_api/documents',
    model: window.arangoDocumentModel,

    loadTotal: function() {
      var self = this;
      $.ajax({
        cache: false,
        type: "GET",
        url: "/_api/collection/" + this.collectionID + "/count",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          self.setTotal(data.count);
        }
      });
    },

    setCollection: function(id) {
      this.resetFilter();
      this.collectionID = id;
      this.setPage(1);
      this.loadTotal();
    },

    setSort: function(key) {
      this.sortAttribute = key;
    },

    getSort: function() {
      return this.sortAttribute;
    },

    addFilter: function(attr, op, val) {
      this.filters.push({
        attr: attr,
        op: op,
        val: val
      });
    },

    setFiltersForQuery: function(bindVars) {
      if (this.filters.length === 0) {
        return "";
      }
      var query = " FILTER",
      parts = _.map(this.filters, function(f, i) {
        var res = " x.`";
        res += f.attr;
        res += "` ";
        res += f.op;
        res += " @param";
        res += i;
        bindVars["param" + i] = f.val;
        return res;
      });
      return query + parts.join(" &&");
    },

    setPagesize: function(size) {
      this.setPageSize(size);
    },

    resetFilter: function() {
      this.filters = [];
    },

    moveDocument: function (key, fromCollection, toCollection, callback) {
      var querySave, queryRemove, queryObj, bindVars = {
        "@collection": fromCollection,
        "filterid": key
      }, queryObj1, queryObj2;

      querySave = "FOR x IN @@collection";
      querySave += " FILTER x._key == @filterid";
      querySave += " INSERT x IN ";
      querySave += toCollection;

      queryRemove = "FOR x in @@collection";
      queryRemove += " FILTER x._key == @filterid";
      queryRemove += " REMOVE x IN @@collection";

      queryObj1 = {
        query: querySave,
        bindVars: bindVars
      };

      queryObj2 = {
        query: queryRemove,
        bindVars: bindVars
      };

      window.progressView.show();
      // first insert docs in toCollection
      $.ajax({
        cache: false,
        type: 'POST',
        async: true,
        url: '/_api/cursor',
        data: JSON.stringify(queryObj1),
        contentType: "application/json",
        success: function(data) {
          // if successful remove unwanted docs
          $.ajax({
            cache: false,
            type: 'POST',
            async: true,
            url: '/_api/cursor',
            data: JSON.stringify(queryObj2),
            contentType: "application/json",
            success: function(data) {
              if (callback) {
                callback();
              }
              window.progressView.hide();
            },
            error: function(data) {
              window.progressView.hide();
              arangoHelper.arangoNotification(
                "Document error", "Documents inserted, but could not be removed."
              );
            }
          });
        },
        error: function(data) {
          window.progressView.hide();
          arangoHelper.arangoNotification("Document error", "Could not move selected documents.");
        }
      });
    },

    getDocuments: function (callback) {
      window.progressView.showWithDelay(300, "Fetching documents...");
      var self = this,
          query,
          bindVars,
          tmp,
          queryObj;
      bindVars = {
        "@collection": this.collectionID,
        "offset": this.getOffset(),
        "count": this.getPageSize()
      };

      // fetch just the first 25 attributes of the document
      // this number is arbitrary, but may reduce HTTP traffic a bit
      query = "FOR x IN @@collection LET att = SLICE(ATTRIBUTES(x), 0, 25)";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT) {
        if (this.getSort() === '_key') {
          query += " SORT TO_NUMBER(x." + this.getSort() + ") == 0 ? x."
                + this.getSort() + " : TO_NUMBER(x." + this.getSort() + ")";
        }
        else {
          query += " SORT x." + this.getSort();
        }
      }

      if (bindVars.count !== 'all') {
        query += " LIMIT @offset, @count RETURN KEEP(x, att)";
      }
      else {
        tmp = {
          "@collection": this.collectionID
        };
        bindVars = tmp;
        query += " RETURN KEEP(x, att)";
      }

      queryObj = {
        query: query,
        bindVars: bindVars
      };
      if (this.getTotal() < 10000 || this.filters.length > 0) {
        queryObj.options = {
          fullCount: true,
        };
      }

      $.ajax({
        cache: false,
        type: 'POST',
        async: true,
        url: '/_api/cursor',
        data: JSON.stringify(queryObj),
        contentType: "application/json",
        success: function(data) {
          window.progressView.toShow = false;
          self.clearDocuments();
          if (data.extra && data.extra.stats.fullCount !== undefined) {
            self.setTotal(data.extra.stats.fullCount);
          }
          if (self.getTotal() !== 0) {
            _.each(data.result, function(v) {
              self.add({
                "id": v._id,
                "rev": v._rev,
                "key": v._key,
                "content": v
              });
            });
          }
          self.lastQuery = queryObj;
          callback();
          window.progressView.hide();
        },
        error: function(data) {
          window.progressView.hide();
          arangoHelper.arangoNotification("Document error", "Could not fetch requested documents.");
        }
      });
    },

    clearDocuments: function () {
      this.reset();
    },

    buildDownloadDocumentQuery: function() {
      var self = this, query, queryObj, bindVars;

      bindVars = {
        "@collection": this.collectionID
      };

      query = "FOR x in @@collection";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT) {
        query += " SORT x." + this.getSort();
      }

      query += " RETURN x";

      queryObj = {
        query: query,
        bindVars: bindVars
      };

      return queryObj;
    },

    uploadDocuments : function (file) {
      var result;
      $.ajax({
        type: "POST",
        async: false,
        url:
        '/_api/import?type=auto&collection='+
        encodeURIComponent(this.collectionID)+
        '&createCollection=false',
        data: file,
        processData: false,
        contentType: 'json',
        dataType: 'json',
        complete: function(xhr) {
          if (xhr.readyState === 4 && xhr.status === 201) {
            result = true;
          } else {
            result = "Upload error";
          }

          try {
            var data = JSON.parse(xhr.responseText);
            if (data.errors > 0) {
              result = "At least one error occurred during upload";
            }
          }
          catch (err) {
          }               
        }
      });
      return result;
    }
  });
}());

/*jshint unused: false */
/*global EJS, window, _, $*/
(function() {
  "use strict";
  // For tests the templates are loaded some where else.
  // We need to use a different engine there.
  if (!window.hasOwnProperty("TEST_BUILD")) {
    var TemplateEngine = function() {
      var exports = {};
      exports.createTemplate = function(id) {
        var template = $("#" + id.replace(".", "\\.")).html();
        return {
          render: function(params) {
            return _.template(template, params);
          }
        };
      };
      return exports;
    };
    window.templateEngine = new TemplateEngine();
  }
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, arangoHelper, window*/

(function() {
  "use strict";
  window.FooterView = Backbone.View.extend({
    el: '#footerBar',
    system: {},
    isOffline: true,
    isOfflineCounter: 0,
    firstLogin: true,

    events: {
      'click .footer-center p' : 'showShortcutModal'
    },

    initialize: function () {
      //also server online check
      var self = this;
      window.setInterval(function(){
        self.getVersion();
      }, 15000);
      self.getVersion();
    },

    template: templateEngine.createTemplate("footerView.ejs"),

    showServerStatus: function(isOnline) {
      if (isOnline === true) {
        $('.serverStatusIndicator').addClass('isOnline');
        $('.serverStatusIndicator').addClass('fa-check-circle-o');
        $('.serverStatusIndicator').removeClass('fa-times-circle-o');
      }
      else {
        $('.serverStatusIndicator').removeClass('isOnline');
        $('.serverStatusIndicator').removeClass('fa-check-circle-o');
        $('.serverStatusIndicator').addClass('fa-times-circle-o');
      }
    },

    showShortcutModal: function() {
      window.arangoHelper.hotkeysFunctions.showHotkeysModal();
    },

    getVersion: function () {
      var self = this;

      // always retry this call, because it also checks if the server is online
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/version",
        contentType: "application/json",
        processData: false,
        async: true,
        success: function(data) {
          self.showServerStatus(true);
          if (self.isOffline === true) {
            self.isOffline = false;
            self.isOfflineCounter = 0;
            if (!self.firstLogin) {
              window.setTimeout(function(){
                self.showServerStatus(true);
              }, 1000);
            } else {
              self.firstLogin = false;
            }
            self.system.name = data.server;
            self.system.version = data.version;
            self.render();
          }
        },
        error: function (data) {
          self.isOffline = true;
          self.isOfflineCounter++;
          if (self.isOfflineCounter >= 1) {
            //arangoHelper.arangoError("Server", "Server is offline");
            self.showServerStatus(false);
          }
        }
      });

      if (! self.system.hasOwnProperty('database')) {
        $.ajax({
          type: "GET",
          cache: false,
          url: "/_api/database/current",
          contentType: "application/json",
          processData: false,
          async: true,
          success: function(data) {
            var name = data.result.name;
            self.system.database = name;

            var timer = window.setInterval(function () {
              var navElement = $('#databaseNavi');

              if (navElement) {
                window.clearTimeout(timer);
                timer = null;

                if (name === '_system') {
                  // show "logs" button
                  $('.logs-menu').css('visibility', 'visible');
                  $('.logs-menu').css('display', 'inline');
                  // show dbs menues
                  $('#databaseNavi').css('display','inline');
                }
                else {
                  // hide "logs" button
                  $('.logs-menu').css('visibility', 'hidden');
                  $('.logs-menu').css('display', 'none');
                }
                self.render();
              }
            }, 50);
          }
        });
      }
    },

    renderVersion: function () {
      if (this.system.hasOwnProperty('database') && this.system.hasOwnProperty('name')) {
        $(this.el).html(this.template.render({
          name: this.system.name,
          version: this.system.version,
          database: this.system.database
        }));
      }
    },

    render: function () {
      if (!this.system.version) {
        this.getVersion();
      }
      $(this.el).html(this.template.render({
        name: this.system.name,
        version: this.system.version
      }));
      return this;
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, console, Dygraph, _,templateEngine */

(function () {
  "use strict";

  function fmtNumber (n, nk) {
    if (n === undefined || n === null) {
      n = 0;
    }

    return n.toFixed(nk);
  }

  window.DashboardView = Backbone.View.extend({
    el: '#content',
    interval: 10000, // in milliseconds
    defaultTimeFrame: 20 * 60 * 1000, // 20 minutes in milliseconds
    defaultDetailFrame: 2 * 24 * 60 * 60 * 1000,
    history: {},
    graphs: {},

    events: {
      // will be filled in initialize
    },

    tendencies: {
      asyncPerSecondCurrent: [
        "asyncPerSecondCurrent", "asyncPerSecondPercentChange"
      ],

      syncPerSecondCurrent: [
        "syncPerSecondCurrent", "syncPerSecondPercentChange"
      ],

      clientConnectionsCurrent: [
        "clientConnectionsCurrent", "clientConnectionsPercentChange"
      ],

      clientConnectionsAverage: [
        "clientConnections15M", "clientConnections15MPercentChange"
      ],

      numberOfThreadsCurrent: [
        "numberOfThreadsCurrent", "numberOfThreadsPercentChange"
      ],

      numberOfThreadsAverage: [
        "numberOfThreads15M", "numberOfThreads15MPercentChange"
      ],

      virtualSizeCurrent: [
        "virtualSizeCurrent", "virtualSizePercentChange"
      ],

      virtualSizeAverage: [
        "virtualSize15M", "virtualSize15MPercentChange"
      ]
    },

    barCharts: {
      totalTimeDistribution: [
        "queueTimeDistributionPercent", "requestTimeDistributionPercent"
      ],
      dataTransferDistribution: [
        "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"
      ]
    },

    barChartsElementNames: {
      queueTimeDistributionPercent: "Queue",
      requestTimeDistributionPercent: "Computation",
      bytesSentDistributionPercent: "Bytes sent",
      bytesReceivedDistributionPercent: "Bytes received"

    },

    getDetailFigure : function (e) {
      var figure = $(e.currentTarget).attr("id").replace(/ChartButton/g, "");
      return figure;
    },

    showDetail: function (e) {
      var self = this,
          figure = this.getDetailFigure(e),
          options;

      options = this.dygraphConfig.getDetailChartConfig(figure);

      this.getHistoryStatistics(figure);
      this.detailGraphFigure = figure;

      window.modalView.hideFooter = true;
      window.modalView.hide();
      window.modalView.show(
        "modalGraph.ejs",
        options.header,
        undefined,
        undefined,
        undefined,
        undefined,
        this.events
      );

      window.modalView.hideFooter = false;

      $('#modal-dialog').on('hidden', function () {
        self.hidden();
      });

      $('#modal-dialog').toggleClass("modal-chart-detail", true);

      options.height = $(window).height() * 0.7;
      options.width = $('.modal-inner-detail').width();

      // Reselect the labelsDiv. It was not known when requesting options
      options.labelsDiv = $(options.labelsDiv)[0];

      this.detailGraph = new Dygraph(
        document.getElementById("lineChartDetail"),
        this.history[this.server][figure],
        options
      );
    },

    hidden: function () {
      this.detailGraph.destroy();
      delete this.detailGraph;
      delete this.detailGraphFigure;
    },


    getCurrentSize: function (div) {
      if (div.substr(0,1) !== "#") {
        div = "#" + div;
      }
      var height, width;
      $(div).attr("style", "");
      height = $(div).height();
      width = $(div).width();
      return {
        height: height,
        width: width
      };
    },

    prepareDygraphs: function () {
      var self = this, options;
      this.dygraphConfig.getDashBoardFigures().forEach(function (f) {
        options = self.dygraphConfig.getDefaultConfig(f);
        var dimensions = self.getCurrentSize(options.div);
        options.height = dimensions.height;
        options.width = dimensions.width;
        self.graphs[f] = new Dygraph(
          document.getElementById(options.div),
          self.history[self.server][f] || [],
          options
        );
      });
    },

    initialize: function () {
      this.dygraphConfig = this.options.dygraphConfig;
      this.d3NotInitialized = true;
      this.events["click .dashboard-sub-bar-menu-sign"] = this.showDetail.bind(this);
      this.events["mousedown .dygraph-rangesel-zoomhandle"] = this.stopUpdating.bind(this);
      this.events["mouseup .dygraph-rangesel-zoomhandle"] = this.startUpdating.bind(this);

      this.serverInfo = this.options.serverToShow;

      if (! this.serverInfo) {
        this.server = "-local-";
      } else {
        this.server = this.serverInfo.target;
      }

      this.history[this.server] = {};
    },

    updateCharts: function () {
      var self = this;
      if (this.detailGraph) {
        this.updateLineChart(this.detailGraphFigure, true);
        return;
      }
      this.prepareD3Charts(this.isUpdating);
      this.prepareResidentSize(this.isUpdating);
      this.updateTendencies();
      Object.keys(this.graphs).forEach(function (f) {
        self.updateLineChart(f, false);
      });
    },

    updateTendencies: function () {
      var self = this, map = this.tendencies;

      var tempColor = "";
      Object.keys(map).forEach(function (a) {
        var p = "";
        var v = 0;
        if (self.history.hasOwnProperty(self.server) &&
            self.history[self.server].hasOwnProperty(a)) {
          v = self.history[self.server][a][1];
        }

        if (v < 0) {
          tempColor = "#d05448";
        }
        else {
          tempColor = "#7da817";
          p = "+";
        }
        $("#" + a).html(self.history[self.server][a][0] + '<br/><span class="dashboard-figurePer" style="color: '
          + tempColor +';">' + p + v + '%</span>');
      });
    },


    updateDateWindow: function (graph, isDetailChart) {
      var t = new Date().getTime();
      var borderLeft, borderRight;
      if (isDetailChart && graph.dateWindow_) {
        borderLeft = graph.dateWindow_[0];
        borderRight = t - graph.dateWindow_[1] - this.interval * 5 > 0 ?
        graph.dateWindow_[1] : t;
        return [borderLeft, borderRight];
      }
      return [t - this.defaultTimeFrame, t];


    },

    updateLineChart: function (figure, isDetailChart) {
      var g = isDetailChart ? this.detailGraph : this.graphs[figure],
      opts = {
        file: this.history[this.server][figure],
        dateWindow: this.updateDateWindow(g, isDetailChart)
      };
      g.updateOptions(opts);
    },

    mergeDygraphHistory: function (newData, i) {
      var self = this, valueList;

      this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {

        // check if figure is known
        if (! self.dygraphConfig.mapStatToFigure[f]) {
          return;
        }

        // need at least an empty history
        if (! self.history[self.server][f]) {
          self.history[self.server][f] = [];
        }

        // generate values for this key
        valueList = [];

        self.dygraphConfig.mapStatToFigure[f].forEach(function (a) {
          if (! newData[a]) {
            return;
          }

          if (a === "times") {
            valueList.push(new Date(newData[a][i] * 1000));
          }
          else {
            valueList.push(newData[a][i]);
          }
        });

        // if we found at list one value besides times, then use the entry
        if (valueList.length > 1) {
          self.history[self.server][f].push(valueList);
        }
      });
    },

    cutOffHistory: function (f, cutoff) {
      var self = this;

      while (self.history[self.server][f].length !== 0) {
        var v = self.history[self.server][f][0][0];

        if (v >= cutoff) {
          break;
        }

        self.history[self.server][f].shift();
      }
    },

    cutOffDygraphHistory: function (cutoff) {
      var self = this;
      var cutoffDate = new Date(cutoff);

      this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {

        // check if figure is known
        if (! self.dygraphConfig.mapStatToFigure[f]) {
          return;
        }

        // history must be non-empty
        if (! self.history[self.server][f]) {
          return;
        }

        self.cutOffHistory(f, cutoffDate);
      });
    },

    mergeHistory: function (newData) {
      var self = this, i;

      for (i = 0; i < newData.times.length; ++i) {
        this.mergeDygraphHistory(newData, i);
      }

      this.cutOffDygraphHistory(new Date().getTime() - this.defaultTimeFrame);

      // convert tendency values
      Object.keys(this.tendencies).forEach(function (a) {
        var n1 = 1;
        var n2 = 1;

        if (a === "virtualSizeCurrent" || a === "virtualSizeAverage") {
          newData[self.tendencies[a][0]] /= (1024 * 1024 * 1024);
          n1 = 2;
        }
        else if (a === "clientConnectionsCurrent") {
          n1 = 0;
        }
        else if (a === "numberOfThreadsCurrent") {
          n1 = 0;
        }

        self.history[self.server][a] = [
          fmtNumber(newData[self.tendencies[a][0]], n1),
          fmtNumber(newData[self.tendencies[a][1]] * 100, n2)
        ];
      });

      // update distribution
      Object.keys(this.barCharts).forEach(function (a) {
        self.history[self.server][a] = self.mergeBarChartData(self.barCharts[a], newData);
      });

      // update physical memory
      self.history[self.server].physicalMemory = newData.physicalMemory;
      self.history[self.server].residentSizeCurrent = newData.residentSizeCurrent;
      self.history[self.server].residentSizePercent = newData.residentSizePercent;

      // generate chart description
      self.history[self.server].residentSizeChart =
      [
        {
          "key": "",
          "color": this.dygraphConfig.colors[1],
          "values": [
            {
              label: "used",
              value: newData.residentSizePercent * 100
            }
          ]
        },
        {
          "key": "",
          "color": this.dygraphConfig.colors[0],
          "values": [
            {
              label: "used",
              value: 100 - newData.residentSizePercent * 100
            }
          ]
        }
      ]
      ;

      // remember next start
      this.nextStart = newData.nextStart;
    },

    mergeBarChartData: function (attribList, newData) {
      var i, v1 = {
        "key": this.barChartsElementNames[attribList[0]],
        "color": this.dygraphConfig.colors[0],
        "values": []
      }, v2 = {
        "key": this.barChartsElementNames[attribList[1]],
        "color": this.dygraphConfig.colors[1],
        "values": []
      };
      for (i = newData[attribList[0]].values.length - 1;  0 <= i;  --i) {
        v1.values.push({
          label: this.getLabel(newData[attribList[0]].cuts, i),
          value: newData[attribList[0]].values[i]
        });
        v2.values.push({
          label: this.getLabel(newData[attribList[1]].cuts, i),
          value: newData[attribList[1]].values[i]
        });
      }
      return [v1, v2];
    },

    getLabel: function (cuts, counter) {
      if (!cuts[counter]) {
        return ">" + cuts[counter - 1];
      }
      return counter === 0 ? "0 - " +
                         cuts[counter] : cuts[counter - 1] + " - " + cuts[counter];
    },

    getStatistics: function (callback) {
      var self = this;
      var url = "/_db/_system/_admin/aardvark/statistics/short";
      var urlParams = "?start=";

      if (self.nextStart) {
        urlParams += self.nextStart;
      }
      else {
        urlParams += (new Date().getTime() - self.defaultTimeFrame) / 1000;
      }

      if (self.server !== "-local-") {
        url = self.serverInfo.endpoint + "/_admin/aardvark/statistics/cluster";
        urlParams += "&type=short&DBserver=" + self.serverInfo.target;

        if (! self.history.hasOwnProperty(self.server)) {
          self.history[self.server] = {};
        }
      }

      $.ajax(
        url + urlParams,
        {async: true}
      ).done(
        function (d) {
          if (d.times.length > 0) {
            self.isUpdating = true;
            self.mergeHistory(d);
          }
          if (self.isUpdating === false) {
            return;
          }
          if (callback) {
            callback();
          }
          self.updateCharts();
      });
    },

    getHistoryStatistics: function (figure) {
      var self = this;
      var url = "statistics/long";

      var urlParams
        = "?filter=" + this.dygraphConfig.mapStatToFigure[figure].join();

      if (self.server !== "-local-") {
        url = self.server.endpoint + "/_admin/aardvark/statistics/cluster";
        urlParams += "&type=long&DBserver=" + self.server.target;

        if (! self.history.hasOwnProperty(self.server)) {
          self.history[self.server] = {};
        }
      }

      $.ajax(
        url + urlParams,
        {async: true}
      ).done(
        function (d) {
          var i;

          self.history[self.server][figure] = [];

          for (i = 0;  i < d.times.length;  ++i) {
            self.mergeDygraphHistory(d, i, true);
          }
        }
      );
    },

    prepareResidentSize: function (update) {
      var self = this;

      var dimensions = this.getCurrentSize('#residentSizeChartContainer');

      var current = self.history[self.server].residentSizeCurrent / 1024 / 1024;
      var currentA = "";

      if (current < 1025) {
        currentA = fmtNumber(current, 2) + " MB";
      }
      else {
        currentA = fmtNumber(current / 1024, 2) + " GB";
      }

      var currentP = fmtNumber(self.history[self.server].residentSizePercent * 100, 2);
      var data = [fmtNumber(self.history[self.server].physicalMemory / 1024 / 1024 / 1024, 0) + " GB"];

      nv.addGraph(function () {
        var chart = nv.models.multiBarHorizontalChart()
          .x(function (d) {
            return d.label;
          })
          .y(function (d) {
            return d.value;
          })
          .width(dimensions.width)
          .height(dimensions.height)
          .margin({
            top: ($("residentSizeChartContainer").outerHeight() - $("residentSizeChartContainer").height()) / 2,
            right: 1,
            bottom: ($("residentSizeChartContainer").outerHeight() - $("residentSizeChartContainer").height()) / 2,
            left: 1
          })
          .showValues(false)
          .showYAxis(false)
          .showXAxis(false)
          .transitionDuration(100)
          .tooltips(false)
          .showLegend(false)
          .showControls(false)
          .stacked(true);

        chart.yAxis
          .tickFormat(function (d) {return d + "%";})
          .showMaxMin(false);
        chart.xAxis.showMaxMin(false);

        d3.select('#residentSizeChart svg')
          .datum(self.history[self.server].residentSizeChart)
          .call(chart);

        d3.select('#residentSizeChart svg').select('.nv-zeroLine').remove();

        if (update) {
          d3.select('#residentSizeChart svg').select('#total').remove();
          d3.select('#residentSizeChart svg').select('#percentage').remove();
        }

        d3.select('.dashboard-bar-chart-title .percentage')
          .html(currentA + " ("+ currentP + " %)");

        d3.select('.dashboard-bar-chart-title .absolut')
          .html(data[0]);

        nv.utils.windowResize(chart.update);

        return chart;
      }, function() {
        d3.selectAll("#residentSizeChart .nv-bar").on('click',
          function() {
            // no idea why this has to be empty, well anyways...
          }
        );
      });
    },

    prepareD3Charts: function (update) {
      var self = this;

      var barCharts = {
        totalTimeDistribution: [
          "queueTimeDistributionPercent", "requestTimeDistributionPercent"],
        dataTransferDistribution: [
          "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"]
      };

      if (this.d3NotInitialized) {
          update = false;
          this.d3NotInitialized = false;
      }

      _.each(Object.keys(barCharts), function (k) {
        var dimensions = self.getCurrentSize('#' + k
          + 'Container .dashboard-interior-chart');

        var selector = "#" + k + "Container svg";

        nv.addGraph(function () {
          var tickMarks = [0, 0.25, 0.5, 0.75, 1];
          var marginLeft = 75;
          var marginBottom = 23;
          var bottomSpacer = 6;

          if (dimensions.width < 219) {
            tickMarks = [0, 0.5, 1];
            marginLeft = 72;
            marginBottom = 21;
            bottomSpacer = 5;
          }
          else if (dimensions.width < 299) {
            tickMarks = [0, 0.3334, 0.6667, 1];
            marginLeft = 77;
          }
          else if (dimensions.width < 379) {
            marginLeft = 87;
          }
          else if (dimensions.width < 459) {
            marginLeft = 95;
          }
          else if (dimensions.width < 539) {
            marginLeft = 100;
          }
          else if (dimensions.width < 619) {
            marginLeft = 105;
          }

          var chart = nv.models.multiBarHorizontalChart()
            .x(function (d) {
              return d.label;
            })
            .y(function (d) {
              return d.value;
            })
            .width(dimensions.width)
            .height(dimensions.height)
            .margin({
              top: 5,
              right: 20,
              bottom: marginBottom,
              left: marginLeft
            })
            .showValues(false)
            .showYAxis(true)
            .showXAxis(true)
            .transitionDuration(100)
            .tooltips(false)
            .showLegend(false)
            .showControls(false)
            .forceY([0,1]);

          chart.yAxis
            .showMaxMin(false);

          var yTicks2 = d3.select('.nv-y.nv-axis')
            .selectAll('text')
            .attr('transform', 'translate (0, ' + bottomSpacer + ')') ;

          chart.yAxis
            .tickValues(tickMarks)
            .tickFormat(function (d) {return fmtNumber(((d * 100 * 100) / 100), 0) + "%";});

          d3.select(selector)
            .datum(self.history[self.server][k])
            .call(chart);

          nv.utils.windowResize(chart.update);

          return chart;
        }, function() {
          d3.selectAll(selector + " .nv-bar").on('click',
            function() {
              // no idea why this has to be empty, well anyways...
            }
          );
        });
      });

    },

    stopUpdating: function () {
      this.isUpdating = false;
    },

  startUpdating: function () {
    var self = this;
    if (self.timer) {
      return;
    }
    self.timer = window.setInterval(function () {
        self.getStatistics();
      },
      self.interval
    );
  },


  resize: function () {
    if (!this.isUpdating) {
      return;
    }
    var self = this, dimensions;
      _.each(this.graphs,function (g) {
      dimensions = self.getCurrentSize(g.maindiv_.id);
      g.resize(dimensions.width, dimensions.height);
    });
    if (this.detailGraph) {
      dimensions = this.getCurrentSize(this.detailGraph.maindiv_.id);
      this.detailGraph.resize(dimensions.width, dimensions.height);
    }
    this.prepareD3Charts(true);
    this.prepareResidentSize(true);
  },

  template: templateEngine.createTemplate("dashboardView.ejs"),

  render: function (modalView) {
    if (!modalView)  {
      $(this.el).html(this.template.render());
    }
    var callback = function() {
      this.prepareDygraphs();
      if (this.isUpdating) {
        this.prepareD3Charts();
        this.prepareResidentSize();
        this.updateTendencies();
      }
      this.startUpdating();
    }.bind(this);

    //check if user has _system permission
    var authorized = this.options.database.hasSystemAccess();
    if (!authorized) {
      $('.contentDiv').remove();
      $('.headerBar').remove();
      $('.dashboard-headerbar').remove();
      $('.dashboard-row').remove();
      $('#content').append(
        '<div style="color: red">You do not have permission to view this page.</div>'
      );
      $('#content').append(
        '<div style="color: red">You can switch to \'_system\' to see the dashboard.</div>'
      );
    }
    else {
      this.getStatistics(callback);
    }
  }
});
}());

/*jshint browser: true */
/*global Backbone, $, window, setTimeout, Joi, _ */
/*global templateEngine*/

(function () {
  "use strict";

  var createButtonStub = function(type, title, cb, confirm) {
    return {
      type: type,
      title: title,
      callback: cb,
      confirm: confirm
    };
  };

  var createTextStub = function(type, label, value, info, placeholder, mandatory, joiObj,
                                addDelete, addAdd, maxEntrySize, tags) {
    var obj = {
      type: type,
      label: label
    };
    if (value !== undefined) {
      obj.value = value;
    }
    if (info !== undefined) {
      obj.info = info;
    }
    if (placeholder !== undefined) {
      obj.placeholder = placeholder;
    }
    if (mandatory !== undefined) {
      obj.mandatory = mandatory;
    }
    if (addDelete !== undefined) {
      obj.addDelete = addDelete;
    }
    if (addAdd !== undefined) {
      obj.addAdd = addAdd;
    }
    if (maxEntrySize !== undefined) {
      obj.maxEntrySize = maxEntrySize;
    }
    if (tags !== undefined) {
      obj.tags = tags;
    }
    if (joiObj){
      // returns true if the string contains the match
      obj.validateInput = function() {
        // return regexp.test(el.val());
        return joiObj;
      };
    }
    return obj;
  };

  window.ModalView = Backbone.View.extend({

    _validators: [],
    _validateWatchers: [],
    baseTemplate: templateEngine.createTemplate("modalBase.ejs"),
    tableTemplate: templateEngine.createTemplate("modalTable.ejs"),
    el: "#modalPlaceholder",
    contentEl: "#modalContent",
    hideFooter: false,
    confirm: {
      list: "#modal-delete-confirmation",
      yes: "#modal-confirm-delete",
      no: "#modal-abort-delete"
    },
    enabledHotkey: false,
    enableHotKeys : true,

    buttons: {
      SUCCESS: "success",
      NOTIFICATION: "notification",
      DELETE: "danger",
      NEUTRAL: "neutral",
      CLOSE: "close"
    },
    tables: {
      READONLY: "readonly",
      TEXT: "text",
      BLOB: "blob",
      PASSWORD: "password",
      SELECT: "select",
      SELECT2: "select2",
      CHECKBOX: "checkbox"
    },

    initialize: function() {
      Object.freeze(this.buttons);
      Object.freeze(this.tables);
    },

    createModalHotkeys: function() {
      //submit modal
      $(this.el).bind('keydown', 'return', function(){
        $('.createModalDialog .modal-footer .button-success').click();
      });
      $("input", $(this.el)).bind('keydown', 'return', function(){
        $('.createModalDialog .modal-footer .button-success').click();
      });
      $("select", $(this.el)).bind('keydown', 'return', function(){
        $('.createModalDialog .modal-footer .button-success').click();
      });
    },

    createInitModalHotkeys: function() {
      var self = this;
      //navigate through modal buttons
      //left cursor
      $(this.el).bind('keydown', 'left', function(){
        self.navigateThroughButtons('left');
      });
      //right cursor
      $(this.el).bind('keydown', 'right', function(){
        self.navigateThroughButtons('right');
      });

    },

    navigateThroughButtons: function(direction) {
      var hasFocus = $('.createModalDialog .modal-footer button').is(':focus');
      if (hasFocus === false) {
        if (direction === 'left') {
          $('.createModalDialog .modal-footer button').first().focus();
        }
        else if (direction === 'right') {
          $('..createModalDialog .modal-footer button').last().focus();
        }
      }
      else if (hasFocus === true) {
        if (direction === 'left') {
          $(':focus').prev().focus();
        }
        else if (direction === 'right') {
          $(':focus').next().focus();
        }
      }

    },

    createCloseButton: function(title, cb) {
      var self = this;
      return createButtonStub(this.buttons.CLOSE, title, function () {
        self.hide();
        if (cb) {
          cb();
        }
      });
    },

    createSuccessButton: function(title, cb) {
      return createButtonStub(this.buttons.SUCCESS, title, cb);
    },

    createNotificationButton: function(title, cb) {
      return createButtonStub(this.buttons.NOTIFICATION, title, cb);
    },

    createDeleteButton: function(title, cb, confirm) {
      return createButtonStub(this.buttons.DELETE, title, cb, confirm);
    },

    createNeutralButton: function(title, cb) {
      return createButtonStub(this.buttons.NEUTRAL, title, cb);
    },

    createDisabledButton: function(title) {
      var disabledButton = createButtonStub(this.buttons.NEUTRAL, title);
      disabledButton.disabled = true;
      return disabledButton;
    },

    createReadOnlyEntry: function(id, label, value, info, addDelete, addAdd) {
      var obj = createTextStub(this.tables.READONLY, label, value, info,undefined, undefined,
        undefined,addDelete, addAdd);
      obj.id = id;
      return obj;
    },

    createTextEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.TEXT, label, value, info, placeholder, mandatory,
                               regexp);
      obj.id = id;
      return obj;
    },

    createBlobEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.BLOB, label, value, info, placeholder, mandatory,
                               regexp);
      obj.id = id;
      return obj;
    },

    createSelect2Entry: function(
      id, label, value, info, placeholder, mandatory, addDelete, addAdd, maxEntrySize, tags) {
      var obj = createTextStub(this.tables.SELECT2, label, value, info, placeholder,
        mandatory, undefined, addDelete, addAdd, maxEntrySize, tags);
      obj.id = id;
      return obj;
    },

    createPasswordEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.PASSWORD, label, value, info, placeholder, mandatory, regexp);
      obj.id = id;
      return obj;
    },

    createCheckboxEntry: function(id, label, value, info, checked) {
      var obj = createTextStub(this.tables.CHECKBOX, label, value, info);
      obj.id = id;
      if (checked) {
        obj.checked = checked;
      }
      return obj;
    },

    createSelectEntry: function(id, label, selected, info, options) {
      var obj = createTextStub(this.tables.SELECT, label, null, info);
      obj.id = id;
      if (selected) {
        obj.selected = selected;
      }
      obj.options = options;
      return obj;
    },

    createOptionEntry: function(label, value) {
      return {
        label: label,
        value: value || label
      };
    },

    show: function(templateName, title, buttons, tableContent, advancedContent, extraInfo, events, noConfirm) {
      var self = this, lastBtn, confirmMsg, closeButtonFound = false;
      buttons = buttons || [];
      noConfirm = Boolean(noConfirm);
      this.clearValidators();
      if (buttons.length > 0) {
        buttons.forEach(function (b) {
          if (b.type === self.buttons.CLOSE) {
              closeButtonFound = true;
          }
          if (b.type === self.buttons.DELETE) {
              confirmMsg = confirmMsg || b.confirm;
          }
        });
        if (!closeButtonFound) {
          // Insert close as second from right
          lastBtn = buttons.pop();
          buttons.push(self.createCloseButton('Cancel'));
          buttons.push(lastBtn);
        }
      } else {
        buttons.push(self.createCloseButton('Close'));
      }
      $(this.el).html(this.baseTemplate.render({
        title: title,
        buttons: buttons,
        hideFooter: this.hideFooter,
        confirm: confirmMsg
      }));
      _.each(buttons, function(b, i) {
        if (b.disabled || !b.callback) {
          return;
        }
        if (b.type === self.buttons.DELETE && !noConfirm) {
          $("#modalButton" + i).bind("click", function() {
            $(self.confirm.yes).unbind("click");
            $(self.confirm.yes).bind("click", b.callback);
            $(self.confirm.list).css("display", "block");
          });
          return;
        }
        $("#modalButton" + i).bind("click", b.callback);
      });
      $(this.confirm.no).bind("click", function() {
        $(self.confirm.list).css("display", "none");
      });

      var template = templateEngine.createTemplate(templateName);
      $(".createModalDialog .modal-body").html(template.render({
        content: tableContent,
        advancedContent: advancedContent,
        info: extraInfo
      }));
      $('.createModalDialog .modalTooltips').tooltip({
        position: {
          my: "left top",
          at: "right+55 top-1"
        }
      });

      var completeTableContent = tableContent || [];
      if (advancedContent && advancedContent.content) {
        completeTableContent = completeTableContent.concat(advancedContent.content);
      }

      _.each(completeTableContent, function(row) {
        self.modalBindValidation(row);
        if (row.type === self.tables.SELECT2) {
          //handle select2
          $('#'+row.id).select2({
            tags: row.tags || [],
            showSearchBox: false,
            minimumResultsForSearch: -1,
            width: "336px",
            maximumSelectionSize: row.maxEntrySize || 8
          });
        }
      });

      if (events) {
        this.events = events;
        this.delegateEvents();
      }

      $("#modal-dialog").modal("show");

      //enable modal hotkeys after rendering is complete
      if (this.enabledHotkey === false) {
        this.createInitModalHotkeys();
        this.enabledHotkey = true;
      }
      if (this.enableHotKeys) {
        this.createModalHotkeys();
      }

      //if input-field is available -> autofocus first one
      var focus = $('#modal-dialog').find('input');
      if (focus) {
        setTimeout(function() {
          var focus = $('#modal-dialog');
          if (focus.length > 0) {
            focus = focus.find('input'); 
              if (focus.length > 0) {
                $(focus[0]).focus();
              }
          }
        }, 800);
      }

    },

    modalBindValidation: function(entry) {
      var self = this;
      if (entry.hasOwnProperty("id")
        && entry.hasOwnProperty("validateInput")) {
        var validCheck = function() {
          var $el = $("#" + entry.id);
          var validation = entry.validateInput($el);
          var error = false;
          _.each(validation, function(validator) {
            var value = $el.val();
            if (!validator.rule) {
              validator = {rule: validator};
            }
            if (typeof validator.rule === 'function') {
              try {
                validator.rule(value);
              } catch (e) {
                error = validator.msg || e.message;
              }
            } else {
              var result = Joi.validate(value, validator.rule);
              if (result.error) {
                error = validator.msg || result.error.message;
              }
            }
            if (error) {
              return false;
            }
          });
          if (error) {
            return error;
          }
        };
        var $el = $('#' + entry.id);
        // catch result of validation and act
        $el.on('keyup focusout', function() {
          var msg = validCheck();
          var errorElement = $el.next()[0];
          if (msg) {
            $el.addClass('invalid-input');
            if (errorElement) {
              //error element available
              $(errorElement).text(msg);
            }
            else {
              //error element not available
              $el.after('<p class="errorMessage">' + msg+ '</p>');
            }
            $('.createModalDialog .modal-footer .button-success')
              .prop('disabled', true)
              .addClass('disabled');
          } else {
            $el.removeClass('invalid-input');
            if (errorElement) {
              $(errorElement).remove();
            }
            self.modalTestAll();
          }
        });
        this._validators.push(validCheck);
        this._validateWatchers.push($el);
      }
      
    },

    modalTestAll: function() {
      var tests = _.map(this._validators, function(v) {
        return v();
      });
      var invalid = _.any(tests);
      if (invalid) {
        $('.createModalDialog .modal-footer .button-success')
          .prop('disabled', true)
          .addClass('disabled');
      } else {
        $('.createModalDialog .modal-footer .button-success')
          .prop('disabled', false)
          .removeClass('disabled');
      }
      return !invalid;
    },

    clearValidators: function() {
      this._validators = [];
      _.each(this._validateWatchers, function(w) {
        w.unbind('keyup focusout');
      });
      this._validateWatchers = [];
    },

    hide: function() {
      this.clearValidators();
      $("#modal-dialog").modal("hide");
    }
  });
}());

/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, window */
window.StatisticsDescriptionCollection = Backbone.Collection.extend({
  model: window.StatisticsDescription,
  url: "/_admin/statistics-description",
  parse: function(response) {
    return response;
  }
});

/*global window, $, Backbone, templateEngine, plannerTemplateEngine, alert */

(function() {
  "use strict";

  window.ClusterDownView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("clusterDown.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #relaunchCluster"  : "relaunchCluster",
      "click #upgradeCluster"   : "upgradeCluster",
      "click #editPlan"         : "editPlan",
      "click #submitEditPlan"   : "submitEditPlan",
      "click #deletePlan"       : "deletePlan",
      "click #submitDeletePlan" : "submitDeletePlan"
    },

    render: function() {
      var planVersion = window.versionHelper.fromString(
        window.App.clusterPlan.getVersion()
      );
      var currentVersion;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_admin/database/target-version",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          currentVersion = data.version;
        }
      });
      currentVersion = window.versionHelper.fromString(
        currentVersion
      );
      var shouldUpgrade = false;
      if (currentVersion.major > planVersion.major
        || (
          currentVersion.major === planVersion.major
          && currentVersion.minor > planVersion.minor
        )) {
        shouldUpgrade = true;
      }
      $(this.el).html(this.template.render({
        canUpgrade: shouldUpgrade
      }));
      $(this.el).append(this.modal.render({}));
    },

    relaunchCluster: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster will be relaunched');
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/relaunch",
        success: function() {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.navigate("showCluster", {trigger: true});
        }
      });
    },

    upgradeCluster: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster will be upgraded');
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/upgrade",
        success: function() {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.clusterPlan.fetch();
          window.App.navigate("showCluster", {trigger: true});
        }
      });
    },

    editPlan: function() {
      $('#deletePlanModal').modal('hide');
      $('#editPlanModal').modal('show');
    },

    submitEditPlan : function() {
      $('#editPlanModal').modal('hide');
      window.App.clusterPlan.cleanUp();
      var plan = window.App.clusterPlan;
      if (plan.isTestSetup()) {
        window.App.navigate("planTest", {trigger : true});
        return;
      }
      window.App.navigate("planAsymmetrical", {trigger : true});
    },

    deletePlan: function() {
      $('#editPlanModal').modal('hide');
      $('#deletePlanModal').modal('show');
    },

    submitDeletePlan : function() {
      $('#deletePlanModal').modal('hide');
      window.App.clusterPlan.cleanUp();
      window.App.clusterPlan.destroy();
      window.App.clusterPlan = new window.ClusterPlan();
      window.App.planScenario();
    }

  });

}());

/*global window, $, Backbone, templateEngine, plannerTemplateEngine, alert, _ */

(function() {
  "use strict";

  window.ClusterUnreachableView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("clusterUnreachable.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #clusterShutdown": "shutdown"
    },

    initialize: function() {
      this.coordinators = new window.ClusterCoordinators([], {
      });
    },


    retryConnection: function() {
      this.coordinators.checkConnection(function() {
        window.App.showCluster();
      });
    },

    shutdown: function() {
      window.clearTimeout(this.timer);
      window.App.shutdownView.clusterShutdown();
    },

    render: function() {
      var plan = window.App.clusterPlan;
      var list = [];
      if (plan && plan.has("runInfo")) {
        var startServerInfos = _.where(plan.get("runInfo"), {isStartServers: true});
        _.each(
          _.filter(startServerInfos, function(s) {
            return _.contains(s.roles, "Coordinator");
          }), function(s) {
            var name = s.endpoints[0].split("://")[1];
            name = name.split(":")[0];
            list.push(name);
          }
        );
      }
      $(this.el).html(this.template.render({
        coordinators: list
      }));
      $(this.el).append(this.modal.render({}));
      this.timer = window.setTimeout(this.retryConnection.bind(this), 10000);
    }

  });
}());

/*global Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, Dygraph, _,templateEngine */

(function() {
  "use strict";

  window.ServerDashboardView = window.DashboardView.extend({
    modal : true,

    hide: function() {
      window.App.showClusterView.startUpdating();
      this.stopUpdating();
    },

    render: function() {
      var self = this;
      window.modalView.hideFooter = true;
      window.modalView.show(
            "dashboardView.ejs",
            null,
            undefined,
            undefined,
            undefined,
            this.events

      );
      $('#modal-dialog').toggleClass("modal-chart-detail", true);
      window.DashboardView.prototype.render.bind(this)(true);
      window.modalView.hideFooter = false;


      $('#modal-dialog').on('hidden', function () {
            self.hide();
      });

      // Inject the closing x
      var closingX = document.createElement("button");
      closingX.className = "close";
      closingX.appendChild(
        document.createTextNode("×")
      );
      closingX = $(closingX);
      closingX.attr("data-dismiss", "modal");
      closingX.attr("aria-hidden", "true");
      closingX.attr("type", "button");
      $(".modal-body .headerBar:first-child")
        .toggleClass("headerBar", false)
        .toggleClass("modal-dashboard-header", true)
        .append(closingX);

    }
  });

}());

/*global templateEngine, window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";

  window.LoginModalView = Backbone.View.extend({

    template: templateEngine.createTemplate("loginModal.ejs"),
    el: '#modalPlaceholder',

    events: {
      "click #confirmLogin": "confirmLogin",
      "hidden #loginModalLayer": "hidden"
    },

    hidden: function () {
      this.undelegateEvents();
      window.App.isCheckingUser = false;
      $(this.el).html("");
    },

    confirmLogin: function() {
      var uName = $("#username").val();
      var passwd = $("#password").val();
      window.App.clusterPlan.storeCredentials(uName, passwd);
      this.hideModal();
    },

    hideModal: function () {
      $('#loginModalLayer').modal('hide');
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      $('#loginModalLayer').modal('show');
    }
  });

}());

/*global Backbone, $, _, window, templateEngine */
(function() {

  "use strict";

  window.PlanScenarioSelectorView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("planScenarioSelector.ejs", "planner"),


    events: {
      "click #multiServerAsymmetrical": "multiServerAsymmetrical",
      "click #singleServer": "singleServer"
    },

    render: function() {
      $(this.el).html(this.template.render({}));
    },

    multiServerAsymmetrical: function() {
      window.App.navigate(
        "planAsymmetrical", {trigger: true}
      );
    },

    singleServer: function() {
      window.App.navigate(
        "planTest", {trigger: true}
      );
    }

  });
}());

/*global window, btoa, $, Backbone, templateEngine, alert, _ */

(function() {
  "use strict";

  window.PlanSymmetricView = Backbone.View.extend({
    el: "#content",
    template: templateEngine.createTemplate("symmetricPlan.ejs"),
    entryTemplate: templateEngine.createTemplate("serverEntry.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),
    connectionValidationKey: null,

    events: {
      "click #startSymmetricPlan"   : "startPlan",
      "click .add"                  : "addEntry",
      "click .delete"               : "removeEntry",
      "click #cancel"               : "cancel",
      "click #test-all-connections" : "checkAllConnections",
      "focusout .host"              : "checkAllConnections",
      "focusout .port"              : "checkAllConnections",
      "focusout .user"              : "checkAllConnections",
      "focusout .passwd"            : "checkAllConnections"
    },

    cancel: function() {
      if(window.App.clusterPlan.get("plan")) {
        window.App.navigate("handleClusterDown", {trigger: true});
      } else {
        window.App.navigate("planScenario", {trigger: true});
      }
    },

    startPlan: function() {
      var self = this;
      var data = {dispatchers: []};
      var foundCoordinator = false;
      var foundDBServer = false;
            data.useSSLonDBservers = !!$(".useSSLonDBservers").prop('checked');
      data.useSSLonCoordinators = !!$(".useSSLonCoordinators").prop('checked');
      $(".dispatcher").each(function(i, dispatcher) {
        var host = $(".host", dispatcher).val();
        var port = $(".port", dispatcher).val();
        var user = $(".user", dispatcher).val();
        var passwd = $(".passwd", dispatcher).val();
        if (!host || 0 === host.length || !port || 0 === port.length) {
          return true;
        }
        var hostObject = {host :  host + ":" + port};
        if (!self.isSymmetric) {
          hostObject.isDBServer = !!$(".isDBServer", dispatcher).prop('checked');
          hostObject.isCoordinator = !!$(".isCoordinator", dispatcher).prop('checked');
        } else {
          hostObject.isDBServer = true;
          hostObject.isCoordinator = true;
        }

        hostObject.username = user;
        hostObject.passwd = passwd;

        foundCoordinator = foundCoordinator || hostObject.isCoordinator;
        foundDBServer = foundDBServer || hostObject.isDBServer;

        data.dispatchers.push(hostObject);
      });
            if (!self.isSymmetric) {
        if (!foundDBServer) {
            alert("Please provide at least one database server");
            return;
        }
        if (!foundCoordinator) {
            alert("Please provide at least one coordinator");
            return;
        }
      } else {
        if ( data.dispatchers.length === 0) {
            alert("Please provide at least one host");
            return;
        }

      }

      data.type = this.isSymmetric ? "symmetricalSetup" : "asymmetricalSetup";
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster is being launched');
      delete window.App.clusterPlan._coord;
      window.App.clusterPlan.save(
        data,
        {
          success : function() {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            window.App.updateAllUrls();
            window.App.navigate("showCluster", {trigger: true});
          },
          error: function(obj, err) {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            alert("Error while starting the cluster: " + err.statusText);
          }
        }
      );
          },

    addEntry: function() {
      //disable launch button
      this.disableLaunchButton();

      var lastUser = $("#server_list div.control-group.dispatcher:last .user").val();
      var lastPasswd = $("#server_list div.control-group.dispatcher:last .passwd").val();

      $("#server_list").append(this.entryTemplate.render({
        isSymmetric: this.isSymmetric,
        isFirst: false,
        isCoordinator: true,
        isDBServer: true,
        host: '',
        port: '',
        user: lastUser,
        passwd: lastPasswd
      }));
    },

    removeEntry: function(e) {
      $(e.currentTarget).closest(".control-group").remove();
      this.checkAllConnections();
    },

    render: function(isSymmetric) {
      var params = {},
        isFirst = true,
        config = window.App.clusterPlan.get("config");
      this.isSymmetric = isSymmetric;
      $(this.el).html(this.template.render({
        isSymmetric : isSymmetric,
        params      : params,
        useSSLonDBservers: config && config.useSSLonDBservers ?
            config.useSSLonDBservers : false,
        useSSLonCoordinators: config && config.useSSLonCoordinators ?
            config.useSSLonCoordinators : false
      }));
      if (config) {
        var self = this,
        isCoordinator = false,
        isDBServer = false;
        _.each(config.dispatchers, function(dispatcher) {
          if (dispatcher.allowDBservers === undefined) {
            isDBServer = true;
          } else {
            isDBServer = dispatcher.allowDBservers;
          }
          if (dispatcher.allowCoordinators === undefined) {
            isCoordinator = true;
          } else {
            isCoordinator = dispatcher.allowCoordinators;
          }
          var host = dispatcher.endpoint;
          host = host.split("//")[1];
          host = host.split(":");

          if (host === 'localhost') {
            host = '127.0.0.1';
          }

          var user = dispatcher.username;
          var passwd = dispatcher.passwd;
          var template = self.entryTemplate.render({
            isSymmetric: isSymmetric,
            isFirst: isFirst,
            host: host[0],
            port: host[1],
            isCoordinator: isCoordinator,
            isDBServer: isDBServer,
            user: user,
            passwd: passwd
          });
          $("#server_list").append(template);
          isFirst = false;
        });
      } else {
        $("#server_list").append(this.entryTemplate.render({
          isSymmetric: isSymmetric,
          isFirst: true,
          isCoordinator: true,
          isDBServer: true,
          host: '',
          port: '',
          user: '',
          passwd: ''
        }));
      }
      //initially disable lunch button
      this.disableLaunchButton();

      $(this.el).append(this.modal.render({}));

    },

    readAllConnections: function() {
      var res = [];
      $(".dispatcher").each(function(key, row) {
        var obj = {
          host: $('.host', row).val(),
          port: $('.port', row).val(),
          user: $('.user', row).val(),
          passwd: $('.passwd', row).val()
        };
        if (obj.host && obj.port) {
          res.push(obj);
        }
      });
      return res;
    },

    checkAllConnections: function() {
      var self = this;
      var connectionValidationKey = Math.random();
      this.connectionValidationKey = connectionValidationKey;
      $('.cluster-connection-check-success').remove();
      $('.cluster-connection-check-fail').remove();
      var list = this.readAllConnections();
      if (list.length) {
        try {
          $.ajax({
            async: true,
            cache: false,
            type: "POST",
            url: "/_admin/aardvark/cluster/communicationCheck",
            data: JSON.stringify(list),
            success: function(checkList) {
              if (connectionValidationKey === self.connectionValidationKey) {
                var dispatcher = $(".dispatcher");
                var i = 0;
                dispatcher.each(function(key, row) {
                  var host = $(".host", row).val();
                  var port = $(".port", row).val();
                  if (host && port) {
                    if (checkList[i]) {
                      $(".controls:first", row).append(
                        '<span class="cluster-connection-check-success">Connection: ok</span>'
                      );
                    } else {
                      $(".controls:first", row).append(
                        '<span class="cluster-connection-check-fail">Connection: fail</span>'
                      );
                    }
                    i++;
                  }
                });
                self.checkDispatcherArray(checkList, connectionValidationKey);
              }
            }
          });
        } catch (e) {
          this.disableLaunchButton();
        }
      }
    },

    checkDispatcherArray: function(dispatcherArray, connectionValidationKey) {
      if(
        (_.every(dispatcherArray, function (e) {return e;}))
          && connectionValidationKey === this.connectionValidationKey
        ) {
        this.enableLaunchButton();
      }
    },

    disableLaunchButton: function() {
      $('#startSymmetricPlan').attr('disabled', 'disabled');
      $('#startSymmetricPlan').removeClass('button-success');
      $('#startSymmetricPlan').addClass('button-neutral');
    },

    enableLaunchButton: function() {
      $('#startSymmetricPlan').attr('disabled', false);
      $('#startSymmetricPlan').removeClass('button-neutral');
      $('#startSymmetricPlan').addClass('button-success');
    }

  });

}());



/*global window, $, Backbone, templateEngine, alert */

(function() {
  "use strict";

  window.PlanTestView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("testPlan.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #startTestPlan": "startPlan",
      "click #cancel": "cancel"
    },

    cancel: function() {
      if(window.App.clusterPlan.get("plan")) {
        window.App.navigate("handleClusterDown", {trigger: true});
      } else {
        window.App.navigate("planScenario", {trigger: true});
      }
    },

    startPlan: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster is being launched');
      var h = $("#host").val(),
        p = $("#port").val(),
        c = $("#coordinators").val(),
        d = $("#dbs").val();
      if (!h) {
        alert("Please define a host");
        return;
      }
      if (!p) {
        alert("Please define a port");
        return;
      }
      if (!c || c < 0) {
        alert("Please define a number of coordinators");
        return;
      }
      if (!d || d < 0) {
        alert("Please define a number of database servers");
        return;
      }
      delete window.App.clusterPlan._coord;
      window.App.clusterPlan.save(
        {
          type: "testSetup",
          dispatchers: h + ":" + p,
          numberDBServers: parseInt(d, 10),
          numberCoordinators: parseInt(c, 10)
        },
        {
          success: function() {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            window.App.updateAllUrls();
            window.App.navigate("showCluster", {trigger: true});
          },
          error: function(obj, err) {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            alert("Error while starting the cluster: " + err.statusText);
          }
        }
      );
    },

    render: function() {
      var param = {};
      var config = window.App.clusterPlan.get("config");
      if (config) {
        param.dbs = config.numberOfDBservers;
        param.coords = config.numberOfCoordinators;
        var host = config.dispatchers.d1.endpoint;
        host = host.split("://")[1];
        host = host.split(":");

        if (host === 'localhost') {
          host = '127.0.0.1';
        }

        param.hostname = host[0];
        param.port = host[1];
      } else {
        param.dbs = 3;
        param.coords = 2;
        param.hostname = window.location.hostname;

        if (param.hostname === 'localhost') {
          param.hostname = '127.0.0.1';
        }

        param.port = window.location.port;
      }
      $(this.el).html(this.template.render(param));
      $(this.el).append(this.modal.render({}));
    }
  });

}());

/*global window, $, Backbone, templateEngine, alert, _, d3, Dygraph, document */

(function() {
  "use strict";

  window.ShowClusterView = Backbone.View.extend({
    detailEl: '#modalPlaceholder',
    el: "#content",
    defaultFrame: 20 * 60 * 1000,
    template: templateEngine.createTemplate("showCluster.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),
    detailTemplate: templateEngine.createTemplate("detailView.ejs"),

    events: {
      "change #selectDB"                : "updateCollections",
      "change #selectCol"               : "updateShards",
      "click .dbserver.success"         : "dashboard",
      "click .coordinator.success"      : "dashboard"
    },

    replaceSVGs: function() {
      $(".svgToReplace").each(function() {
        var img = $(this);
        var id = img.attr("id");
        var src = img.attr("src");
        $.get(src, function(d) {
          var svg = $(d).find("svg");
          svg.attr("id", id)
          .attr("class", "icon")
          .removeAttr("xmlns:a");
          img.replaceWith(svg);
        }, "xml");
      });
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    },

    setShowAll: function() {
      this.graphShowAll = true;
    },

    resetShowAll: function() {
      this.graphShowAll = false;
      this.renderLineChart();
    },


    initialize: function() {
      this.interval = 10000;
      this.isUpdating = false;
      this.timer = null;
      this.knownServers = [];
      this.graph = undefined;
      this.graphShowAll = false;
      this.updateServerTime();
      this.dygraphConfig = this.options.dygraphConfig;
      this.dbservers = new window.ClusterServers([], {
        interval: this.interval
      });
      this.coordinators = new window.ClusterCoordinators([], {
        interval: this.interval
      });
      this.documentStore =  new window.arangoDocuments();
      this.statisticsDescription = new window.StatisticsDescription();
      this.statisticsDescription.fetch({
        async: false
      });
      this.dbs = new window.ClusterDatabases([], {
        interval: this.interval
      });
      this.cols = new window.ClusterCollections();
      this.shards = new window.ClusterShards();
      this.startUpdating();
    },

    listByAddress: function(callback) {
      var byAddress = {};
      var self = this;
      this.dbservers.byAddress(byAddress, function(res) {
        self.coordinators.byAddress(res, callback);
      });
    },

    updateCollections: function() {
      var self = this;
      var selCol = $("#selectCol");
      var dbName = $("#selectDB").find(":selected").attr("id");
      if (!dbName) {
        return;
      }
      var colName = selCol.find(":selected").attr("id");
      selCol.html("");
      this.cols.getList(dbName, function(list) {
        _.each(_.pluck(list, "name"), function(c) {
          selCol.append("<option id=\"" + c + "\">" + c + "</option>");
        });
        var colToSel = $("#" + colName, selCol);
        if (colToSel.length === 1) {
          colToSel.prop("selected", true);
        }
        self.updateShards();
      });
    },

    updateShards: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      var colName = $("#selectCol").find(":selected").attr("id");
      this.shards.getList(dbName, colName, function(list) {
        $(".shardCounter").html("0");
        _.each(list, function(s) {
          $("#" + s.server + "Shards").html(s.shards.length);
        });
      });
    },

    updateServerStatus: function(nextStep) {
      var self = this;
      var callBack = function(cls, stat, serv) {
        var id = serv,
          type, icon;
        id = id.replace(/\./g,'-');
        id = id.replace(/\:/g,'_');
        icon = $("#id" + id);
        if (icon.length < 1) {
          // callback after view was unrendered
          return;
        }
        type = icon.attr("class").split(/\s+/)[1];
        icon.attr("class", cls + " " + type + " " + stat);
        if (cls === "coordinator") {
          if (stat === "success") {
            $(".button-gui", icon.closest(".tile")).toggleClass("button-gui-disabled", false);
          } else {
            $(".button-gui", icon.closest(".tile")).toggleClass("button-gui-disabled", true);
          }

        }
      };
      this.coordinators.getStatuses(callBack.bind(this, "coordinator"), function() {
        self.dbservers.getStatuses(callBack.bind(self, "dbserver"));
        nextStep();
      });
    },

    updateDBDetailList: function() {
      var self = this;
      var selDB = $("#selectDB");
      var dbName = selDB.find(":selected").attr("id");
      selDB.html("");
      this.dbs.getList(function(dbList) {
        _.each(_.pluck(dbList, "name"), function(c) {
          selDB.append("<option id=\"" + c + "\">" + c + "</option>");
        });
        var dbToSel = $("#" + dbName, selDB);
        if (dbToSel.length === 1) {
          dbToSel.prop("selected", true);
        }
        self.updateCollections();
      });
    },

    rerender : function() {
      var self = this;
      this.updateServerStatus(function() {
        self.getServerStatistics(function() {
          self.updateServerTime();
          self.data = self.generatePieData();
          self.renderPieChart(self.data);
          self.renderLineChart();
          self.updateDBDetailList();
        });
      });
    },

    render: function() {
      this.knownServers = [];
      delete this.hist;
      var self = this;
      this.listByAddress(function(byAddress) {
        if (Object.keys(byAddress).length === 1) {
          self.type = "testPlan";
        } else {
          self.type = "other";
        }
        self.updateDBDetailList();
        self.dbs.getList(function(dbList) {
          $(self.el).html(self.template.render({
            dbs: _.pluck(dbList, "name"),
            byAddress: byAddress,
            type: self.type
          }));
          $(self.el).append(self.modal.render({}));
          self.replaceSVGs();
          /* this.loadHistory(); */
          self.getServerStatistics(function() {
            self.data = self.generatePieData();
            self.renderPieChart(self.data);
            self.renderLineChart();
            self.updateDBDetailList();
            self.startUpdating();
          });
        });
      });
    },

    generatePieData: function() {
      var pieData = [];
      var self = this;

      this.data.forEach(function(m) {
        pieData.push({key: m.get("name"), value: m.get("system").virtualSize,
          time: self.serverTime});
      });

      return pieData;
    },

    /*
     loadHistory : function() {
       this.hist = {};

       var self = this;
       var coord = this.coordinators.findWhere({
         status: "ok"
       });

       var endpoint = coord.get("protocol")
       + "://"
       + coord.get("address");

       this.dbservers.forEach(function (dbserver) {
         if (dbserver.get("status") !== "ok") {return;}

         if (self.knownServers.indexOf(dbserver.id) === -1) {
           self.knownServers.push(dbserver.id);
         }

         var server = {
           raw: dbserver.get("address"),
           isDBServer: true,
           target: encodeURIComponent(dbserver.get("name")),
           endpoint: endpoint,
           addAuth: window.App.addAuth.bind(window.App)
         };
       });

       this.coordinators.forEach(function (coordinator) {
         if (coordinator.get("status") !== "ok") {return;}

         if (self.knownServers.indexOf(coordinator.id) === -1) {
           self.knownServers.push(coordinator.id);
         }

         var server = {
           raw: coordinator.get("address"),
           isDBServer: false,
           target: encodeURIComponent(coordinator.get("name")),
           endpoint: coordinator.get("protocol") + "://" + coordinator.get("address"),
           addAuth: window.App.addAuth.bind(window.App)
         };
       });
     },
     */

    addStatisticsItem: function(name, time, requests, snap) {
      var self = this;

      if (! self.hasOwnProperty('hist')) {
        self.hist = {};
      }

      if (! self.hist.hasOwnProperty(name)) {
        self.hist[name] = [];
      }

      var h = self.hist[name];
      var l = h.length;

      if (0 === l) {
        h.push({
          time: time,
          snap: snap,
          requests: requests,
          requestsPerSecond: 0
        });
      }
      else {
        var lt = h[l - 1].time;
        var tt = h[l - 1].requests;

        if (tt < requests) {
          var dt = time - lt;
          var ps = 0;

          if (dt > 0) {
            ps = (requests - tt) / dt;
          }

          h.push({
            time: time,
            snap: snap,
            requests: requests,
            requestsPerSecond: ps
          });
        }
        /*
        else {
          h.times.push({
            time: time,
            snap: snap,
            requests: requests,
            requestsPerSecond: 0
          });
        }
        */
      }
    },

    getServerStatistics: function(nextStep) {
      var self = this;
      var snap = Math.round(self.serverTime / 1000);

      this.data = undefined;

      var statCollect = new window.ClusterStatisticsCollection();
      var coord = this.coordinators.first();

      // create statistics collector for DB servers
      this.dbservers.forEach(function (dbserver) {
        if (dbserver.get("status") !== "ok") {return;}

        if (self.knownServers.indexOf(dbserver.id) === -1) {
          self.knownServers.push(dbserver.id);
        }

        var stat = new window.Statistics({name: dbserver.id});

        stat.url = coord.get("protocol") + "://"
        + coord.get("address")
        + "/_admin/clusterStatistics?DBserver="
        + dbserver.get("name");

        statCollect.add(stat);
      });

      // create statistics collector for coordinator
      this.coordinators.forEach(function (coordinator) {
        if (coordinator.get("status") !== "ok") {return;}

        if (self.knownServers.indexOf(coordinator.id) === -1) {
          self.knownServers.push(coordinator.id);
        }

        var stat = new window.Statistics({name: coordinator.id});

        stat.url = coordinator.get("protocol") + "://"
        + coordinator.get("address")
        + "/_admin/statistics";

        statCollect.add(stat);
      });

      var cbCounter = statCollect.size();

      this.data = [];

      var successCB = function(m) {
        cbCounter--;
        var time = m.get("time");
        var name = m.get("name");
        var requests = m.get("http").requestsTotal;

        self.addStatisticsItem(name, time, requests, snap);
        self.data.push(m);
        if (cbCounter === 0) {
          nextStep();
        }
      };
      var errCB = function() {
        cbCounter--;
        if (cbCounter === 0) {
          nextStep();
        }
      };
      // now fetch the statistics
      statCollect.fetch(successCB, errCB);
    },

    renderPieChart: function(dataset) {
      var w = $("#clusterGraphs svg").width();
      var h = $("#clusterGraphs svg").height();
      var radius = Math.min(w, h) / 2; //change 2 to 1.4. It's hilarious.
      // var color = d3.scale.category20();
      var color = this.dygraphConfig.colors;

      var arc = d3.svg.arc() //each datapoint will create one later.
      .outerRadius(radius - 20)
      .innerRadius(0);
      var pie = d3.layout.pie()
      .sort(function (d) {
        return d.value;
      })
      .value(function (d) {
        return d.value;
      });
      d3.select("#clusterGraphs").select("svg").remove();
      var pieChartSvg = d3.select("#clusterGraphs").append("svg")
      // .attr("width", w)
      // .attr("height", h)
      .attr("class", "clusterChart")
      .append("g") //someone to transform. Groups data.
      .attr("transform", "translate(" + w / 2 + "," + ((h / 2) - 10) + ")");

    var arc2 = d3.svg.arc()
    .outerRadius(radius-2)
    .innerRadius(radius-2);
    var slices = pieChartSvg.selectAll(".arc")
    .data(pie(dataset))
    .enter().append("g")
    .attr("class", "slice");
        slices.append("path")
    .attr("d", arc)
    .style("fill", function (item, i) {
      return color[i % color.length];
    })
    .style("stroke", function (item, i) {
      return color[i % color.length];
    });
        slices.append("text")
    .attr("transform", function(d) { return "translate(" + arc.centroid(d) + ")"; })
    // .attr("dy", "0.35em")
    .style("text-anchor", "middle")
    .text(function(d) {
      var v = d.data.value / 1024 / 1024 / 1024;
      return v.toFixed(2); });

    slices.append("text")
    .attr("transform", function(d) { return "translate(" + arc2.centroid(d) + ")"; })
    // .attr("dy", "1em")
    .style("text-anchor", "middle")
    .text(function(d) { return d.data.key; });
  },

  renderLineChart: function() {
    var self = this;

    var interval = 60 * 20;
    var data = [];
    var hash = [];
    var t = Math.round(new Date().getTime() / 1000) - interval;
    var ks = self.knownServers;
    var f = function() {
      return null;
    };

    var d, h, i, j, tt, snap;

    for (i = 0;  i < ks.length;  ++i) {
      h = self.hist[ks[i]];

      if (h) {
        for (j = 0;  j < h.length;  ++j) {
          snap = h[j].snap;

          if (snap < t) {
            continue;
          }

          if (! hash.hasOwnProperty(snap)) {
            tt = new Date(snap * 1000);

            d = hash[snap] = [ tt ].concat(ks.map(f));
          }
          else {
            d = hash[snap];
          }

          d[i + 1] = h[j].requestsPerSecond;
        }
      }
    }

    data = [];

    Object.keys(hash).sort().forEach(function (m) {
      data.push(hash[m]);
    });

    var options = this.dygraphConfig.getDefaultConfig('clusterRequestsPerSecond');
    options.labelsDiv = $("#lineGraphLegend")[0];
    options.labels = [ "datetime" ].concat(ks);

    self.graph = new Dygraph(
      document.getElementById('lineGraph'),
      data,
      options
    );
  },

  stopUpdating: function () {
    window.clearTimeout(this.timer);
    delete this.graph;
    this.isUpdating = false;
  },

  startUpdating: function () {
    if (this.isUpdating) {
      return;
    }

    this.isUpdating = true;

    var self = this;

    this.timer = window.setInterval(function() {
      self.rerender();
    }, this.interval);
  },


  dashboard: function(e) {
    this.stopUpdating();

    var tar = $(e.currentTarget);
    var serv = {};
    var cur;
    var coord;

    var ip_port = tar.attr("id");
    ip_port = ip_port.replace(/\-/g,'.');
    ip_port = ip_port.replace(/\_/g,':');
    ip_port = ip_port.substr(2);

    serv.raw = ip_port;
    serv.isDBServer = tar.hasClass("dbserver");

    if (serv.isDBServer) {
      cur = this.dbservers.findWhere({
        address: serv.raw
      });
      coord = this.coordinators.findWhere({
        status: "ok"
      });
      serv.endpoint = coord.get("protocol")
      + "://"
      + coord.get("address");
    }
    else {
      cur = this.coordinators.findWhere({
        address: serv.raw
      });
      serv.endpoint = cur.get("protocol")
      + "://"
      + cur.get("address");
    }

    serv.target = encodeURIComponent(cur.get("name"));
    window.App.serverToShow = serv;
    window.App.dashboard();
  },

  getCurrentSize: function (div) {
    if (div.substr(0,1) !== "#") {
      div = "#" + div;
    }
    var height, width;
    $(div).attr("style", "");
    height = $(div).height();
    width = $(div).width();
    return {
      height: height,
      width: width
    };
  },

  resize: function () {
    var dimensions;
    if (this.graph) {
      dimensions = this.getCurrentSize(this.graph.maindiv_.id);
      this.graph.resize(dimensions.width, dimensions.height);
    }
  }
});
}());

/*global window, $, Backbone, templateEngine, _, alert */

(function() {
  "use strict";

  window.ShowShardsView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("showShards.ejs"),

    events: {
      "change #selectDB" : "updateCollections",
      "change #selectCol" : "updateShards"
    },

    initialize: function() {
      this.dbservers = new window.ClusterServers([], {
        interval: 10000
      });
      this.dbservers.fetch({
        async : false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      this.dbs = new window.ClusterDatabases([], {
        interval: 10000
      });
      this.cols = new window.ClusterCollections();
      this.shards = new window.ClusterShards();
    },

    updateCollections: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      $("#selectCol").html("");
      _.each(_.pluck(this.cols.getList(dbName), "name"), function(c) {
        $("#selectCol").append("<option id=\"" + c + "\">" + c + "</option>");
      });
      this.updateShards();
    },

    updateShards: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      var colName = $("#selectCol").find(":selected").attr("id");
      var list = this.shards.getList(dbName, colName);
      $(".shardContainer").empty();
      _.each(list, function(s) {
        var item = $("#" + s.server + "Shards");
        $(".collectionName", item).html(s.server + ": " + s.shards.length);
        /* Will be needed in future
        _.each(s.shards, function(shard) {
          var shardIcon = document.createElement("span");
          shardIcon = $(shardIcon);
          shardIcon.toggleClass("fa");
          shardIcon.toggleClass("fa-th");
          item.append(shardIcon);
        });
        */
      });
    },

    render: function() {
      $(this.el).html(this.template.render({
        names: this.dbservers.pluck("name"),
        dbs: _.pluck(this.dbs.getList(), "name")
      }));
      this.updateCollections();
    }
  });

}());

/*global Backbone, templateEngine, $, window*/
(function () {
  "use strict";
  window.ShutdownButtonView = Backbone.View.extend({
    el: '#navigationBar',

    events: {
      "click #clusterShutdown"  : "clusterShutdown"
    },

    initialize: function() {
      this.overview = this.options.overview;
    },

    template: templateEngine.createTemplate("shutdownButtonView.ejs"),

    clusterShutdown: function() {
      this.overview.stopUpdating();
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster is shutting down');
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/shutdown",
        success: function(data) {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.navigate("handleClusterDown", {trigger: true});
        }
      });
    },

    render: function () {
      $(this.el).html(this.template.render({}));
      return this;
    },

    unrender: function() {
      $(this.el).html("");
    }
  });
}());


/*global window, $, Backbone, document, arangoCollectionModel,arangoHelper,
arangoDatabase, btoa, _*/

(function() {
  "use strict";

  window.ClusterRouter = Backbone.Router.extend({

    routes: {
      ""                       : "initialRoute",
      "planScenario"           : "planScenario",
      "planTest"               : "planTest",
      "planAsymmetrical"       : "planAsymmetric",
      "shards"                 : "showShards",
      "showCluster"            : "showCluster",
      "handleClusterDown"      : "handleClusterDown"
    },

    // Quick fix for server authentication
    addAuth: function (xhr) {
      var u = this.clusterPlan.get("user");
      if (!u) {
        xhr.abort();
        if (!this.isCheckingUser) {
          this.requestAuth();
        }
        return;
      }
      var user = u.name;
      var pass = u.passwd;
      var token = user.concat(":", pass);
      xhr.setRequestHeader('Authorization', "Basic " + btoa(token));
    },

    requestAuth: function() {
      this.isCheckingUser = true;
      this.clusterPlan.set({"user": null});
      var modalLogin = new window.LoginModalView();
      modalLogin.render();
    },

    getNewRoute: function(last) {
      if (last === "statistics") {
        return this.clusterPlan.getCoordinator()
          + "/_admin/"
          + last;
      }
      return this.clusterPlan.getCoordinator()
        + "/_admin/aardvark/cluster/"
        + last;
    },

    initialRoute: function() {
      this.initial();
    },

    updateAllUrls: function() {
      _.each(this.toUpdate, function(u) {
        u.updateUrl();
      });
    },

    registerForUpdate: function(o) {
      this.toUpdate.push(o);
      o.updateUrl();
    },

    initialize: function () {
      this.footerView = new window.FooterView();
      this.footerView.render();

      var self = this;
      this.dygraphConfig = window.dygraphConfig;
      window.modalView = new window.ModalView();
      this.initial = this.planScenario;
      this.isCheckingUser = false;
      this.bind('all', function(trigger, args) {
        var routeData = trigger.split(":");
        if (trigger === "route") {
          if (args !== "showCluster") {
            if (self.showClusterView) {
              self.showClusterView.stopUpdating();
              self.shutdownView.unrender();
            }
            if (self.dashboardView) {
              self.dashboardView.stopUpdating();
            }
          }
        }
      });
      this.toUpdate = [];
      this.clusterPlan = new window.ClusterPlan();
      this.clusterPlan.fetch({
        async: false
      });
      $(window).resize(function() {
        self.handleResize();
      });
    },

    showCluster: function() {
      if (!this.showClusterView) {
        this.showClusterView = new window.ShowClusterView(
            {dygraphConfig : this.dygraphConfig}
      );
      }
      if (!this.shutdownView) {
        this.shutdownView = new window.ShutdownButtonView({
          overview: this.showClusterView
        });
      }
      this.shutdownView.render();
      this.showClusterView.render();
    },

    showShards: function() {
      if (!this.showShardsView) {
        this.showShardsView = new window.ShowShardsView();
      }
      this.showShardsView.render();
    },

    handleResize: function() {
      if (this.dashboardView) {
        this.dashboardView.resize();
      }
      if (this.showClusterView) {
        this.showClusterView.resize();
      }
    },

    planTest: function() {
      if (!this.planTestView) {
        this.planTestView = new window.PlanTestView(
          {model : this.clusterPlan}
        );
      }
      this.planTestView.render();
    },

    planAsymmetric: function() {
      if (!this.planSymmetricView) {
        this.planSymmetricView = new window.PlanSymmetricView(
          {model : this.clusterPlan}
        );
      }
      this.planSymmetricView.render(false);
    },

    planScenario: function() {
      if (!this.planScenarioSelector) {
        this.planScenarioSelector = new window.PlanScenarioSelectorView();
      }
      this.planScenarioSelector.render();
    },

    handleClusterDown : function() {
      if (!this.clusterDownView) {
        this.clusterDownView = new window.ClusterDownView();
      }
      this.clusterDownView.render();
    },

    dashboard: function() {
      var server = this.serverToShow;
      if (!server) {
        this.navigate("", {trigger: true});
        return;
      }

      server.addAuth = this.addAuth.bind(this);
      this.dashboardView = new window.ServerDashboardView({
        dygraphConfig: this.dygraphConfig,
        serverToShow : this.serverToShow,
        database: {
          hasSystemAccess: function() {
            return true;
          }
        }
      });
      this.dashboardView.render();
    },

    clusterUnreachable: function() {
      if (this.showClusterView) {
        this.showClusterView.stopUpdating();
        this.shutdownView.unrender();
      }
      if (!this.unreachableView) {
        this.unreachableView = new window.ClusterUnreachableView();
      }
      this.unreachableView.render();
    }

  });

}());

/*global window, $, Backbone, document */

(function() {
  "use strict";

  $.get("cluster/amIDispatcher", function(data) {
    if (!data) {
      var url = window.location.origin;
      url += window.location.pathname;
      url = url.replace("cluster", "index");
      window.location.replace(url);
    }
  });
  window.location.hash = "";
  $(document).ready(function() {
    window.App = new window.ClusterRouter();

    Backbone.history.start();

    if(window.App.clusterPlan.get("plan")) {
      if(window.App.clusterPlan.isAlive()) {
        window.App.initial = window.App.showCluster;
      } else {
        window.App.initial = window.App.handleClusterDown;
      }
    } else {
      window.App.initial = window.App.planScenario;
    }
    window.App.initialRoute();
    window.App.handleResize();
  });

}());
