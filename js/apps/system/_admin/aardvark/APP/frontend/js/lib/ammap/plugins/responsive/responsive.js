/*
Plugin Name: amCharts Responsive
Description: This plugin add responsive functionality to JavaScript Charts and Maps.
Author: Martynas Majeris, amCharts
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

AmCharts.addInitHandler( function ( chart ) {

	// check if responsive object is set
	if ( undefined === chart.responsive )
		return;

	// check if it's already initialized
	if ( chart.responsive.ready )
		return;

	// a small variable for easy reference
	var r = chart.responsive;

	// init
	r.ready = true,
	r.currentRules = {};
	r.overridden = [];
	r.original = {};

	// check if responsive is enabled
	if ( true !== r.enabled )
		return;

	// check charts version for compatibility
	// the first compatible version is 3.13
	var version = chart.version.split( '.' );
	if ( ( Number( version[0] ) < 3 ) || ( 3 == Number( version[0] ) && ( Number( version[1] ) < 13 ) ) )
		return;

	// defaults per chart type
	var defaults = {

		/**
		 * AmPie
		 */

		'pie': [

			/**
			 * Disable legend in certain cases
			 */
			{
				"maxWidth": 550,
				"legendPosition": "left",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 550,
				"legendPosition": "right",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 150,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 350,
				"legendPosition": "top",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 350,
				"legendPosition": "bottom",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 150,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},

			/**
			 * Narrow chart
			 */
			{
				"maxWidth": 400,
				"overrides": {
					"labelsEnabled": false
				}
			},
			{
				"maxWidth": 100,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},

			/**
			 * Short chart
			 */
			{
				"maxHeight": 350,
				"overrides": {
					"pullOutRadius": 0
				}
			},
			{
				"maxHeight": 200,
				"overrides": {
					"titles": {
						"enabled": false
					},
					"labelsEnabled": false
				}
			},

			/**
			 * Supersmall
			 */

			{
				"maxWidth": 60,
				"overrides": {
					"autoMargins": false,
					"marginTop": 0,
					"marginBottom": 0,
					"marginLeft": 0,
					"marginRight": 0,
					"radius": "50%",
					"innerRadius": 0,
					"balloon": {
						"enabled": false
					},
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 60,
				"overrides": {
					"marginTop": 0,
					"marginBottom": 0,
					"marginLeft": 0,
					"marginRight": 0,
					"radius": "50%",
					"innerRadius": 0,
					"balloon": {
						"enabled": false
					},
					"legend": {
						"enabled": false
					}
				}
			}

		],

		/**
		 * AmFunnel
		 */

		'funnel': [

			{
				"maxWidth": 550,
				"legendPosition": "left",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 550,
				"legendPosition": "right",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 150,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 500,
				"legendPosition": "top",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 500,
				"legendPosition": "bottom",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 150,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 400,
				"overrides": {
					"labelsEnabled": false,
					"marginLeft": 10,
					"marginRight": 10,
					"legend": {
						"enabled": false
					}
				}
			},

			{
				"maxHeight": 350,
				"overrides": {
					"pullOutRadius": 0,
					"legend": {
						"enabled": false
					}
				}
			},

			{
				"maxHeight": 300,
				"overrides": {
					"titles": {
						"enabled": false
					}
				}
			}
		],

		/**
		 * AmRadar
		 */

		"radar": [

			{
				"maxWidth": 550,
				"legendPosition": "left",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 550,
				"legendPosition": "right",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 150,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 350,
				"legendPosition": "top",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 350,
				"legendPosition": "bottom",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 150,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 300,
				"overrides": {
					"labelsEnabled": false
				}
			},
			{
				"maxWidth": 200,
				"overrides": {
					"autoMargins": false,
					"marginTop": 0,
					"marginBottom": 0,
					"marginLeft": 0,
					"marginRight": 0,
					"radius": "50%",
					"titles": {
						"enabled": false
					},
					"valueAxes": {
						"labelsEnabled": false,
						"radarCategoriesEnabled": false
					}
				}
			},

			{
				"maxHeight": 300,
				"overrides": {
					"labelsEnabled": false
				}
			},

			{
				"maxHeight": 200,
				"overrides": {
					"autoMargins": false,
					"marginTop": 0,
					"marginBottom": 0,
					"marginLeft": 0,
					"marginRight": 0,
					"radius": "50%",
					"titles": {
						"enabled": false
					},
					"valueAxes": {
						"radarCategoriesEnabled": false
					}
				}
			},

			{
				"maxHeight": 100,
				"overrides": {
					"valueAxes": {
						"labelsEnabled": false
					}
				}
			}

		],

		/**
		 * AmGauge
		 */

		'gauge': [
			{
				"maxWidth": 550,
				"legendPosition": "left",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 550,
				"legendPosition": "right",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 150,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 500,
				"legendPosition": "top",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 500,
				"legendPosition": "bottom",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 150,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 200,
				"overrides": {
					"titles": {
						"enabled": false
					},
					"allLabels": {
						"enabled": false
					},
					"axes": {
						"labelsEnabled": false
					}
				}
			},
			{
				"maxHeight": 200,
				"overrides": {
					"titles": {
						"enabled": false
					},
					"allLabels": {
						"enabled": false
					},
					"axes": {
						"labelsEnabled": false
					}
				}
			}
		],
		
		/**
		 * AmSerial
		 */
		"serial": [

			/**
			 * Disable legend in certain cases
			 */
			{
				"maxWidth": 550,
				"legendPosition": "left",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 550,
				"legendPosition": "right",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 100,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 350,
				"legendPosition": "top",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 350,
				"legendPosition": "bottom",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 100,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},


			/**
			 * Narrow chart
			 */
			{
				"maxWidth": 350,
				"overrides": {
					"autoMarginOffset": 0,
					"graphs": {
						"hideBulletsCount": 10
					}
				}
			},
			{
				"maxWidth": 350,
				"rotate": false,
				"overrides": {
					"marginLeft": 10,
					"marginRight": 10,
					"valueAxes": {
						"ignoreAxisWidth": true,
						"inside": true,
						"title": "",
						"showFirstLabel": false,
						"showLastLabel": false
					},
					"graphs": {
						"bullet": "none"
					}
				}
			},
			{
				"maxWidth": 350,
				"rotate": true,
				"overrides": {
					"marginLeft": 10,
					"marginRight": 10,
					"categoryAxis": {
						"ignoreAxisWidth": true,
						"inside": true,
						"title": ""
					}
				}
			},
			{
				"maxWidth": 200,
				"rotate": false,
				"overrides": {
					"marginLeft": 10,
					"marginRight": 10,
					"marginTop": 10,
					"marginBottom": 10,
					"categoryAxis": {
						"ignoreAxisWidth": true,
						"labelsEnabled": false,
						"inside": true,
						"title": "",
						"guides": {
							"inside": true
						}
					},
					"valueAxes": {
						"ignoreAxisWidth": true,
						"labelsEnabled": false,
						"axisAlpha": 0,
						"guides": {
							"label": ""
						}
					},
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 200,
				"rotate": true,
				"overrides": {
					"chartScrollbar": {
						"scrollbarHeight": 4,
						"graph": "",
						"resizeEnabled": false
					},
					"categoryAxis": {
						"labelsEnabled": false,
						"axisAlpha": 0,
						"guides": {
							"label": ""
						}
					},
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 100,
				"rotate": false,
				"overrides": {
					"valueAxes": {
						"gridAlpha": 0
					}
				}
			},
			{
				"maxWidth": 100,
				"rotate": true,
				"overrides": {
					"categoryAxis": {
						"gridAlpha": 0
					}
				}
			},

			/**
			 * Short chart
			 */
			{
				"maxHeight": 300,
				"overrides": {
					"autoMarginOffset": 0,
					"graphs": {
						"hideBulletsCount": 10
					}
				}
			},
			{
				"maxHeight": 200,
				"rotate": false,
				"overrides": {
					"marginTop": 10,
					"marginBottom": 10,
					"categoryAxis": {
						"ignoreAxisWidth": true,
						"inside": true,
						"title": "",
						"showFirstLabel": false,
						"showLastLabel": false
					}
				}
			},
			{
				"maxHeight": 200,
				"rotate": true,
				"overrides": {
					"marginTop": 10,
					"marginBottom": 10,
					"valueAxes": {
						"ignoreAxisWidth": true,
						"inside": true,
						"title": "",
						"showFirstLabel": false,
						"showLastLabel": false
					},
					"graphs": {
						"bullet": "none"
					}
				}
			},
			{
				"maxHeight": 150,
				"rotate": false,
				"overrides": {
					"titles": {
						"enabled": false
					},
					"chartScrollbar": {
						"scrollbarHeight": 4,
						"graph": "",
						"resizeEnabled": false
					},
					"categoryAxis": {
						"labelsEnabled": false,
						"ignoreAxisWidth": true,
						"axisAlpha": 0,
						"guides": {
							"label": ""
						}
					}
				}
			},
			{
				"maxHeight": 150,
				"rotate": true,
				"overrides": {
					"titles": {
						"enabled": false
					},
					"valueAxes": {
						"labelsEnabled": false,
						"ignoreAxisWidth": true,
						"axisAlpha": 0,
						"guides": {
							"label": ""
						}
					}
				}
			},
			{
				"maxHeight": 100,
				"rotate": false,
				"overrides": {
					"valueAxes": {
						"labelsEnabled": false,
						"ignoreAxisWidth": true,
						"axisAlpha": 0,
						"gridAlpha": 0,
						"guides": {
							"label": ""
						}
					}
				}
			},
			{
				"maxHeight": 100,
				"rotate": true,
				"overrides": {
					"categoryAxis": {
						"labelsEnabled": false,
						"ignoreAxisWidth": true,
						"axisAlpha": 0,
						"gridAlpha": 0,
						"guides": {
							"label": ""
						}
					}
				}
			},

			/**
			 * Really small charts: microcharts and sparklines
			 */
			{
				"maxWidth": 100,
				"overrides": {
					"autoMargins": false,
					"marginTop": 0,
					"marginBottom": 0,
					"marginLeft": 0,
					"marginRight": 0,
					"categoryAxis": {
						"labelsEnabled": false
					},
					"valueAxes": {
						"labelsEnabled": false
					}
				}
			},
			{
				"maxHeight": 100,
				"overrides": {
					"autoMargins": false,
					"marginTop": 0,
					"marginBottom": 0,
					"marginLeft": 0,
					"marginRight": 0,
					"categoryAxis": {
						"labelsEnabled": false
					},
					"valueAxes": {
						"labelsEnabled": false
					}
				}
			}

			],
		
		/**
		 * AmXY
		 */
		"xy": [

			/**
			 * Disable legend in certain cases
			 */
			{
				"maxWidth": 550,
				"legendPosition": "left",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 550,
				"legendPosition": "right",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 100,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 350,
				"legendPosition": "top",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 350,
				"legendPosition": "bottom",
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxHeight": 100,
				"overrides": {
					"legend": {
						"enabled": false
					}
				}
			},

			/**
			 * Narrow chart
			 */
			{
				"maxWidth": 250,
				"overrides": {
					"autoMarginOffset": 0,
					"autoMargins": false,
					"marginTop": 0,
					"marginBottom": 0,
					"marginLeft": 0,
					"marginRight": 0,
					"valueAxes": {
						"inside": true,
						"title": "",
						"showFirstLabel": false,
						"showLastLabel": false
					},
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 150,
				"overrides": {
					"valueyAxes": {
						"labelsEnabled": false,
						"axisAlpha": 0,
						"gridAlpha": 0,
						"guides": {
							"label": ""
						}
					}
				}
			},

			/**
			 * Short chart
			 */
			{
				"maxHeight": 250,
				"overrides": {
					"autoMarginOffset": 0,
					"autoMargins": false,
					"marginTop": 0,
					"marginBottom": 0,
					"marginLeft": 0,
					"marginRight": 0,
					"valueAxes": {
						"inside": true,
						"title": "",
						"showFirstLabel": false,
						"showLastLabel": false
					},
					"legend": {
						"enabled": false
					}
				}
			},
			{
				"maxWidth": 150,
				"overrides": {
					"valueyAxes": {
						"labelsEnabled": false,
						"axisAlpha": 0,
						"gridAlpha": 0,
						"guides": {
							"label": ""
						}
					}
				}
			}],

		/**
		 * AmStock
		 */

		'stock': [
			
			{
				"maxWidth": 500,
				"overrides": {
					"dataSetSelector": {
						"position": "top"
					},
					"periodSelector": {
						"position": "bottom"
					}
				}
			},

			{
				"maxWidth": 400,
				"overrides": {
					"dataSetSelector": {
						"selectText": "",
						"compareText": ""
					},
					"periodSelector": {
						"periodsText": "",
						"inputFieldsEnabled": false
					}
				}
			}
		],

		/**
		 * AmMap
		 */

		'map': [
			{
				"maxWidth": 200,
				"overrides": {
					"zoomControl": {
						"zoomControlEnabled": false
					},
					"smallMap": {
						"enabled": false
					},
					"valueLegend": {
						"enabled": false
					},
					"dataProvider": {
						"areas": {
							"descriptionWindowWidth": 160,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						},
						"images": {
							"descriptionWindowWidth": 160,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						},
						"lines": {
							"descriptionWindowWidth": 160,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						}
					}
				}
			},
			{
				"maxWidth": 150,
				"overrides": {
					"dataProvider": {
						"areas": {
							"descriptionWindowWidth": 110,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						},
						"images": {
							"descriptionWindowWidth": 110,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						},
						"lines": {
							"descriptionWindowWidth": 110,
							"descriptionWindowLeft": 10,
							"descriptionWindowRight": 10
						}
					}
				}
			},
			{
				"maxHeight": 200,
				"overrides": {
					"zoomControl": {
						"zoomControlEnabled": false
					},
					"smallMap": {
						"enabled": false
					},
					"valueLegend": {
						"enabled": false
					},
					"dataProvider": {
						"areas": {
							"descriptionWindowHeight": 160,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						},
						"images": {
							"descriptionWindowHeight": 160,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						},
						"lines": {
							"descriptionWindowHeight": 160,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						}
					}
				}
			},
			{
				"maxHeight": 150,
				"overrides": {
					"dataProvider": {
						"areas": {
							"descriptionWindowHeight": 110,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						},
						"images": {
							"descriptionWindowHeight": 110,
							"descriptionWindowRight": 10,
							"descriptionWindowTop": 10
						},
						"lines": {
							"descriptionWindowHeight": 110,
							"descriptionWindowLeft": 10,
							"descriptionWindowRight": 10
						}
					}
				}
			}
		]
	};

	// duplicate the serial chart defaults for gantt
	defaults[ 'gantt' ] = defaults[ 'serial' ];

	// populate with defaults if necessary
	if ( undefined == r.rules || 0 == r.rules.length || !isArray( r.rules ) )
		r.rules = defaults[chart.type];
	else if ( false !== r.addDefaultRules )
		r.rules = defaults[chart.type].concat( r.rules );

	// set original 
	setOriginalProperty( chart, 'zoomOutOnDataUpdate', chart.zoomOutOnDataUpdate );

	// tap into chart resize events
	chart.addListener( 'resized', checkRules );
	chart.addListener( 'init', checkRules );

	function checkRules () {
		
		// get current chart dimensions
		var w = chart.divRealWidth;
		var h = chart.divRealHeight;

		// get current rules
		var rulesChanged = false;
		for ( var x in r.rules ) {
			var rule = r.rules[x];
			if (
				( undefined == rule.minWidth || ( rule.minWidth <= w ) )
				&&
				( undefined == rule.maxWidth || ( rule.maxWidth >= w ) )
				&&
				( undefined == rule.minHeight || ( rule.minHeight <= h ) )
				&&
				( undefined == rule.maxHeight || ( rule.maxHeight >= h ) )
				&&
				( undefined == rule.rotate || ( true === rule.rotate && true === chart.rotate ) || ( false === rule.rotate && ( undefined === chart.rotate || false === chart.rotate ) ) )
				&&
				( undefined == rule.legendPosition || ( undefined !== chart.legend && undefined !== chart.legend.position && chart.legend.position === rule.legendPosition ) )
			) {
				if ( undefined == r.currentRules[x] ) {
					r.currentRules[x] = true;
					rulesChanged = true;
				}
			}
			else {
				if ( undefined != r.currentRules[x] ) {
					r.currentRules[x] = undefined;
					rulesChanged = true;
				}
			}
		}

		// check if any rules have changed
		if ( !rulesChanged )
			return;

		// apply original config
		restoreOriginals();
		
		// apply rule-specific properties
		for ( var x in r.currentRules ) {
			if ( undefined != r.currentRules[x] )
				applyConfig( chart, r.rules[x].overrides );
		}
		
		// re-apply zooms/slices as necessary
		// TODO
		
		// redraw the chart
		redrawChart();
	}

	function findArrayObject ( node, id ) {
		if ( node instanceof Array ) {
			for( var x in node ) {
				if ( 'object' === typeof node[x] && node[x].id == id )
					return node[x];
			}
		}
		return false;
	}

	function redrawChart () {
		chart.dataChanged = true;
		if ( 'xy' !== chart.type )
			chart.marginsUpdated = false;
		chart.zoomOutOnDataUpdate = false;
		chart.validateNow( true );
		restoreOriginalProperty( chart, 'zoomOutOnDataUpdate' );
	}

	function isArray ( obj ) {
		return obj instanceof Array;
	}

	function isObject ( obj ) {
		return 'object' === typeof( obj );
	}

	function applyConfig ( original, override ) {
		for( var key in override ) {
			if ( undefined === original[key] ) {
				original[key] = override[key];
				setOriginalProperty( original, key, '_r_none' );
			}
			else if ( isArray( original[key] ) ) {
				// special case - apply overrides selectively

				// an array of simple values
				if ( original[key].length && ! isObject( original[key][0] ) ) {
					setOriginalProperty( original, key, original[key] );
					original[key] = override[key];
				}

				// an array of objects
				else if ( isArray( override[key] ) ) {
					for ( var x in override[key] ) {
						var originalNode = false;
						if ( undefined === override[key][x].id && undefined != original[key][x] )
							originalNode = original[key][x];
						else if ( undefined !== override[key][x].id )
							originalNode = findArrayObject( original[key], override[key][x].id );
						if ( originalNode ) {
							applyConfig( originalNode, override[key][x] );
						}
					}
				}

				// override all array objects with the same values form a single
				// override object
				else if ( isObject( override[key] ) ) {
					for( var x in original[key] ) {
						applyConfig( original[key][x], override[key] );
					}
				}

				// error situation
				else {
					// do nothing (we can't override array using single value)
				}
			}
			else if ( isObject( original[key] ) ) {
				applyConfig( original[key], override[key] );
			}
			else {
				setOriginalProperty( original, key, original[key] );
				original[key] = override[key];
			}
		}
	}

	function setOriginalProperty ( object, prop, value ) {
		if ( undefined === object['_r_' + prop] )
			object['_r_' + prop] = value;

		r.overridden.push( { o: object, p: prop } );
	}

	function restoreOriginals () {
		var p;
		while( p = r.overridden.pop() ) {
			if ( '_r_none' === p.o['_r_' + p.p] )
				delete p.o[p.p];
			else
				p.o[p.p] = p.o['_r_' + p.p];
		}
	}

	function restoreOriginalProperty ( object, prop ) {
		object[prop] = object['_r_' + prop];
	}

}, [ 'pie', 'serial', 'xy', 'funnel', 'radar', 'gauge', 'gantt', 'stock', 'map' ] );