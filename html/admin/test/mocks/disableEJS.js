/*var extend = function(cls) {
  function foo(){
  cls.apply(this, arguments);
  };

  foo.prototype = Object.create(cls.prototype);
  return foo;
  };

  window.EJS = extend(EJS);
  var old = EJS.prototype.constructor;
  console.log(123123);

  EJS.prototype.constructor = function () {
  console.log(444);
  console.log(arguments);
  return old();
  }

*/


//EJS = function () {};
//

