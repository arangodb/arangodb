# Webpack

## Build structure
- Define a new and clean build directory infrastructure

## Sass
- Concat and compile SASS / CSS with webpack

## Html & Templates
- Concat and compile HTML and EJS Templates webpack
- (remove ../frontend/html/test.html later)
- (remove ../frontend/build/templates.html later)

## Resolve & Alias
- Fix loading of images (e.g. included ones in css) in react dev mode (because we have defined paths in aardvark which we are not available here).


# Sources
- look for all `if (frontendConfig.react) {` statements and fix them properly (currently only for development)


# Libraries
- Leaflet.css (../frontend/css/leaflet.css) used to use "images" not "img", this is a custom change, rewrite that with webpack later
- removed Noty/noty library, was not working well with webpack (Search for a replacement)
- Prettify / Pretty (e.g. documentsView) not working with webpack, search for an alternative


# Additional
- Fix the login view properly. Image loading of community or enterprise is not included nicely. Needs cleanup!

# CSS
- Pure.css has isssues,  e.g. Nodes View: The header of the tabular view is misaligned. Currently don't know why.
