/**
 * This is a sample chart export config file. It is provided as a reference on
 * how miscelaneous items in export menu can be used and set up.
 *
 * Please refer to README.md for more information.
 */

/**
 * PDF-specfic configuration
 */
AmCharts.exportPDF = [ {
  label: "Undo",
  click: function() {
    this.drawing.undo();
  }
}, {
  label: "Redo",
  click: function() {
    this.drawing.redo();
  }
}, {
  label: "Cancel",
  click: function() {
    this.drawing.done();
  }
}, {
  label: "Save",
  menu: [ {
    label: "JPG",
    click: function() {
      this.drawing.done();
      this.toJPG( {}, function( data ) {
        this.download( data, "image/jpg", "amCharts.jpg" );
      } );
    }
  }, {
    label: "PNG",
    click: function() {
      this.drawing.done();
      this.toPNG( {}, function( data ) {
        this.download( data, "image/png", "amCharts.png" );
      } );
    }
  }, {
    label: "PDF",
    click: function() {
      this.drawing.done();
      this.toPDF( {}, function( data ) {
        this.download( data, "application/pdf", "amCharts.pdf" );
      } );
    }
  }, {
    label: "SVG",
    click: function() {
      this.drawing.done();
      this.toSVG( {}, function( data ) {
        this.download( data, "text/xml", "amCharts.svg" );
      } );
    }
  } ]
} ];


/**
 * Define main universal config
 */
AmCharts.exportCFG = {
  enabled: true,
  libs: {
    path: "../libs/"
  },
  menu: [ {
    icon: "export.png",
    menu: [
      /*
       ** DRAWING
       */
      {
        label: "Draw",
        click: function() {
          this.capture( {
            action: "draw",
            freeDrawingBrush: {
              width: 2,
              color: "#000000",
              shadow: {
                color: "rgba(0,0,0,0.3)",
                blur: 10,
                offsetX: 3,
                offsetY: 3
              }
            }
          }, function() {
            this.createMenu( AmCharts.exportPDF );
          } );
        }
      },

      /*
       ** DELAYED EXPORT
       */
      {
        label: "Delayed",
        click: function() {
          var _this = this;
          var delay = 2;
          var timer = 0;
          var starttime = Number( new Date() );
          var menu = this.createMenu( [ {
            label: "Capturing: " + delay,
            click: function() {
              clearTimeout( timer );
              this.createMenu( this.defaults.menu );
            }
          } ] );
          var label = menu.getElementsByTagName( "span" )[ 0 ];

          timer = setInterval( function() {
            diff = ( delay - ( Number( new Date() ) - starttime ) / 1000 );
            label.innerHTML = "Capturing: " + ( diff < 0 ? 0 : diff ).toFixed( 2 );

            if ( diff <= 0 ) {
              clearTimeout( timer );
              _this.capture( {}, function() {
                this.toJPG( {}, function( data ) {
                  this.download( data, "image/jpg", "amCharts.jpg" );
                } );
                this.createMenu( this.defaults.menu );
              } );
            }
          }, 10 );
        }
      },

      /*
       ** DELAYED EXPORT WITH DRAWING
       */
      {
        label: "Delayed Draw",
        click: function() {
          var _this = this;
          var delay = 2;
          var timer = 0;
          var starttime = Number( new Date() );
          var menu = this.createMenu( [ {
            label: "Capturing: " + delay,
            click: function() {
              clearTimeout( timer );
              this.createMenu( this.defaults.menu );
            }
          } ] );
          var label = menu.getElementsByTagName( "span" )[ 0 ];

          timer = setInterval( function() {
            var diff = ( delay - ( Number( new Date() ) - starttime ) / 1000 );
            label.innerHTML = "Capturing: " + ( diff < 0 ? 0 : diff ).toFixed( 2 );

            if ( diff <= 0 ) {
              clearTimeout( timer );
              _this.capture( {
                isDrawingMode: true
              }, function() {
                _this.createMenu( exportDrawingMenu );
              } );
            }
          }, 10 );
        }
      },

      /*
       ** DOWNLOAD
       */
      {
        label: "Download",
        menu: [ {
          label: "JPG",
          click: function() {
            this.capture( {}, function() {
              this.toJPG( {}, function( data ) {
                this.download( data, "image/jpg", "amCharts.jpg" );
              } );
            } );
          }
        }, {
          label: "PNG",
          click: function() {
            this.capture( {}, function() {
              this.toPNG( {}, function( data ) {
                this.download( data, "image/png", "amCharts.png" );
              } );
            } );
          }
        }, {
          label: "PDF",
          click: function() {
            this.capture( {}, function() {
              this.toPDF( {}, function( data ) {
                this.download( data, "application/pdf", "amCharts.pdf" );
              } );
            } );
          }
        }, {
          label: "PDF + data",
          click: function() {
            this.capture( {}, function() {
              var tableData = this.setup.chart.dataProvider;
              var tableBody = this.toArray( {
                stringify: true,
                data: tableData
              } );
              var tableHeader = [];
              var tableWidths = [];
              var content = [ {
                image: "reference"
              } ];

              for ( i in tableData[ 0 ] ) {
                tableHeader.push( i );
                tableWidths.push( "*" );
              }
              tableBody.unshift( tableHeader );

              content.push( {
                table: {
                  headerRows: 1,
                  widths: tableWidths,
                  body: tableBody
                },
                layout: 'lightHorizontalLines'
              } );

              this.toPDF( {
                content: content
              }, function( data ) {
                this.download( data, "application/pdf", "amCharts.pdf" );
              } );
            } );
          }
        }, {
          label: "SVG",
          click: function() {
            this.capture( {}, function() {
              this.toSVG( {}, function( data ) {
                this.download( data, "text/xml", "amCharts.svg" );
              } );
            } );
          }
        }, {
          label: "CSV",
          click: function() {
            this.toCSV( {}, function( data ) {
              this.download( data, "text/plain", "amCharts.csv" );
            } );
          }
        }, {
          label: "JSON",
          click: function() {
            this.toJSON( {}, function( data ) {
              this.download( data, "text/plain", "amCharts.json" );
            } );
          }
        }, {
          label: "XLSX",
          click: function() {
            this.toXLSX( {}, function( data ) {
              this.download( data, "application/octet-stream", "amCharts.xlsx" );
            } );
          }
        } ]
      }
    ]
  } ]
};