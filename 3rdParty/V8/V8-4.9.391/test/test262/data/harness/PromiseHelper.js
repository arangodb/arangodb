//-----------------------------------------------------------------------------
function checkSequence(arr, message) {
    arr.forEach(function(e, i) {
        if (e !== (i+1)) {
            $ERROR((message ? message : "Steps in unexpected sequence:") +
                   " '" + arr.join(',') + "'");
        }
    });
}
