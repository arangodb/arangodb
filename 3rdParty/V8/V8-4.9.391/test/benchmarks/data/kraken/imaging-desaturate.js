/*
 * Pixastic Lib - Desaturation filter - v0.1.1
 * Copyright (c) 2008 Jacob Seidelin, jseidelin@nihilogic.dk, http://blog.nihilogic.dk/
 * License: [http://www.pixastic.com/lib/license.txt] (MPL 1.1)
 */

var Pixastic = {};
Pixastic.Actions = {};
Pixastic.Actions.desaturate = {
    process : function(params) {
        var useAverage = !!(params.options.average && params.options.average != "false");
        var data = params.data;
        var rect = params.options.rect;
        var w = rect.width;
        var h = rect.height;

        var p = w*h;
        var pix = p*4, pix1, pix2;

        if (useAverage) {
            while (p--) 
                data[pix-=4] = data[pix1=pix+1] = data[pix2=pix+2] = (data[pix]+data[pix1]+data[pix2])/3
        } else {
            while (p--)
                data[pix-=4] = data[pix1=pix+1] = data[pix2=pix+2] = (data[pix]*0.3 + data[pix1]*0.59 + data[pix2]*0.11);
        }
        return true;
    }
}

var params = {
    options: {
        rect: { width: width, height: height},
    },
    data: squidImageData
}

//XXX improve dataset rather than loop
for (var pixcounter = 0; pixcounter < 200; pixcounter++)
    Pixastic.Actions.desaturate.process(params);