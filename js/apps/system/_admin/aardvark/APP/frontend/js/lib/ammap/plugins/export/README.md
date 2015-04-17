# amCharts Export

Version: 1.0


## Description

This plugin adds export capabilities to all amCharts products - charts and maps.

It allows annotating and exporting chart or related data to various bitmap, 
vector, document or data formats, such as PNG, JPG, PDF, SVG, JSON, XLSX and 
many more.


## Important notice

Please note that due to security measures implemented in modern browsers, some 
or all export options might not work if the web page is loaded locally (via 
file:///) or contain images loaded from different host than the web page itself.


## Usage

### 1) Include the minified version of file of this plugin as well as the 
bundled CSS file. I.e.:

```
<script src="amcharts/plugins/export/export.min.js"></script>
<link  type="text/css" href="../export.css" rel="stylesheet">
```

(this needs to go after all the other amCharts includes)

### 2) Enable `export` with default options:

```
AmCharts.makeChart( "chartdiv", {
  ...,
  "export": {
    "enabled": true,
    "libs": {
      "path": "../libs/"
    }
  }
} );
```

### ... OR set your own custom options:

```
AmCharts.makeChart( "chartdiv", {
  ...,
  "export": {
    "enabled": true,
    "libs": {
      "path": "../libs/"
    },
    "menu": [ {
      "class": "export-main",
      "menu": [ {
        "label": "Download",
        "menu": [ "PNG", "JPG", "CSV" ]
      }, {
        "label": "Annotate",
        "action": "draw",
        "menu": [ {
          "class": "export-drawing",
          "menu": [ "PNG", "JPG" ]
        } ]
      } ]
    } ]
  }
} );
```


## Loading external libraries needed for operation of this plugin

The plugin relies on a number of different libraries, to export images, draw 
annotations or generate download files.

Those libraries need to be loaded for the plugin to work properly.

There are two ways to load them. Choose the one that is right:

### 1) Automatic (preferred)

All libraries required for plugin operation are included withing plugins */libs* 
subdirectory.

If you want the plugin to load them on-demand (when it's needed for a certain 
operation), make sure you set the `path` proprty under `libs` object to a 
relative or absolute url.

If you are using relative url, note that it is relative to the web page you are 
displaying your chart on, not the export.js library.

### 2) Manual

You can also load all those JavaScript libraries by `<script>` tags. Here is a 
full list of the files that need to be loaded for each operation:

File | Located in | Required for
---- | ---------- | ------------
blob.js | libs/blob.js/ | Exporting to any image format
fabric.min.js | libs/fabric.js/ | Any export operation
FileSaver.js | libs/FileSaver.js/ | Used to offer download files
pdfmake.min.js | libs/pdfmake/ | Export to PDF format
vfs_fonts.js | libs/pdfmake/ | Export to PDF format
jszip.js | libs/jszip/ | Export to XLSX format
xlsx.js | libs/xlsx/ | Export to XLSX format


## Complete list of available export settings

Property | Default | Description
-------- | ------- | -----------
backgroundColor | #FFFFFF | RGB code of the color for the background of the exported image
enabled | true | Enables or disables export functionality
legend | {} | Places the legend in case it is within an external container
menu | [] | A list of menu or submenu items (see the next chapter for details)
fabric | {} | Overwrites the default drawing settings (Frabric library)
pdfMake | {} | Overwrites the default settings for PDF export (pdfMake library)
removeImages | true | If true export checks for and removes "tainted" images that area lodead from different domains


## Configuring export menu

Plugin includes a way to completely control what is displayed on export menu. 
You can set up menus, sub-menus, down to any level. You can even add custom 
items there that execute your arbitrary code on click. It's so configurable 
it makes us sick with power ;)

The top-level menu is configured via `menu` property under `export`. It should 
always be an array, even if you have a single item in it.

The array items could be either objects or format codes. Objects will allow you 
to specify labels, action, icon, child items and even custom code to be executed
on click.

Simple format codes will assume you need an export to that format.

### Simple menu setup

Here's a sample of the simple menu setup that allows export to PNG, JPG and CSV:

```
"export": {
  "enabled": true,
  "libs": {
    "path": "../libs/"
  },
  "menu": [ {
    "class": "export-main",
    "menu": [ "PNG", "JPG", "CSV" ]
  } ]
}
```

The above will display a menu out of three options when you hover on export 
icon:

* PNG
* JPG
* CSV

When clicked the plugin will trigger export to a respective format.

If that is all you need, you're all set.

Please note that we have wrapped out menu into another menu item, so that only 
the icon is displayed until we roll over the icon. This means that technically
we have a two-level hierarchical menu.

If we opmitted that first step, the chart would simply display a format list 
right there on the chart.

### Advanced menu setup

However, you can do so much more with the menu.

Let's add more formats and organize image and data formats into separate 
submenus.

To add a submenu to a menu item, simply add a `menu` array as its own property:

```
"export": {
  "enabled": true,
  "libs": {
    "path": "../libs/"
  },
  "menu": [ {
    "class": "export-main",
    "menu": [ {
      "label": "Download as image",
      "menu": [ "PNG", "JPG", "SVG" ]
    }, {
      "label": "Download data",
      "menu": [ "CSV", "XLSX" ]
    } ]
  } ]
}
```

Now we have a hierarchical menu with the following topology:

* Download as image
  * PNG
  * JPG
  * SVG
* Download data
  * CSV
  * XLSX

We can mix "string" and "object" formats the way we see fit, i.e.:

```
"menu": [
  "PNG", 
  { "label": "JPEG",
    "format": "JPG" },
  "SVG"
]
```

The magic does not end here, though.

### Adding custom click events to menu items

Just like we set `label` and `format` properties for menu item, we can set 
`click` as well.

This needs to be a function reference. I.e.:

```
"menu": [
  "PNG", 
  { "label": "JPEG",
    "click": function () {
      alert( "Clicked JPEG. Wow cool!" );
    } },
  "SVG"
]
```

### Printing the chart

Adding menu item to print the chart or map is as easy as adding export ones. You 
just use "PRINT" as `format`. I.e.:

```
"menu": [
  "PNG", 
  "SVG",
  "PRINT"
]
```

Or if you want to change the label:

```
"menu": [
  "PNG", 
  "SVG",
  { "format": "PRINT",
    "label": "Print Chart"
  }
]
```

### Annotating the chart before export

OK, this one is so cool, you'll need a class 700 insulation jacket.

By default each menu item triggers some kind of export. You can trigger an 
"annotation" mode instead by including `"action": "draw"` instead.

```
"menu": [ {
  "class": "export-main",
  "menu": [ {
    "label": "Download",
    "menu": [ "PNG", "JPG", "CSV", "XLSX" ]
  }, {
    "label": "Annotate",
    "action": "draw"
  } ]
} ]
```

Now, when you click on the "Annotate" item in the menu, the chart will turn into 
an image editor which you can actual draw on.

As cool as it may sound, there's little we can do if the annotated chart if we 
can't save the result image.

That's where sub-menus come for the rescue again:

```
"menu": [ {
  "class": "export-main",
  "menu": [ {
    "label": "Download",
    "menu": [ "PNG", "JPG", "CSV", "XLSX" ]
  }, {
    "label": "Annotate",
    "action": "draw",
    "menu": [ {
      "class": "export-drawing",
      "menu": [ "JPG", "PNG", "SVG", PDF" ]
    } ]
  } ]
} ]
```

Now, when you turn on the annotation mode, a submenu will display, allowing to 
export the image into either PNG,JPG,SVG or PDF.

And that's not even the end of it. You can add menu items to cancel, undo, redo.

```
"menu": [ {
  "class": "export-main",
  "menu": [ {
    "label": "Download",
    "menu": [ "PNG", "JPG", "CSV", "XLSX" ]
  }, {
    "label": "Annotate",
    "action": "draw",
    "menu": [ {
      "class": "export-drawing",
      "menu": [ {
        "label": "Edit",
        "menu": [ "UNDO", "REDO", "CANCEL" ]
      }, {
        "label": "Save",
        "format": "PNG"
      } ]
    } ]
  } ]
} ]
```

### A list of menu item properties

Property | Description
-------- | -----------
action | Set to "draw" if you want the item to trigger annotation mode
class | Class name applied to the tag
click | Function handler invoked upon click on menu item
format | A format to export chart/map to upon click (see below for a list of available formats)
icon | Icon file (will use chart's `pathToImages` if the URL is not full)
label | Text label to be displayed
menu | An array of submenu items
title | A title attribute of the link

Available `format` values:

* JPG
* PNG
* SVG
* CSV
* JSON
* PDF
* XLSX
* PRINT

### Exporting to PDF

When exporting to PDF, you can set and modify the content of the resulting 
document. I.e. add additional text and/or modify image size, etc.

To do that, you can use menu item's `content` property.

Each item in `content` represents either a text line (string) or an exported 
image.

To add a text line, simply use a string. It can even be a JavaScript variable or 
a function that returns a string.

To include exported image, use `image: "reference"`.

Additionally, you can add `fit` property which is an array of pixel dimensions, 
you want the image to be scaled to fit into.

Here's an example of such export menu item:

```
{
  "format": "PDF",
  "content": [ "Saved from:", window.location.href, {
    "image": "reference",
    "fit": [ 523.28, 769.89 ] // fit image to A4
  } ]
}
```


## Styling the export menu

The plugin comes with a default CSS file `export.css`. You just need to include 
it on your page.

Feel free to override any styles defined in it, create your own version and 
modify as you see fit.

If you choose to modify it, we suggest creating a copy so it does not get 
overwritten when you update amCharts or plugin.


## Plugin API

We explained how you can define custom functions to be executed on click on 
export menu items.

Those functions can tap into plugin's methods to augment it with some custom 
functionality.

Here's an example:

```
menu: [ {
  label: "JPG",
  click: function() {
    this.capture({},function() {
      this.toJPG( {}, function( data ) {
        this.download( data, "image/jpg", "amCharts.jpg" );
      });
    });
  }
} ]
```

The above will use plugin's internal `capture` method to capture it's current state and `toJPG()`
method to export the chart to JPEG format.

Yes, you're right, it's the exact equivalent of just including "JPG" string. The 
code is here for the explanatory purposes.

Here's a full list of API functions available for your consumption:

Function | Parameters | Description
-------- | ---------- | -----------
toJPG | (object) options, (function) callback | Prepares a JPEG representation of the chart and passes the binary data to the callback function
toPNG | (object) options, (function) callback | Prepares a PNG representation of the chart and passes the binary data to the callback function
toSVG | (object) options, (function) callback | Prepares a SVG representation of the chart and passes the binary data to the callback function
toPDF | (object) options, (function) callback | Prepares a PDF representation of the chart and passes the binary data to the callback function
toJSON | (object) options, (function) callback | Prepares a JSON and passes the plain data to the callback function
toCSV | (object) options, (function) callback | Prepares a CSV and passes the plain data to the callback function
toXLSX | (object) options, (function) callback | Prepares a XLSX representation of the chart and passes the binary data to the callback function
toBlob | (object) options, (function) callback | Prepares a BLOB and passes the instance to the callback function
toCanvas | (object) options, (function) callback | Prepares a Canvas and passes the element to the callback function
toArray | (object) options, (function) callback | Prepares an Array and passes the data to the callback function


## Requirements

This plugin requires at least 3.13 version of JavaScript Charts, JavaScript
Stock Chart or JavaScript Maps.

The export will also need relatively recent browsers.

IE10 and up are supported.

Partial support for IE9 will be implemented in the future versions.

IE8 and older are not supported I'm afraid. Hey, it's time to upgrade!


## Demos

They're all in subdirectory /examples.


## Extending this plugin

You're encouraged to modify, extend and make derivative plugins out of this
plugin.

You can modify files, included in this archive or, better yet, fork this project
on GitHub:

https://github.com/amcharts/export

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
* Initial release