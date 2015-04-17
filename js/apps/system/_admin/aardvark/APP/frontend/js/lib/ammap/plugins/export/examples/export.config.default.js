/**
 * This is a sample chart export config file. It is provided as a reference on
 * how miscelaneous items in export menu can be used and set up.
 *
 * You do not need to use this file. It contains default export menu options 
 * that will be shown if you do not provide any "menu" in your export config.
 *
 * Please refer to README.md for more information.
 */


/**
 * PDF-specfic configuration
 */
AmCharts.exportPDF = {
	"format": "PDF",
	"content": [ "Saved from:", window.location.href, {
		"image": "reference",
		"fit": [ 523.28, 769.89 ] // fit image to A4
	} ]
};

/**
 * Print-specfic configuration
 */
AmCharts.exportPrint = {
	"format": "PRINT",
	"label": "Print"
};

/**
 * Define main universal config
 */
AmCharts.exportCFG = {
	"enabled": true,
	"libs": {
		"path": "../libs/"
	},
	"menu": [ {
		"class": "export-main",
		"label": "Export",
		"menu": [ {
			"label": "Download as ...",
			"menu": [ "PNG", "JPG", "SVG", AmCharts.exportPDF ]
		}, {
			"label": "Save data ...",
			"menu": [ "CSV", "XLSX", "JSON" ]
		}, {
			"label": "Annotate",
			"action": "draw",
			"menu": [ {
				"class": "export-drawing",
				"menu": [ {
					"label": "Color ...",
					"menu": [ {
						"class": "export-drawing-color export-drawing-color-black",
						"label": "Black",
						"click": function () {
							this.setup.fabric.freeDrawingBrush.color = "#000";
						}
					}, {
						"class": "export-drawing-color export-drawing-color-white",
						"label": "White",
						"click": function () {
							this.setup.fabric.freeDrawingBrush.color = "#fff";
						}
					}, {
						"class": "export-drawing-color export-drawing-color-red",
						"label": "Red",
						"click": function () {
							this.setup.fabric.freeDrawingBrush.color = "#f00";
						}
					}, {
						"class": "export-drawing-color export-drawing-color-green",
						"label": "Green",
						"click": function () {
							this.setup.fabric.freeDrawingBrush.color = "#0f0";
						}
					}, {
						"class": "export-drawing-color export-drawing-color-blue",
						"label": "Blue",
						"click": function () {
							this.setup.fabric.freeDrawingBrush.color = "#00f";
						}
					} ]
				}, "UNDO", "REDO", {
					"label": "Save as ...",
					"menu": [ "PNG", "JPG", "SVG", AmCharts.exportPDF ]
				}, AmCharts.exportPrint, "CANCEL" ]
			} ]
		}, AmCharts.exportPrint ]
	} ]
};