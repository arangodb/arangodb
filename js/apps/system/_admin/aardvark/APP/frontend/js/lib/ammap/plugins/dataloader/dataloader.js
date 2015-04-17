/*
Plugin Name: amCharts Data Loader
Description: This plugin adds external data loading capabilities to all amCharts libraries.
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

/**
 * TODO:
 * incremental load
 * XML support (?)
 */

AmCharts.addInitHandler( function ( chart ) {

  /**
   * Check if dataLoader is set (initialize it)
   */
  if ( undefined === chart.dataLoader || ! isObject( chart.dataLoader ) )
    chart.dataLoader = {};

  /**
   * Check charts version for compatibility:
   * the first compatible version is 3.13
   */
  var version = chart.version.split( '.' );
  if ( ( Number( version[0] ) < 3 ) || ( 3 == Number( version[0] ) && ( Number( version[1] ) < 13 ) ) )
    return;

  /**
   * Define object reference for easy access
   */
  var l = chart.dataLoader;
  l.remaining = 0;

  /**
   * Set defaults
   */
  var defaults = {
    'async':          true,
    'format':        'json',
    'showErrors':     true,
    'showCurtain':    true,
    'noStyles':       false,
    'reload':         0,
    'timestamp':      false,
    'delimiter':      ',',
    'skip':           0,
    'useColumnNames': false,
    'reverse':        false,
    'reloading':      false
  };

  /**
   * Load all files in a row
   */
  if ( 'stock' === chart.type ) {
    
    // delay this a little bit so the chart has the chance to build itself
    setTimeout( function () {

      // preserve animation
      if ( 0 > chart.panelsSettings.startDuration ) {
        l.startDuration = chart.panelsSettings.startDuration;
        chart.panelsSettings.startDuration = 0;
      }

      // cycle through all of the data sets
      for ( var x in chart.dataSets ) {
        var ds = chart.dataSets[ x ];

        // load data
        if ( undefined !== ds.dataLoader && undefined !== ds.dataLoader.url ) {

          ds.dataProvider = [];
          applyDefaults( ds.dataLoader );
          loadFile( ds.dataLoader.url, ds, ds.dataLoader, 'dataProvider' );

        }

        // load events data
        if ( undefined !== ds.eventDataLoader && undefined !== ds.eventDataLoader.url ) {

          ds.events = [];
          applyDefaults( ds.eventDataLoader );
          loadFile( ds.eventDataLoader.url, ds, ds.eventDataLoader, 'stockEvents' );

        }
      }

    }, 100 );

  }
  else {

    applyDefaults( l );

    if ( undefined === l.url )
      return;

    // preserve animation
    if ( undefined !== chart.startDuration && ( 0 < chart.startDuration ) ) {
      l.startDuration = chart.startDuration;
      chart.startDuration = 0;
    }

    chart.dataProvider = [];
    loadFile( l.url, chart, l, 'dataProvider' );

  }

  /**
   * Loads a file and determines correct parsing mechanism for it
   */
  function loadFile( url, holder, options, providerKey ) {

    // set default providerKey
    if ( undefined === providerKey )
      providerKey = 'dataProvider';

    // show curtain
    if ( options.showCurtain )
      showCurtain( undefined, options.noStyles );

    // increment loader count
    l.remaining++;

    // load the file
    AmCharts.loadFile( url, options, function ( response ) {

      // error?
      if ( false === response )
        raiseError( AmCharts.__( 'Error loading the file', chart.language ) + ': ' + url, false, options );
      
      else {

        // determine the format
        if ( undefined === options.format ) {
          // TODO
          options.format = 'json';
        }

        // lowercase
        options.format = options.format.toLowerCase();

        // invoke parsing function
        switch( options.format ) {
          
          case 'json':
            
            holder[providerKey] = AmCharts.parseJSON( response, options );
            
            if ( false === holder[providerKey] ) {
              raiseError( AmCharts.__( 'Error parsing JSON file', chart.language ) + ': ' + l.url, false, options );
              holder[providerKey] = [];
            }
            else {
              holder[providerKey] = postprocess( holder[providerKey], options );
            }

            break;

          case 'csv':
            
            holder[providerKey] = AmCharts.parseCSV( response, options );
            
            if ( false === holder[providerKey] ) {
              raiseError( AmCharts.__( 'Error parsing CSV file', chart.language ) + ': ' + l.url, false, options );
              holder[providerKey] = [];
            }
            else {
              holder[providerKey] = postprocess( holder[providerKey], options );
            }

            break;

          default:
            raiseError( AmCharts.__( 'Unsupported data format', chart.language ) + ': ' + options.format, false, options.noStyles );
            break;
        }

        // decrement remaining counter
        l.remaining--;

        // we done?
        if ( 0 === l.remaining ) {

          // take in the new data
          if ( options.async ) {

            if ( 'map' === chart.type )
              chart.validateNow( true );
            else {
              
              // take in new data
              chart.validateData();

              // make the chart animate again
              if ( l.startDuration ) {
                if ( 'stock' === chart.type ) {
                  chart.panelsSettings.startDuration = l.startDuration;
                  for ( var x in chart.panels ) {
                    chart.panels[x].startDuration = l.startDuration;
                    chart.panels[x].animateAgain();
                  }
                }
                else {
                  chart.startDuration = l.startDuration;
                  chart.animateAgain();
                }
              }
            }
          }

          // restore default period
          if ( 'stock' === chart.type && ! options.reloading )
            chart.periodSelector.setDefaultPeriod();

          // remove curtain
          removeCurtain();
        }

        // schedule another load of necessary
        if ( options.reload ) {
          
          if ( options.timeout )
            clearTimeout( options.timeout );

          options.timeout = setTimeout( loadFile, 1000 * options.reload, url, holder, options );
          options.reloading = true;

        }

      }

    } );
  
  }

  /**
   * Checks if postProcess is set and invokes the handler
   */
  function postprocess ( data, options ) {
    if ( undefined !== options.postProcess && isFunction( options.postProcess ) )
      try {
        return options.postProcess.call( this, data, options );
      }
      catch ( e ) {
        raiseError( AmCharts.__( 'Error loading file', chart.language ) + ': ' + options.url, false, options );
        return data;
      }
    else
      return data;
  }

  /**
   * Returns true if argument is object
   */
  function isArray ( obj ) {
    return obj instanceof Array;
  }

  /**
   * Returns true if argument is array
   */
  function isObject ( obj ) {
    return 'object' === typeof( obj );
  }

  /**
   * Returns true is argument is a function
   */
  function isFunction ( obj ) {
    return 'function' === typeof( obj );
  }

  /**
   * Applies defaults to config object
   */
  function applyDefaults ( obj ) {
    for ( var x in defaults ) {
      setDefault( obj, x, defaults[ x ] );
    }
  }

  /**
   * Checks if object property is set, sets with a default if it isn't
   */
  function setDefault ( obj, key, value ) {
    if ( undefined === obj[ key ] )
      obj[ key ] = value;
  }

  /**
   * Raises an internal error (writes it out to console)
   */
  function raiseError ( msg, error, options ) {

    if ( options.showErrors )
      showCurtain( msg, options.noStyles );
    else {
      removeCurtain();
      console.log( msg );
    }

  }

  /**
   * Shows curtain over chart area
   */
  function showCurtain ( msg, noStyles ) {

    // remove previous curtain if there is one
    removeCurtain();

    // did we pass in the message?
    if ( undefined === msg )
      msg = AmCharts.__( 'Loading data...', chart.language );

    // create and populate curtain element
    var curtain =document.createElement( 'div' );
    curtain.setAttribute( 'id', chart.div.id + '-curtain' );
    curtain.className = 'amcharts-dataloader-curtain';

    if ( true !== noStyles ) {
      curtain.style.position = 'absolute';
      curtain.style.top = 0;
      curtain.style.left = 0;
      curtain.style.width = ( undefined !== chart.realWidth ? chart.realWidth : chart.divRealWidth ) + 'px';
      curtain.style.height = ( undefined !== chart.realHeight ? chart.realHeight : chart.divRealHeight ) + 'px';
      curtain.style.textAlign = 'center';
      curtain.style.display = 'table';
      curtain.style.fontSize = '20px';
      curtain.style.background = 'rgba(255, 255, 255, 0.3)';
      curtain.innerHTML = '<div style="display: table-cell; vertical-align: middle;">' + msg + '</div>';
    }
    else {
      curtain.innerHTML = msg;
    }
    chart.containerDiv.appendChild( curtain );

    l.curtain = curtain;
  }

  /**
   * Removes the curtain
   */
  function removeCurtain () {
    try {
      if ( undefined !== l.curtain )
        chart.containerDiv.removeChild( l.curtain );
    }
    catch ( e ) {
      // do nothing
    }

    l.curtain = undefined;

  }

}, [ 'pie', 'serial', 'xy', 'funnel', 'radar', 'gauge', 'gantt', 'stock', 'map' ] );


/**
 * Returns prompt in a chart language (set by chart.language) if it is
 * available
 */
if ( undefined === AmCharts.__ ) {
  AmCharts.__ = function ( msg, language ) {
    if ( undefined !== language 
      && undefined !== AmCharts.translations.dataLoader[ chart.language ] 
      && undefined !== AmCharts.translations.dataLoader[ chart.language ][ msg ] )
      return AmCharts.translations.dataLoader[ chart.language ][ msg ];
    else
      return msg;
  }
}

/**
 * Loads a file from url and calls function handler with the result
 */
AmCharts.loadFile = function ( url, options, handler ) {

  // create the request
  if ( window.XMLHttpRequest ) {
    // IE7+, Firefox, Chrome, Opera, Safari
    var request = new XMLHttpRequest();
  } else {
    // code for IE6, IE5
    var request = new ActiveXObject( 'Microsoft.XMLHTTP' );
  }

  // set handler for data if async loading
  request.onreadystatechange = function () {
    
    if ( 4 == request.readyState && 404 == request.status )
      handler.call( this, false );

    else if ( 4 == request.readyState && 200 == request.status )
      handler.call( this, request.responseText );

  }

  // load the file
  try {
    request.open( 'GET', options.timestamp ? AmCharts.timestampUrl( url ) : url, options.async );
    request.send();
  }
  catch ( e ) {
    handler.call( this, false );
  }

};

/**
 * Parses JSON string into an object
 */
AmCharts.parseJSON = function ( response, options ) {
  try {
    if ( undefined !== JSON )
      return JSON.parse( response );
    else
      return eval( response );
  }
  catch ( e ) {
    return false;
  }
}

/**
 * Prases CSV string into an object
 */
AmCharts.parseCSV = function ( response, options ) {
  
  // parse CSV into array
  var data = AmCharts.CSVToArray( response, options.delimiter );

  // init resuling array
  var res = [];
  var cols = [];
  
  // first row holds column names?
  if ( options.useColumnNames ) {
    cols = data.shift();

    // normalize column names
    for ( var x in cols ) {
      // trim
      var col = cols[ x ].replace( /^\s+|\s+$/gm, '' );

      // check for empty
      if ( '' === col )
        col = 'col' + x;

      cols[ x ] = col;
    }

    if ( 0 < options.skip )
      options.skip--;
  }

  // skip rows
  for ( var i = 0; i < options.skip; i++ )
    data.shift();

  // iterate through the result set
  var row;
  while ( row = options.reverse ? data.pop() : data.shift() ) {
    var dataPoint = {};
    for ( var i = 0; i < row.length; i++ ) {
      var col = undefined === cols[ i ] ? 'col' + i : cols[ i ];
      dataPoint[ col ] = row[ i ];
    }
    res.push( dataPoint );
  }

  return res;
}

/**
 * Parses CSV data into array
 * Taken from here: (thanks!)
 * http://www.bennadel.com/blog/1504-ask-ben-parsing-csv-strings-with-javascript-exec-regular-expression-command.htm
 */
AmCharts.CSVToArray = function ( strData, strDelimiter ){
  // Check to see if the delimiter is defined. If not,
  // then default to comma.
  strDelimiter = (strDelimiter || ",");

  // Create a regular expression to parse the CSV values.
  var objPattern = new RegExp(
    (
        // Delimiters.
        "(\\" + strDelimiter + "|\\r?\\n|\\r|^)" +

        // Quoted fields.
        "(?:\"([^\"]*(?:\"\"[^\"]*)*)\"|" +

        // Standard fields.
        "([^\"\\" + strDelimiter + "\\r\\n]*))"
    ),
    "gi"
    );


  // Create an array to hold our data. Give the array
  // a default empty first row.
  var arrData = [[]];

  // Create an array to hold our individual pattern
  // matching groups.
  var arrMatches = null;


  // Keep looping over the regular expression matches
  // until we can no longer find a match.
  while (arrMatches = objPattern.exec( strData )){

    // Get the delimiter that was found.
    var strMatchedDelimiter = arrMatches[ 1 ];

    // Check to see if the given delimiter has a length
    // (is not the start of string) and if it matches
    // field delimiter. If id does not, then we know
    // that this delimiter is a row delimiter.
    if (
        strMatchedDelimiter.length &&
        (strMatchedDelimiter != strDelimiter)
        ){

        // Since we have reached a new row of data,
        // add an empty row to our data array.
        arrData.push( [] );

    }


    // Now that we have our delimiter out of the way,
    // let's check to see which kind of value we
    // captured (quoted or unquoted).
    if (arrMatches[ 2 ]){

        // We found a quoted value. When we capture
        // this value, unescape any double quotes.
        var strMatchedValue = arrMatches[ 2 ].replace(
            new RegExp( "\"\"", "g" ),
            "\""
            );

    } else {

        // We found a non-quoted value.
        var strMatchedValue = arrMatches[ 3 ];

    }


    // Now that we have our value string, let's add
    // it to the data array.
    arrData[ arrData.length - 1 ].push( strMatchedValue );
  }

  // Return the parsed data.
  return( arrData );
}

/**
 * Appends timestamp to the url
 */
AmCharts.timestampUrl = function ( url ) {
  var p = url.split( '?' );
  if ( 1 === p.length )
    p[1] = new Date().getTime();
  else
    p[1] += '&' + new Date().getTime();
  return p.join( '?' );
}