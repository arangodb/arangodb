if (typeof $ === "function"){ 
    $( document ).ready(function() {
        console.log("Configure hljs");
        hljs.configure({languages:['cpp']});
    });
}
