- Concat and compile SASS / CSS with webpack

- Concat and compile HTML and EJS Templates webpack
- (remove ../frontend/html/test.html later)
- (remove ../frontend/build/templates.html later)

- Fix loading of images (e.g. included ones in css) in react dev mode (because we have defined paths in aardvark which we are not available here).

- Leaflet.css (../frontend/css/leaflet.css) used to use "images" not "img", this is a custom change, rewrite that with webpack later
