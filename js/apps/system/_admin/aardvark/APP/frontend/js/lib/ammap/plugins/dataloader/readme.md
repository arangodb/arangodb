# amCharts Data Loader

Version: 1.0


## Description

By default all amCharts libraries accept data in JSON format. It needs to be 
there when the web page loads, defined in-line or loaded via custom code.

This plugin introduces are native wrapper that enables automatic loading of data
from external data data sources in CSV and JSON formats.

Most of the times you will just need to provide a URL of the external data 
source - static file or dynamically generated - and it will do the rest.


## Important notice

Due to security measures implemented in most of the browsers, the external data 
loader will work only when the page with the chart or map is loaded via web 
server.

So, any of the examples loaded locally (file:///) will not work.

The page needs to be loaded via web server (http://) in order to work properly.

Loading data from another domain than the web page is loaded is possible but is 
a subject for `Access-Control-Allow-Origin` policies defined by the web server 
you are loading data from.

For more about loading data across domains use the following thread:
http://stackoverflow.com/questions/1653308/access-control-allow-origin-multiple-origin-domains


## Usage

### 1) Include the minified version of file of this plugin. I.e.:

```
<script src="amcharts/plugins/dataloader/dataloader.min.js" type="text/javascript"></script>
```

(this needs to go after all the other amCharts includes)

### 2) Add data source properties to your chart configuration.

Regular (Serial, Pie, etc.) charts:

```
AmCharts.makeChart( "chartdiv", {
  ...,
  "dataLoader": {
    "url": "data.json",
    "format": "json"
  }
} );
```

Stock chart:

```
AmCharts.makeChart( "chartdiv", {
  ...,
  "dataSets": [{
    ...,
    "dataLoader": {
      "url": "data.csv"
      "format": "csv",
      "delimiter": ",",       // column separator
      "useColumnNames": true, // use first row for column names
      "skip": 1               // skip header row
    }
  }]
} );
```

That's it. The plugin will make sure the files are loaded and dataProvider is 
populated with their content *before* the chart is built.

Some formats, like CSV, will require additional parameters needed to parse the 
data, such as "separator".

If the "format" is omitted, the plugin will assume JSON.


## Complete list of available dataLoader settings

Property | Default | Description
-------- | ------- | -----------
async | true | If set to false (not recommended) everything will wait until data is fully loaded
delimiter | , | [CSV only] a delimiter for columns (use \t for tab delimiters)
format | json | Type of data: json, csv
noStyles | false | If set to true no styles will be applied to "Data loading" curtain
postProcess | | If set to function reference, that function will be called to "post-process" loaded data before passing it on to chart
showErrors | true | Show loading errors in a chart curtain
showCurtain | true| Show curtain over the chart area when loading data
reload | 0 | Reload data every X seconds
reverse | false | [CSV only] add data points in revers order
skip | 0 | [CSV only] skip X first rows in data (includes first row if useColumnNames is used)
timestamp | false | Add current timestamp to data URLs (to avoid caching)
useColumnNames | false | [CSV only] Use first row in data as column names when parsing


## Using in JavaScript Stock Chart

In JavaScript Stock Chart it works exactly the same as in other chart types, 
with the exception that `dataLoader` is set as a property to the data set 
definition. I.e.:

```
var chart = AmCharts.makeChart("chartdiv", {
  "type": "stock",
  ...
  "dataSets": [{
    "title": "MSFT",
      "fieldMappings": [{
        "fromField": "Open",
        "toField": "open"
      }, {
        "fromField": "High",
        "toField": "high"
      }, {
        "fromField": "Low",
        "toField": "low"
      }, {
        "fromField": "Close",
        "toField": "close"
      }, {
        "fromField": "Volume",
        "toField": "volume"
      }],
      "compared": false,
      "categoryField": "Date",
      "dataLoader": {
        "url": "data/MSFT.csv",
        "format": "csv",
        "showCurtain": true,
        "showErrors": true,
        "async": true,
        "reverse": true,
        "delimiter": ",",
        "useColumnNames": true
      }
    }
  }]
});
```

### Can I also load event data the same way?

Sure. You just add a `eventDataLoader` object to your data set. All the same 
settings apply.


## Translating into other languages

Depending on configuration options the plugin will display a small number of 
text prompts, like 'Data loading...'.

Plugin will try matching chart's `language` property and display text prompts in 
a corresponding language. For that the plugin needs to have the translations.

Some of the plugin translations are in **lang** subdirectory. Simply include the 
one you need.

If there is no translation to your language readily available, just grab en.js, 
copy it and translate.

The structure is simple:

```
'The phrase in English': 'Translation'
```

The phrase in English must be left intact.

When you're done, you can include your language as a JavaScript file.

P.S. send us your translation so we can include it for the benefits of other 
users. Thanks!


## Requirements

This plugin requires at least 3.13 version of JavaScript Charts, JavaScript
Stock Chart or JavaScript Maps.


## Demos

They're all in subdirectory /examples.


## Extending this plugin

You're encouraged to modify, extend and make derivative plugins out of this
plugin.

You can modify files, included in this archive or, better yet, fork this project
on GitHub:

https://github.com/amcharts/dataloader

We're curious types. Please let us know (contact@amcharts.com) if you do create
something new out of this plugin.


## License

This plugin is licensed under Apache License 2.0.

This basically means you're free to use or modify this plugin, even make your
own versions or completely different products out of it.

Please see attached file "license.txt" for the complete license or online here:

http://www.apache.org/licenses/LICENSE-2.0


## Contact us

* Email:contact@amcharts.com
* Web: http://www.amcharts.com/
* Facebook: https://www.facebook.com/amcharts
* Twitter: https://twitter.com/amcharts


## Changelog

### 1.0
* Added GANTT chart support

### 0.9.2
* Added global data load methods that can be used to load and parse data by code outside plugin
* Trim CSV column names
* Translation added: Lithuanian

### 0.9.1
* Fix chart animations not playing after asynchronous load

### 0.9
* Initial release