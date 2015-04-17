/*
Plugin Name: amCharts Export
Description: Adds export capabilities to amCharts products
Author: Benjamin Maertz, amCharts
Version: 1.0
Author URI: http://www.amcharts.com/

Copyright 2015 amCharts

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Please note that the above license covers only this plugin. It by all means does
not apply to any other amCharts products that are covered by different licenses.
*/

AmCharts.addInitHandler( function( chart ) {
	var _this = {
		name: "export",
		version: "1.0",
		libs: {
			autoLoad: true,
			path: "./plugins/export/libs/",
			resources: [ {
				"pdfmake/pdfmake.js": [ "pdfmake/vfs_fonts.js" ],
				"jszip/jszip.js": [ "xlsx/xlsx.js" ]
			}, "fabric.js/fabric.js", "FileSaver.js/FileSaver.js" ],
			loaded: 0
		},
		config: {},
		setup: {},
		drawing: {
			enabled: false,
			actions: [ "undo", "redo", "done", "cancel" ],
			undos: [],
			undo: function() {
				var last = _this.drawing.undos.pop();

				if ( last ) {
					_this.drawing.redos.push( last );
					last.path.remove();
				}
			},
			redos: [],
			redo: function() {
				var last = _this.drawing.redos.pop();

				if ( last ) {
					_this.setup.fabric.add( last.path );
					_this.drawing.undos.push( last );
				}
			},
			done: function() {
				_this.drawing.enabled = false;
				_this.drawing.undos = [];
				_this.drawing.redos = [];
				_this.setup.fabric.renderAll();
				_this.setup.wrapper.setAttribute( "class", "amcharts-export-canvas" );
				_this.createMenu( _this.config.menu );
			}
		},
		defaults: {
			position: "top-right",
			fileName: "amCharts",
			action: "download",
			formats: {
				JPG: {
					mimeType: "image/jpg",
					extension: "jpg",
					capture: true
				},
				PNG: {
					mimeType: "image/png",
					extension: "png",
					capture: true
				},
				SVG: {
					mimeType: "text/xml",
					extension: "svg",
					capture: true
				},
				PDF: {
					mimeType: "application/pdf",
					extension: "pdf",
					capture: true
				},
				CSV: {
					mimeType: "text/plain",
					extension: "csv"
				},
				JSON: {
					mimeType: "text/plain",
					extension: "json"
				},
				XLSX: {
					mimeType: "application/octet-stream",
					extension: "xlsx"
				}
			},
			fabric: {
				backgroundColor: "#FFFFFF",
				isDrawingMode: false,
				selection: false,
				removeImages: true
			},
			pdfMake: {
				pageSize: "A4",
				pageOrientation: "portrait",
				images: {},
				content: [ {
					image: "reference",
					fit: [ 523.28, 769.89 ]
				} ]
			},
			menu: [ {
				class: "export-main",
				label: "Export",
				menu: [ {
					label: "Download as ...",
					menu: [ "PNG", "JPG", "SVG", {
						format: "PDF",
						content: [ "Saved from:", window.location.href, {
							image: "reference",
							fit: [ 523.28, 769.89 ] // fit image to A4
						} ]
					} ]
				}, {
					label: "Save data ...",
					menu: [ "CSV", "XLSX", "JSON" ]
				}, {
					label: "Annotate",
					action: "draw",
					menu: [ {
						class: "export-drawing",
						menu: [ {
							label: "Color ...",
							menu: [ {
								class: "export-drawing-color export-drawing-color-black",
								label: "Black",
								click: function() {
									this.setup.fabric.freeDrawingBrush.color = "#000";
								}
							}, {
								class: "export-drawing-color export-drawing-color-white",
								label: "White",
								click: function() {
									this.setup.fabric.freeDrawingBrush.color = "#fff";
								}
							}, {
								class: "export-drawing-color export-drawing-color-red",
								label: "Red",
								click: function() {
									this.setup.fabric.freeDrawingBrush.color = "#f00";
								}
							}, {
								class: "export-drawing-color export-drawing-color-green",
								label: "Green",
								click: function() {
									this.setup.fabric.freeDrawingBrush.color = "#0f0";
								}
							}, {
								class: "export-drawing-color export-drawing-color-blue",
								label: "Blue",
								click: function() {
									this.setup.fabric.freeDrawingBrush.color = "#00f";
								}
							} ]
						}, "UNDO", "REDO", {
							label: "Save as ...",
							menu: [ "PNG", "JPG", "SVG", {
								format: "PDF",
								content: [ "Saved from:", window.location.href, {
									image: "reference",
									fit: [ 523.28, 769.89 ] // fit image to A4
								} ]
							} ]
						}, {
							format: "PRINT",
							label: "Print"
						}, "CANCEL" ]
					} ]
				}, {
					format: "PRINT",
					label: "Print"
				} ]
			} ]
		},

		download: function( data, type, filename ) {
			var blob = _this.toBlob( {
				data: data,
				type: type
			}, function( data ) {
				if ( window.saveAs ) {
					saveAs( data, filename );
				} else {
					throw new Error( "Unable to create file. Ensure saveAs (FileSaver.js) is supported." );
				}
			} );

			return data;
		},

		loadResource: function( src, addons ) {
			var node, url = src.indexOf( "//" ) != -1 ? src : [ _this.libs.path, src ].join( "" );

			function callback() {
				_this.libs.loaded++;
				if ( addons ) {
					for ( i in addons ) {
						_this.loadResource( addons[ i ] );
					}
				}
			}

			if ( src.indexOf( ".js" ) != -1 ) {
				node = document.createElement( "script" );
				node.setAttribute( "type", "text/javascript" );
				node.setAttribute( "src", url );

			} else if ( src.indexOf( ".css" ) != -1 ) {
				node = document.createElement( "link" );
				node.setAttribute( "type", "text/css" );
				node.setAttribute( "rel", "stylesheet" );
				node.setAttribute( "href", url );
			}

			if ( node ) {
				node.addEventListener( "load", callback );
				document.head.appendChild( node );
			} else {
				callback();
			}
		},

		loadDependencies: function() {
			if ( !_this.libs.loaded && _this.libs.autoLoad ) {
				for ( i in _this.libs.resources ) {
					if ( typeof _this.libs.resources[ i ] == "object" ) {
						for ( i2 in _this.libs.resources[ i ] ) {
							var addons = _this.libs.resources[ i ][ i2 ];
							_this.loadResource( i2, addons );
						}
					} else {
						_this.loadResource( _this.libs.resources[ i ] );
					}
				}
			}
		},

		pxToNumber: function( attr ) {
			return Number( String( attr ).replace( "px", "" ) ) || 0;
		},

		deepMerge: function( a, b, overwrite ) {
			for ( i in b ) {
				var v = b[ i ];

				// NEW
				if ( a[ i ] == undefined || overwrite ) {
					if ( v instanceof Array ) {
						a[ i ] = new Array();
					} else if ( v instanceof Function ) {
						a[ i ] = new Function();
					} else if ( v instanceof Date ) {
						a[ i ] = new Date();
					} else if ( v instanceof Object ) {
						a[ i ] = new Object();
					} else if ( v instanceof Number ) {
						a[ i ] = new Number();
					} else if ( v instanceof String ) {
						a[ i ] = new String();
					}
				}

				if ( !( v instanceof Function || v instanceof Date ) && ( v instanceof Object || v instanceof Array ) ) {
					_this.deepMerge( a[ i ], v, overwrite );
				} else {
					if ( a instanceof Array && !overwrite ) {
						a.push( v );
					} else {
						a[ i ] = v;
					}
				}
			}
			return a;
		},

		// CAPTURE EMOTIONAL MOMENT
		capture: function( options, callback ) {
			var cfg = _this.deepMerge( _this.deepMerge( {}, _this.config.fabric ), options || {} );
			var i1, i2, i3 = 0;
			var groups = [];
			var offset = {
				x: 0,
				y: 0,
				width: _this.setup.chart.divRealWidth,
				height: _this.setup.chart.divRealHeight
			};

			// GATHER SVGs
			var svgs = _this.setup.chart.containerDiv.getElementsByTagName( "svg" );
			for ( i1 = 0; i1 < svgs.length; i1++ ) {
				var group = {
					svg: svgs[ i1 ],
					parent: svgs[ i1 ].parentNode,
					offset: {
						x: 0,
						y: 0
					},
					patterns: {},
					clippings: {}
				}

				// GATHER CLIPPATHS; make them invisible
				var items = svgs[ i1 ].getElementsByTagName( "clipPath" );
				for ( i2 = 0; i2 < items.length; i2++ ) {
					for ( i3 = 0; i3 < items[ i2 ].childNodes.length; i3++ ) {
						items[ i2 ].childNodes[ i3 ].setAttribute( "fill", "transparent" );
					}
					group.clippings[ items[ i2 ].id ] = items[ i2 ];
				}

				// GATHER PATTERNS
				var items = svgs[ i1 ].getElementsByTagName( "pattern" );
				for ( i2 = 0; i2 < items.length; i2++ ) {
					var props = {
						node: items[ i2 ],
						source: items[ i2 ].getAttribute( "xlink:href" ),
						width: Number( items[ i2 ].getAttribute( "width" ) ),
						height: Number( items[ i2 ].getAttribute( "height" ) ),
						repeat: "repeat"
					}

					// REPLACE SOURCE
					var rect = items[ i2 ].getElementsByTagName( "rect" );
					if ( rect.length > 0 ) {
						var IMG = new Image();
						IMG.src = props.source;

						var PSC = new fabric.StaticCanvas( undefined, {
							width: props.width,
							height: props.height,
							backgroundColor: rect[ 0 ].getAttribute( "fill" )
						} );

						var RECT = new fabric.Rect( {
							width: props.width,
							height: props.height,
							fill: new fabric.Pattern( {
								source: IMG,
								repeat: "repeat"
							} )
						} );
						PSC.add( RECT );
						props.source = PSC.toDataURL();
					}

					// BUFFER PATTERN
					group.patterns[ items[ i2 ].id ] = new fabric.Pattern( props );
				}

				// APPEND GROUP
				groups.push( group );
			}

			// GATHER EXTERNAL LEGEND
			if ( _this.config.legend && _this.setup.chart.legend && _this.setup.chart.legend.position == "outside" ) {
				var group = {
					svg: _this.setup.chart.legend.container.container,
					parent: _this.setup.chart.legend.container.div,
					offset: {
						x: 0,
						y: 0
					},
					legend: {
						type: [ "top", "left" ].indexOf( _this.config.legend.position ) != -1 ? "unshift" : "push",
						position: _this.config.legend.position,
						width: _this.config.legend.width ? _this.config.legend.width : _this.setup.chart.legend.container.width,
						height: _this.config.legend.height ? _this.config.legend.height : _this.setup.chart.legend.container.height
					}
				}

				// Adapt canvas dimensions
				if ( [ "left", "right" ].indexOf( group.legend.position ) != -1 ) {
					offset.width += group.legend.width;
					offset.height = group.legend.height > offset.height ? group.legend.height : offset.height;
				} else if ( [ "top", "bottom" ].indexOf( group.legend.position ) != -1 ) {
					offset.height += group.legend.height;
				}

				// PRE/APPEND SVG
				groups[ group.legend.type ]( group );
			}

			// STOCK CHART
			if ( _this.setup.chart.type == "stock" ) {
				if ( _this.setup.chart.leftContainer ) {
					offset.width -= _this.pxToNumber( _this.setup.chart.leftContainer.style.width );
					_this.setup.wrapper.style.paddingLeft = _this.pxToNumber( _this.setup.chart.leftContainer.style.width ) + _this.setup.chart.panelsSettings.panelSpacing * 2;
				}
				if ( _this.setup.chart.rightContainer ) {
					offset.width -= _this.pxToNumber( _this.setup.chart.rightContainer.style.width );
					_this.setup.wrapper.style.paddingRight = _this.pxToNumber( _this.setup.chart.rightContainer.style.width ) + _this.setup.chart.panelsSettings.panelSpacing * 2;
				}
				if ( _this.setup.chart.periodSelector && [ "top", "bottom" ].indexOf( _this.setup.chart.periodSelector.position ) != -1 ) {
					offset.height -= _this.setup.chart.periodSelector.offsetHeight + _this.setup.chart.panelsSettings.panelSpacing;
				}
				if ( _this.setup.chart.dataSetSelector && [ "top", "bottom" ].indexOf( _this.setup.chart.dataSetSelector.position ) != -1 ) {
					offset.height -= _this.setup.chart.dataSetSelector.offsetHeight;
				}
			}

			// CLEAR IF EXIST
			_this.drawing.enabled = cfg.isDrawingMode = ( cfg.drawing && cfg.drawing.enabled ) ? true : cfg.action == "draw";

			if ( !_this.setup.wrapper ) {
				_this.setup.wrapper = document.createElement( "div" );
				_this.setup.wrapper.setAttribute( "class", "amcharts-export-canvas" );
				_this.setup.wrapper.appendChild( _this.setup.canvas );
			} else {
				_this.setup.wrapper.innerHTML = "";
			}

			_this.setup.canvas = document.createElement( "canvas" );
			_this.setup.wrapper.appendChild( _this.setup.canvas );
			_this.setup.fabric = new fabric.Canvas( _this.setup.canvas, _this.deepMerge( {
				width: offset.width,
				height: offset.height
			}, cfg ) );

			_this.deepMerge( _this.setup.fabric, cfg );

			// OBSERVE MOUSE
			_this.setup.fabric.on( "path:created", function( path ) {
				_this.drawing.undos.push( path );
			} );

			// DRAWING
			if ( _this.drawing.enabled ) {
				_this.setup.wrapper.setAttribute( "class", "amcharts-export-canvas active" );
			} else {
				_this.setup.wrapper.setAttribute( "class", "amcharts-export-canvas" );
			}

			for ( i1 = 0; i1 < groups.length; i1++ ) {
				var group = groups[ i1 ];

				// GATHER POSITION
				if ( group.parent.style.top || group.parent.style.left ) {
					group.offset.y = _this.pxToNumber( group.parent.style.top );
					group.offset.x = _this.pxToNumber( group.parent.style.left );
				} else {
					// EXTERNAL LEGEND
					if ( group.legend ) {
						if ( group.legend.position == "left" ) {
							offset.x += chart.legend.container.width;
						} else if ( group.legend.position == "right" ) {
							group.offset.x += offset.width - group.legend.width;
						} else if ( group.legend.position == "top" ) {
							offset.y += group.legend.height;
						} else if ( group.legend.position == "bottom" ) {
							group.offset.y += offset.height - group.legend.height; // offset.y
						}

						// NORMAL
					} else {
						group.offset.x = offset.x;
						group.offset.y = offset.y;
						offset.y += _this.pxToNumber( group.parent.style.height );
					}

					// PANEL
					if ( group.parent && ( group.parent.getAttribute( "class" ) || "" ).split( " " ).indexOf( "amChartsLegend" ) != -1 ) {
						offset.y += _this.pxToNumber( group.parent.parentNode.parentNode.style.marginTop );
						group.offset.y += _this.pxToNumber( group.parent.parentNode.parentNode.style.marginTop );
					}
				}

				// ADD TO CANVAS
				fabric.parseSVGDocument( group.svg, ( function( group ) {
					return function( objects, options ) {
						var g = fabric.util.groupSVGElements( objects, options );
						var tmp = {
							top: group.offset.y,
							left: group.offset.x
						};

						for ( i1 in g.paths ) {

							// OPACITY; TODO: Distinguish opacity types
							if ( g.paths[ i1 ] ) {

								// CHECK ORIGIN; REMOVE TAINTED
								if ( cfg.removeImages && g.paths[ i1 ][ "xlink:href" ] && g.paths[ i1 ][ "xlink:href" ].indexOf( location.origin ) == -1 ) {
									g.paths.splice( i1, 1 );
								}

								// SET OPACITY
								if ( g.paths[ i1 ].fill instanceof Object ) {

									// MISINTERPRETATION OF FABRIC
									if ( g.paths[ i1 ].fill.type == "radial" ) {
										g.paths[ i1 ].fill.coords.r2 = g.paths[ i1 ].fill.coords.r1 * -1;
										g.paths[ i1 ].fill.coords.r1 = 0;
									}

									g.paths[ i1 ].set( {
										opacity: g.paths[ i1 ].fillOpacity
									} );

									// PATTERN; TODO: Distinguish opacity types
								} else if ( String( g.paths[ i1 ].fill ).slice( 0, 3 ) == "url" ) {
									var PID = g.paths[ i1 ].fill.slice( 5, -1 );
									if ( group.patterns[ PID ] ) {
										g.paths[ i1 ].set( {
											fill: group.patterns[ PID ],
											opacity: g.paths[ i1 ].fillOpacity
										} );
									}
								}
								if ( String( g.paths[ i1 ].clipPath ).slice( 0, 3 ) == "url" ) {
									var PID = g.paths[ i1 ].clipPath.slice( 5, -1 );

									if ( group.clippings[ PID ] ) {
										var mask = group.clippings[ PID ].childNodes[ 0 ];
										var transform = g.paths[ i1 ].svg.getAttribute( "transform" ) || "translate(0,0)";

										transform = transform.slice( 10, -1 ).split( "," );

										g.paths[ i1 ].set( {
											clipTo: ( function( mask, transform ) {
												return function( ctx ) {
													var width = Number( mask.getAttribute( "width" ) || "0" );
													var height = Number( mask.getAttribute( "height" ) || "0" );
													var x = Number( mask.getAttribute( "x" ) || "0" );
													var y = Number( mask.getAttribute( "y" ) || "0" );

													ctx.rect( Number( transform[ 0 ] ) * -1 + x, Number( transform[ 1 ] ) * -1 + y, width, height );
												}
											} )( mask, transform )
										} );
									}
								}
							}
						}

						g.set( tmp );

						_this.setup.fabric.add( g );

						// ADD BALLOONS
						var balloons = group.svg.parentNode.getElementsByClassName( "amcharts-balloon-div" );
						for ( i = 0; i < balloons.length; i++ ) {
							if ( cfg.balloonFunction instanceof Function ) {
								cfg.balloonFunction.apply( _this, [ balloons[ i ], group ] );
							} else {
								var parent = balloons[ i ];
								var text = parent.childNodes[ 0 ];
								var label = new fabric.Text( text.innerText || text.innerHTML, {
									fontSize: _this.pxToNumber( text.style.fontSize ),
									fontFamily: text.style.fontFamily,
									fill: text.style.color,
									top: _this.pxToNumber( parent.style.top ) + group.offset.y,
									left: _this.pxToNumber( parent.style.left ) + group.offset.x
								} );

								_this.setup.fabric.add( label );
							}
						}
						if ( group.svg.nextSibling && group.svg.nextSibling.tagName == "A" ) {
							var label = new fabric.Text( group.svg.nextSibling.innerText || group.svg.nextSibling.innerHTML, {
								fontSize: _this.pxToNumber( group.svg.nextSibling.style.fontSize ),
								fontFamily: group.svg.nextSibling.style.fontFamily,
								fill: group.svg.nextSibling.style.color,
								top: _this.pxToNumber( group.svg.nextSibling.style.top ) + group.offset.y,
								left: _this.pxToNumber( group.svg.nextSibling.style.left ) + group.offset.x
							} );
							_this.setup.fabric.add( label );
						}

						groups.pop();

						if ( !groups.length ) {
							_this.handleCallback( callback );
						}
					}

					// Identify elements through classnames
				} )( group ), function( svg, obj ) {
					var className = svg.getAttribute( "class" ) || svg.parentNode.getAttribute( "class" ) || "";
					var visibility = svg.getAttribute( "visibility" ) || svg.parentNode.getAttribute( "visibility" ) || svg.parentNode.parentNode.getAttribute( "visibility" ) || "";
					var clipPath = svg.getAttribute( "clip-path" ) || svg.parentNode.getAttribute( "clip-path" ) || "";

					obj.className = className;
					obj.clipPath = clipPath;
					obj.svg = svg;

					// HIDE HIDDEN ELEMENTS; TODO: Find a better way to handle that
					if ( visibility == "hidden" ) {
						obj.opacity = 0;

						// WALKTHROUGH ELEMENTS
					} else {

						// TRANSPORT FILL/STROKE OPACITY
						var attrs = [ "fill", "stroke" ];
						for ( i1 in attrs ) {
							var attr = attrs[ i1 ]
							var attrVal = String( svg.getAttribute( attr ) || "" );
							var attrOpacity = Number( svg.getAttribute( attr + "-opacity" ) || "1" );
							var attrRGBA = fabric.Color.fromHex( attrVal ).getSource();

							// EXCEPTION
							if ( obj.className == "amcharts-guide-fill" && !attrVal ) {
								attrOpacity = 0;
								attrRGBA = fabric.Color.fromHex( "#000000" ).getSource();
							}

							if ( attrRGBA ) {
								attrRGBA.pop();
								attrRGBA.push( attrOpacity )
								obj[ attr ] = "rgba(" + attrRGBA.join() + ")";
								obj[ _this.capitalize( attr + "-opacity" ) ] = attrOpacity;
							}
						}
					}
				} );
			}
		},

		toCanvas: function( options, callback ) {
			var cfg = _this.deepMerge( {
				// Nuffin
			}, options || {} );
			var data = _this.setup.canvas;

			_this.handleCallback( callback, data );

			return data;
		},

		toBlob: function( options, callback ) {
			var cfg = _this.deepMerge( {
				data: "empty",
				type: "text/plain"
			}, options || {} );
			var isBase64 = /^data:.+;base64,(.*)$/.exec( cfg.data );

			// GATHER BODY
			if ( isBase64 ) {
				cfg.data = isBase64[ 0 ];
				cfg.type = cfg.data.slice( 5, cfg.data.indexOf( "," ) - 7 );
				cfg.data = _this.toByteArray( {
					data: cfg.data.slice( cfg.data.indexOf( "," ) + 1, cfg.data.length )
				} );
			}

			if ( cfg.getByteArray ) {
				data = cfg.data;
			} else {
				data = new Blob( [ cfg.data ], {
					type: cfg.type
				} );
			}

			_this.handleCallback( callback, data );

			return data;
		},

		toJPG: function( options, callback ) {
			var cfg = _this.deepMerge( {
				format: "jpeg",
				quality: 1,
				multiplier: 1,
				background: "#FF00FF"
			}, options || {} );
			var data = _this.setup.fabric.toDataURL( cfg );

			_this.handleCallback( callback, data );

			return data;
		},

		toPNG: function( options, callback ) {
			var cfg = _this.deepMerge( {
				format: "png",
				quality: 1,
				multiplier: 1
			}, options || {} );
			var data = _this.setup.fabric.toDataURL( cfg );

			_this.handleCallback( callback, data );

			return data;
		},

		toSVG: function( options, callback ) {
			var cfg = _this.deepMerge( {
				// nothing in here
			}, options || {} );
			var data = _this.setup.fabric.toSVG( cfg );

			_this.handleCallback( callback, data );

			return data;
		},

		toPDF: function( options, callback ) {
			var cfg = _this.deepMerge( _this.deepMerge( {
				multiplier: 2
			}, _this.config.pdfMake ), options || {}, true );
			cfg.images.reference = _this.toPNG( cfg );

			var data = new pdfMake.createPdf( cfg );

			if ( callback ) {
				data.getDataUrl( ( function( callback ) {
					return function() {
						callback.apply( _this, arguments );
					}
				} )( callback ) );
			}

			return data;
		},

		toPRINT: function( options, callback ) {
			var cfg = _this.deepMerge( {
				// nothing in here
			}, options || {} );
			var data = _this.toPNG( cfg );
			var states = [];
			var items = document.body.childNodes;
			var img = document.createElement( "img" );

			img.src = data;
			img.setAttribute( "style", "width: 100%; max-height: 100%;" );

			for ( i in items ) {
				if ( items[ i ].nodeType === 1 ) {
					states[ i ] = items[ i ].style.display;
					items[ i ].style.display = "none";
				}
			}

			document.body.appendChild( img );
			window.print();

			for ( i in items ) {
				if ( items[ i ].nodeType === 1 ) {
					items[ i ].style.display = states[ i ];
				}
			}
			img.remove();

			_this.handleCallback( callback, data );

			return true;
		},

		toJSON: function( options, callback ) {
			var cfg = _this.deepMerge( {
				data: _this.getChartData()
			}, options || {}, true );
			var data = JSON.stringify( cfg.data, undefined, "\t" );

			_this.handleCallback( callback, data );

			return data;
		},

		toCSV: function( options, callback ) {
			var cfg = _this.deepMerge( {
				data: _this.getChartData(),
				delimiter: ",",
				quotes: true,
				escape: true,
				dateFields: [],
				dateFormat: _this.setup.chart.dataDateFormat || "YYYY-MM-DD"
			}, options || {}, true );
			var data = "";

			if ( _this.setup.chart.categoryAxis && _this.setup.chart.categoryAxis.parseDates && _this.setup.chart.categoryField ) {
				cfg.dateFields.push( _this.setup.chart.categoryField );
			}

			for ( row in cfg.data ) {
				var buffer = [];
				for ( col in cfg.data[ row ] ) {
					var value = cfg.data[ row ][ col ];

					// HEADER
					if ( row == 0 ) {
						value = col;

						// BODY
					} else {
						if ( typeof value === "string" ) {
							value = value;
						} else if ( cfg.dateFormat && value instanceof Date && cfg.dateFields.indexOf( col ) != -1 ) {
							value = AmCharts.formatDate( value, cfg.dateFormat );
						}
					}

					// WRAP IN QUOTES
					if ( typeof value === "string" ) {
						if ( cfg.escape ) {
							value = value.replace( '"', '""' );
						}
						if ( cfg.quotes ) {
							value = [ '"', value, '"' ].join( "" );
						}
					}

					buffer.push( value );
				}
				data += buffer.join( cfg.delimiter ) + "\n";
			}

			_this.handleCallback( callback, data );

			return data;
		},

		toXLSX: function( options, callback ) {
			var cfg = _this.deepMerge( {
				data: _this.getChartData(),
				name: "amCharts",
				withHeader: true
			}, options || {}, true );
			var data = "";
			var wb = {
				SheetNames: [],
				Sheets: {}
			}

			function datenum( v, date1904 ) {
				if ( date1904 ) v += 1462;
				var epoch = Date.parse( v );
				return ( epoch - new Date( Date.UTC( 1899, 11, 30 ) ) ) / ( 24 * 60 * 60 * 1000 );
			}

			function sheet_from_array_of_arrays( data, opts ) {
				var ws = {};
				var range = {
					s: {
						c: 10000000,
						r: 10000000
					},
					e: {
						c: 0,
						r: 0
					}
				};
				for ( var R = 0; R != data.length; ++R ) {
					for ( var C = 0; C != data[ R ].length; ++C ) {
						if ( range.s.r > R ) range.s.r = R;
						if ( range.s.c > C ) range.s.c = C;
						if ( range.e.r < R ) range.e.r = R;
						if ( range.e.c < C ) range.e.c = C;
						var cell = {
							v: data[ R ][ C ]
						};
						if ( cell.v == null ) continue;
						var cell_ref = XLSX.utils.encode_cell( {
							c: C,
							r: R
						} );

						if ( typeof cell.v === "number" ) cell.t = "n";
						else if ( typeof cell.v === "boolean" ) cell.t = "b";
						else if ( cell.v instanceof Date ) {
							cell.t = "n";
							cell.z = XLSX.SSF._table[ 14 ];
							cell.v = datenum( cell.v );
						} else cell.t = "s";

						ws[ cell_ref ] = cell;
					}
				}
				if ( range.s.c < 10000000 ) ws[ "!ref" ] = XLSX.utils.encode_range( range );
				return ws;
			}

			wb.SheetNames.push( cfg.name );
			wb.Sheets[ cfg.name ] = sheet_from_array_of_arrays( _this.toArray( cfg ) );

			data = XLSX.write( wb, {
				bookType: "xlsx",
				bookSST: true,
				type: "base64"
			} );

			data = "data:application/vnd.openxmlformats-officedocument.spreadsheetml.sheet;base64," + data;

			_this.handleCallback( callback, data );

			return data;
		},

		toArray: function( options, callback ) {
			var cfg = _this.deepMerge( {
				data: _this.getChartData(),
				dateFields: [],
				dateFormat: _this.setup.chart.dataDateFormat || "YYYY-MM-DD",
				withHeader: false
			}, options || {}, true );
			var data = [];
			var cols = [];

			if ( _this.setup.chart.categoryAxis && _this.setup.chart.categoryAxis.parseDates && _this.setup.chart.categoryField ) {
				cfg.dateFields.push( _this.setup.chart.categoryField );
			}

			// HEADER
			if ( cfg.withHeader ) {
				for ( col in cfg.data[ 0 ] ) {
					cols.push( col );
				}
				data.push( cols );
			}

			// BODY
			for ( row in cfg.data ) {
				var buffer = [];
				for ( col in cols ) {
					var col = cols[ col ];
					var value = cfg.data[ row ][ col ] || "";

					if ( cfg.dateFormat && value instanceof Date && cfg.dateFields.indexOf( col ) != -1 ) {
						value = AmCharts.formatDate( value, cfg.dateFormat );
					} else {
						value = String( value )
					}

					buffer.push( value );
				}
				data.push( buffer );
			}

			_this.handleCallback( callback, data );

			return data;
		},

		toByteArray: function( options, callback ) {
			var cfg = _this.deepMerge( {
				// Nuffin
			}, options || {} );
			var Arr = ( typeof Uint8Array !== 'undefined' ) ? Uint8Array : Array
			var PLUS = '+'.charCodeAt( 0 )
			var SLASH = '/'.charCodeAt( 0 )
			var NUMBER = '0'.charCodeAt( 0 )
			var LOWER = 'a'.charCodeAt( 0 )
			var UPPER = 'A'.charCodeAt( 0 )
			var data = b64ToByteArray( cfg.data );

			function decode( elt ) {
				var code = elt.charCodeAt( 0 )
				if ( code === PLUS )
					return 62 // '+'
				if ( code === SLASH )
					return 63 // '/'
				if ( code < NUMBER )
					return -1 //no match
				if ( code < NUMBER + 10 )
					return code - NUMBER + 26 + 26
				if ( code < UPPER + 26 )
					return code - UPPER
				if ( code < LOWER + 26 )
					return code - LOWER + 26
			}

			function b64ToByteArray( b64 ) {
				var i, j, l, tmp, placeHolders, arr

				if ( b64.length % 4 > 0 ) {
					throw new Error( 'Invalid string. Length must be a multiple of 4' )
				}

				// the number of equal signs (place holders)
				// if there are two placeholders, than the two characters before it
				// represent one byte
				// if there is only one, then the three characters before it represent 2 bytes
				// this is just a cheap hack to not do indexOf twice
				var len = b64.length
				placeHolders = '=' === b64.charAt( len - 2 ) ? 2 : '=' === b64.charAt( len - 1 ) ? 1 : 0

				// base64 is 4/3 + up to two characters of the original data
				arr = new Arr( b64.length * 3 / 4 - placeHolders )

				// if there are placeholders, only get up to the last complete 4 chars
				l = placeHolders > 0 ? b64.length - 4 : b64.length

				var L = 0

				function push( v ) {
					arr[ L++ ] = v
				}

				for ( i = 0, j = 0; i < l; i += 4, j += 3 ) {
					tmp = ( decode( b64.charAt( i ) ) << 18 ) | ( decode( b64.charAt( i + 1 ) ) << 12 ) | ( decode( b64.charAt( i + 2 ) ) << 6 ) | decode( b64.charAt( i + 3 ) )
					push( ( tmp & 0xFF0000 ) >> 16 )
					push( ( tmp & 0xFF00 ) >> 8 )
					push( tmp & 0xFF )
				}

				if ( placeHolders === 2 ) {
					tmp = ( decode( b64.charAt( i ) ) << 2 ) | ( decode( b64.charAt( i + 1 ) ) >> 4 )
					push( tmp & 0xFF )
				} else if ( placeHolders === 1 ) {
					tmp = ( decode( b64.charAt( i ) ) << 10 ) | ( decode( b64.charAt( i + 1 ) ) << 4 ) | ( decode( b64.charAt( i + 2 ) ) >> 2 )
					push( ( tmp >> 8 ) & 0xFF )
					push( tmp & 0xFF )
				}

				return arr
			}

			_this.handleCallback( callback, data );

			return data;
		},

		// CALLBACK HANDLER
		handleCallback: function( callback, data ) {
			if ( callback ) {
				callback.apply( _this, [ data ] );
			}
		},

		getChartData: function() {
			var data = [];

			if ( _this.setup.chart.type == "stock" ) {
				data = _this.setup.chart.mainDataSet.dataProvider;
			} else if ( _this.setup.chart.type == "gantt" ) {
				var segmentsField = _this.setup.chart.segmentsField;
				for ( var i1 = 0; i1 < _this.setup.chart.dataProvider.length; i1++ ) {
					if ( _this.setup.chart.dataProvider[ i1 ][ segmentsField ] ) {
						for ( var i2 = 0; i2 < _this.setup.chart.dataProvider[ i1 ][ segmentsField ].length; i2++ ) {
							data.push( _this.setup.chart.dataProvider[ i1 ][ segmentsField ][ i2 ] )
						}
					}
				}
			} else {
				data = _this.setup.chart.dataProvider;
			}

			return data;
		},

		capitalize: function( string ) {
			return string.charAt( 0 ).toUpperCase() + string.slice( 1 ).toLowerCase();
		},

		// MENU BUILDER
		createMenu: function( list ) {
			function buildList( list, container ) {
				var ul = document.createElement( "ul" );
				for ( i in list ) {
					var item = typeof list[ i ] === "string" ? {
						format: list[ i ]
					} : list[ i ];
					var li = document.createElement( "li" );
					var a = document.createElement( "a" );
					var img = document.createElement( "img" );
					var span = document.createElement( "span" );
					var action = String( item.action ? item.action : item.format ).toLowerCase();

					item.format = String( item.format ).toUpperCase();

					// MERGE WITH GIVEN FORMAT
					if ( _this.config.formats[ item.format ] ) {
						item = _this.deepMerge( {
							label: item.icon ? "" : item.format,
							format: item.format,
							mimeType: _this.config.formats[ item.format ].mimeType,
							extension: _this.config.formats[ item.format ].extension,
							capture: _this.config.formats[ item.format ].capture,
							action: _this.config.action,
							fileName: _this.config.fileName
						}, item );
					} else if ( !item.menu && !item.items ) {
						item.label = item.label ? item.label : _this.capitalize( action );
					}

					// FILTER; TOGGLE FLAG
					if ( [ "CSV", "JSON", "XLSX" ].indexOf( item.format ) != -1 && [ "map", "gauge" ].indexOf( _this.setup.chart.type ) != -1 ) {
						continue;
					}

					// ADD CLICK HANDLER
					if ( !item.click && !item.menu && !item.items ) {

						// DRAWING METHODS
						if ( _this.drawing.actions.indexOf( action ) != -1 ) {
							item.action = action;
							item.click = ( function( item ) {
								return function() {
									this.drawing[ item.action ]();
								}
							} )( item );

							// DRAWING
						} else if ( _this.drawing.enabled ) {
							item.click = ( function( item ) {
								return function() {
									this[ "to" + item.format ]( item, function( data ) {
										if ( item.action != "print" && item.format != "PRINT" ) {
											this.download( data, item.mimeType, [ item.fileName, item.extension ].join( "." ) );
										}
										this.drawing.done();
									} );
								}
							} )( item );

							// REGULAR
						} else if ( item.format != "UNKNOWN" ) {
							item.click = ( function( item ) {
								return function() {
									if ( item.capture || ( item.action == "print" || item.format == "PRINT" ) ) {
										this.capture( item, function() {
											this[ "to" + item.format ]( item, function( data ) {
												if ( item.action == "download" ) {
													this.download( data, item.mimeType, [ item.fileName, item.extension ].join( "." ) );
												}
											} );
										} )

									} else if ( this[ "to" + item.format ] ) {
										this[ "to" + item.format ]( item, function( data ) {
											this.download( data, item.mimeType, [ item.fileName, item.extension ].join( "." ) );
										} );
									} else {
										throw new Error( 'Invalid format. Could not determine output type.' );
									}
								}
							} )( item );
						}

						// DRAWING
					} else if ( item.action == "draw" ) {
						item.click = ( function( item ) {
							return function() {
								this.capture( item, function() {
									this.createMenu( item.menu, item );
								} );
							}
						} )( item );
					}

					// ADD LINK ATTR
					a.setAttribute( "href", "#" );
					a.addEventListener( "click", ( function( callback ) {
						return function( e ) {
							callback.apply( _this, arguments );
							e.preventDefault();
						}
					} )( item.click || function( e ) {
						e.preventDefault();
					} ) );
					li.appendChild( a );

					// ADD LABEL
					span.innerHTML = item.label;

					// APPEND ITEMS
					if ( item[ "class" ] ) {
						li.className = item[ "class" ];
					}
					if ( item.icon ) {
						img.setAttribute( "src", ( item.icon.slice( 0, 10 ).indexOf( "//" ) == -1 ? chart.pathToImages : "" ) + item.icon );
						a.appendChild( img );
					}
					if ( item.label ) {
						a.appendChild( span );
					}
					if ( item.title ) {
						a.setAttribute( "title", item.title );
					}

					// CALLBACK; REVIVER FOR MENU ITEMS
					if ( _this.config.menuReviver ) {
						li = _this.config.menuReviver.apply( _this, [ item, li ] );
					}

					// ADD SUBLIST; JUST WITH ENTRIES
					if ( ( item.menu || item.items ) && item.action != "draw" ) {
						if ( buildList( item.menu || item.items, li ).childNodes.length ) {
							ul.appendChild( li );
						}
					} else {
						ul.appendChild( li );
					}
				}

				// JUST ADD THOSE WITH ENTRIES
				return container.appendChild( ul );
			}

			var div = _this.setup.chart.containerDiv.getElementsByClassName( "amcharts-export-menu" );
			if ( div.length ) {
				div = div[ 0 ];
				div.innerHTML = "";
			} else {
				var div = document.createElement( "div" );
				div.setAttribute( "class", "amExportButton amcharts-export-menu amcharts-export-menu-" + _this.config.position );
				_this.setup.menu = div;
			}

			// CALLBACK; REPLACES THE MENU WALKER
			if ( _this.config.menuWalker ) {
				buildList = _this.config.menuWalker;
			}
			buildList( list, div );

			_this.setup.chart.containerDiv.appendChild( div );

			return div;
		},

		migrateSetup: function( chart ) {
			if ( chart.amExport || chart.exportConfig ) {
				var config = _this.deepMerge( {
					enabled: true,
					migrated: true,
					libs: {
						autoLoad: false
					}
				}, _this.defaults );

				function crawler( object ) {
					for ( key in object ) {
						var value = object[ key ];

						if ( key.slice( 0, 6 ) == "export" && value ) {
							config.menu.push( key.slice( 6 ) );
						} else if ( key == "userCFG" ) {
							crawler( value );
						} else if ( key == "menuItems" ) {
							config.menu = value;
						} else if ( key == "libs" ) {
							config.libs = value;
						}
					}
				}
				crawler( chart.amExport || chart.exportConfig );
				chart[ "export" ] = config;
			}
			return chart;
		},

		// INITIATE
		init: function( chart ) {
			_this.setup.canvas = document.createElement( "canvas" );
			_this.setup.wrapper = document.createElement( "div" );
			_this.setup.wrapper.setAttribute( "class", "amcharts-export-canvas" );
			_this.setup.wrapper.appendChild( _this.setup.canvas );
			_this.setup.chart.containerDiv.appendChild( _this.setup.wrapper );

			_this.setup.chart.AmExport = _this;

			// CREATE MENU
			_this.timer = setTimeout( function() {
				clearTimeout( _this.timer );
				_this.createMenu( _this.config.menu );
			}, AmCharts.updateRate );
		}
	}

	// EXTEND DRAWING TO SUPPORT "CANCEL" MENU ACTION
	_this.drawing.cancel = _this.drawing.done;

	// MIRGRATE
	_this.setup.chart = _this.migrateSetup( chart );

	// ENABLED-I-O?
	if ( undefined === _this.setup.chart[ "export" ] || !_this.setup.chart[ "export" ].enabled ) {
		return;
	}

	// POLYFILL BLOB
	if ( !window.Blob ) {
		_this.libs.resources.push( "blob.js/blob.js" );
	}

	// MERGE SETTINGS
	_this.deepMerge( _this.libs, _this.setup.chart[ "export" ].libs || {}, true );
	_this.deepMerge( _this.defaults.pdfMake, _this.setup.chart[ "export" ] );
	_this.deepMerge( _this.defaults.fabric, _this.setup.chart[ "export" ] );
	_this.config = _this.deepMerge( _this.defaults, _this.setup.chart[ "export" ], true );

	_this.setup.chart[ "export" ] = _this;
	_this.setup.chart.addClassNames = true;

	// WITH IE?
	if ( AmCharts.isIE && AmCharts.IEversion < 9 ) {
		return;
	}

	// LOAD DEPENDENCIES
	_this.loadDependencies();

	// WAIT FOR CONTAINER
	_this.timer = setInterval( function() {
		if ( _this.setup.chart.containerDiv ) {
			clearTimeout( _this.timer );
			_this.init();
		}
	}, AmCharts.updateRate );

}, [ "pie", "serial", "xy", "funnel", "radar", "gauge", "stock", "map", "gantt" ] );