define('ace/ext/spellcheck', ['require', 'exports', 'module' ], function(require, exports, module) {


    text.spellcheck = !false;
    host.on("nativecontextmenu", function(e){
        if (!host.selection.isEmpty())
            return;
        var c = host.getCursorPosition();
        var r = host.session.getWordRange(c.row, c.column);
        var w = host.session.getTextRange(r);

        host.session.tokenRe.lastIndex = 0;
        if (!host.session.tokenRe.test(w))
            return;
        var e = w + " " + PLACEHOLDER;
        text.value = e;
        text.setSelectionRange(w.length + 1, w.length + 1);
        text.setSelectionRange(0, 0);

        inputHandler = function(newVal) {
            if (newVal == e)
                return '';
            if (newVal.lastIndexOf(e) == newVal.length - e.length)
                return newVal.slice(0, -e.length);
            if (newVal.indexOf(e) == 0)
                return newVal.slice(e.length);
            if (newVal.slice(-2) == PLACEHOLDER) {
                var val = newVal.slice(0, -2)
                if (val.slice(-1) == " ") {
                    val = val.slice(0, -1)
                    host.session.replace(r, val);
                    return val;
                }
            }
            
            return newVal;
        }
    });



});

