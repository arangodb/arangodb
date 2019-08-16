# Webpack

# OPEN

## Important
- make react aware if we're in cluster env or not (clusterHealth)
- check build environment
- check starter copy js methods

## Features
- Shard Distribution View needs to be finalized. Currently poc stace. 

# TODO DEVs (optional)

# Sources
- look for all `if (frontendConfig.react) {` statements and fix them properly (currently only for development)

## Resolve & Alias
- Fix loading of images (e.g. included ones in css) in react dev mode (because we have defined paths in aardvark which we are not available here).

# Notes
- Aardvark: What is defaultDocument. Is this still "working" and "needed"? This change does not change anything. See: 
  -  "defaultDocument": "index.html",
  -  "defaultDocument": "react/build/index.html",

.
.
.
.


# DONE

- remove all not needed files
- move all old build files to new react folder.

## Finalizing build
- ~~Setup react dev and production setups.~~ (Done)
- ~~hard Reload on graphs views, create new, enter something => auto drops out (important, need to fix this) (DONE)

## Libraries
- ~~Leaflet.css (../frontend/css/leaflet.css) used to use "images" not "img", this is a custom change, rewrite that with webpack later~~ (Done)
- Updated Noty/noty library, was not working well with webpack (Search for a replacement) (Done)
- Prettify / Pretty (e.g. documentsView) not working with webpack, search for an alternative (Done)

## HTML
- `<body style="margin-top: -10px">` <-- REMOVE ME, the origin is react root element. (Done)

## Html & Templates
- ~~Concat and compile HTML and EJS Templates webpack~~ (Done)
- ~~(remove ../frontend/html/test.html later)~~ (Done)
- ~~(remove ../frontend/build/templates.html later)~~ (Done)

## Fix Ace
- ~~ DONE

## Build structure
- ~~Define a new and clean build directory infrastructure~~ (Done)
- ~~Define a new and clean img directory as well (../frontend/img/ArangoDB-community-edition-Web-UI.svg) compressed vs non compresses svg's etc. REFACTOR!!!~~ (Done)

## Sass
- ~~Concat and compile SASS / CSS with webpack~~ (Done)

## Finalizing build
- Change "make frontend"

## CSS
- Pure.css has isssues,  e.g. Nodes View: The header of the tabular view is misaligned. Currently don't know why.
- Fix the login view properly. Image loading of community or enterprise is not included nicely. Needs cleanup!

