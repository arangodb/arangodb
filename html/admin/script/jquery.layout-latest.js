/**
 * @preserve jquery.layout 1.3.0 - Release Candidate 29.15
 * $Date: 2011-06-25 08:00:00 (Sat, 25 Jun 2011) $
 * $Rev: 302915 $
 *
 * Copyright (c) 2010 
 *   Fabrizio Balliano (http://www.fabrizioballiano.net)
 *   Kevin Dalman (http://allpro.net)
 *
 * Dual licensed under the GPL (http://www.gnu.org/licenses/gpl.html)
 * and MIT (http://www.opensource.org/licenses/mit-license.php) licenses.
 *
 * Changelog: http://layout.jquery-dev.net/changelog.cfm#1.3.0.rc29.15
 *
 * Docs: http://layout.jquery-dev.net/documentation.html
 * Tips: http://layout.jquery-dev.net/tips.html
 * Help: http://groups.google.com/group/jquery-ui-layout
 */

// NOTE: For best readability, view with a fixed-width font and tabs equal to 4-chars

;(function ($) {

/*
 *	GENERIC $.layout METHODS - used by all layouts
 */
$.layout = {

	version:	"1.3.rc29.15"
,	revision:	0.032915 // 1.3.0 final = 1.0300 - major(n+).minor(nn)+patch(nn+)

	// LANGUAGE CUSTOMIZATION
,	language: {
	//	Tips and messages for resizers, togglers, custom buttons, etc.
		Open:			"Open"	// eg: "Open Pane"
	,	Close:			"Close"
	,	Resize:			"Resize"
	,	Slide:			"Slide Open"
	,	Pin:			"Pin"
	,	Unpin:			"Un-Pin"
	,	noRoomToOpenTip: "Not enough room to show this pane."
	//	Developer error messages
	,	pane:					"pane"		// description of "layout pane element"
	,	selector:				"selector"	// description of "jQuery-selector"
	,	errButton:				"Error Adding Button \n\nInvalid "
	,	errContainerMissing:	"UI Layout Initialization Error\n\nThe specified layout-container does not exist."
	,	errCenterPaneMissing:	"UI Layout Initialization Error\n\nThe center-pane element does not exist.\n\nThe center-pane is a required element."
	,	errContainerHeight:		"UI Layout Initialization Warning\n\nThe layout-container \"CONTAINER\" has no height.\n\nTherefore the layout is 0-height and hence 'invisible'!"
	}

	// can update code here if $.browser is phased out
,	browser: {
		mozilla:	!!$.browser.mozilla
	,	webkit:		!!$.browser.webkit || !!$.browser.safari // webkit = jQ 1.4
	,	msie:		!!$.browser.msie
	,	isIE6:		!!$.browser.msie && $.browser.version == 6
	,	boxModel:	false	// page must load first, so will be updated set by _create
	//,	version:	$.browser.version - not used
	}

	/*
	*	GENERIC UTILITY METHODS
	*/

	// calculate and return the scrollbar width, as an integer
,	scrollbarWidth:		function () { return window.scrollbarWidth  || $.layout.getScrollbarSize('width'); }
,	scrollbarHeight:	function () { return window.scrollbarHeight || $.layout.getScrollbarSize('height'); }
,	getScrollbarSize:	function (dim) {
		var $c	= $('<div style="position: absolute; top: -10000px; left: -10000px; width: 100px; height: 100px; overflow: scroll;"></div>').appendTo("body");
		var d	= { width: $c.width() - $c[0].clientWidth, height: $c.height() - $c[0].clientHeight };
		$c.remove();
		window.scrollbarWidth	= d.width;
		window.scrollbarHeight	= d.height;
		return dim.match(/^(width|height)$/i) ? d[dim] : d;
	}


	/**
	* Returns hash container 'display' and 'visibility'
	*
	* @see	$.swap() - swaps CSS, runs callback, resets CSS
	*/
,	showInvisibly: function ($E, force) {
		if (!$E) return {};
		if (!$E.jquery) $E = $($E);
		var CSS = {
			display:	$E.css('display')
		,	visibility:	$E.css('visibility')
		};
		if (force || CSS.display == "none") { // only if not *already hidden*
			$E.css({ display: "block", visibility: "hidden" }); // show element 'invisibly' so can be measured
			return CSS;
		}
		else return {};
	}

	/**
	* Returns data for setting size of an element (container or a pane).
	*
	* @see  _create(), onWindowResize() for container, plus others for pane
	* @return JSON  Returns a hash of all dimensions: top, bottom, left, right, outerWidth, innerHeight, etc
	*/
,	getElementDimensions: function ($E) {
		var
			d	= {}			// dimensions hash
		,	x	= d.css = {}	// CSS hash
		,	i	= {}			// TEMP insets
		,	b, p				// TEMP border, padding
		,	N	= $.layout.cssNum
		,	off = $E.offset()
		;
		d.offsetLeft = off.left;
		d.offsetTop  = off.top;

		$.each("Left,Right,Top,Bottom".split(","), function (idx, e) { // e = edge
			b = x["border" + e] = $.layout.borderWidth($E, e);
			p = x["padding"+ e] = $.layout.cssNum($E, "padding"+e);
			i[e] = b + p; // total offset of content from outer side
			d["inset"+ e] = p;
		});

		d.offsetWidth	= $E.innerWidth();
		d.offsetHeight	= $E.innerHeight();
		d.outerWidth	= $E.outerWidth();
		d.outerHeight	= $E.outerHeight();
		d.innerWidth	= Math.max(0, d.outerWidth  - i.Left - i.Right);
		d.innerHeight	= Math.max(0, d.outerHeight - i.Top  - i.Bottom);

		x.width		= $E.width();
		x.height	= $E.height();
		x.top		= N($E,"top",true);
		x.bottom	= N($E,"bottom",true);
		x.left		= N($E,"left",true);
		x.right		= N($E,"right",true);

		//d.visible	= $E.is(":visible");// && x.width > 0 && x.height > 0;

		return d;
	}

,	getElementCSS: function ($E, list) {
		var
			CSS	= {}
		,	style	= $E[0].style
		,	props	= list.split(",")
		,	sides	= "Top,Bottom,Left,Right".split(",")
		,	attrs	= "Color,Style,Width".split(",")
		,	p, s, a, i, j, k
		;
		for (i=0; i < props.length; i++) {
			p = props[i];
			if (p.match(/(border|padding|margin)$/))
				for (j=0; j < 4; j++) {
					s = sides[j];
					if (p == "border")
						for (k=0; k < 3; k++) {
							a = attrs[k];
							CSS[p+s+a] = style[p+s+a];
						}
					else
						CSS[p+s] = style[p+s];
				}
			else
				CSS[p] = style[p];
		};
		return CSS
	}

	/**
	* Contains logic to check boxModel & browser, and return the correct width/height for the current browser/doctype
	*
	* @see  initPanes(), sizeMidPanes(), initHandles(), sizeHandles()
	* @param  {Array.<Object>}	$E  Must pass a jQuery object - first element is processed
	* @param  {number=}			outerWidth/outerHeight  (optional) Can pass a width, allowing calculations BEFORE element is resized
	* @return {number}		Returns the innerWidth/Height of the elem by subtracting padding and borders
	*/
,	cssWidth: function ($E, outerWidth) {
		var
			b = $.layout.borderWidth
		,	n = $.layout.cssNum
		;
		// a 'calculated' outerHeight can be passed so borders and/or padding are removed if needed
		if (outerWidth <= 0) return 0;

		if (!$.layout.browser.boxModel) return outerWidth;

		// strip border and padding from outerWidth to get CSS Width
		var W = outerWidth
			- b($E, "Left")
			- b($E, "Right")
			- n($E, "paddingLeft")		
			- n($E, "paddingRight")
		;

		return Math.max(0,W);
	}

,	cssHeight: function ($E, outerHeight) {
		var
			b = $.layout.borderWidth
		,	n = $.layout.cssNum
		;
		// a 'calculated' outerHeight can be passed so borders and/or padding are removed if needed
		if (outerHeight <= 0) return 0;

		if (!$.layout.browser.boxModel) return outerHeight;

		// strip border and padding from outerHeight to get CSS Height
		var H = outerHeight
			- b($E, "Top")
			- b($E, "Bottom")
			- n($E, "paddingTop")
			- n($E, "paddingBottom")
		;

		return Math.max(0,H);
	}

	/**
	* Returns the 'current CSS numeric value' for a CSS property - 0 if property does not exist
	*
	* @see  Called by many methods
	* @param {Array.<Object>}	$E			Must pass a jQuery object - first element is processed
	* @param {string}			prop		The name of the CSS property, eg: top, width, etc.
	* @param {boolean=}			allowAuto	true = return 'auto' if that is value; false = return 0
	* @return {(string|number)}				Usually used to get an integer value for position (top, left) or size (height, width)
	*/
,	cssNum: function ($E, prop, allowAuto) {
		if (!$E.jquery) $E = $($E);
		var CSS = $.layout.showInvisibly($E)
		,	p	= $.curCSS($E[0], prop, true)
		,	v	= allowAuto && p=="auto" ? p : (parseInt(p, 10) || 0);
		$E.css( CSS ); // RESET
		return v;
	}

,	borderWidth: function (el, side) {
		if (el.jquery) el = el[0];
		var b = "border"+ side.substr(0,1).toUpperCase() + side.substr(1); // left => Left
		return $.curCSS(el, b+"Style", true) == "none" ? 0 : (parseInt($.curCSS(el, b+"Width", true), 10) || 0);
	}


	/**
	* UTLITY for mouse tracking - FUTURE REFERENCE
	*
	* init: if (!window.mouse) {
	*			window.mouse = { x: 0, y: 0 };
	*			$(document).mousemove( $.layout.trackMouse );
	*		}
	*
	* @param {Object}		evt
	*
,	trackMouse: function (evt) {
		window.mouse = { x: evt.clientX, y: evt.clientY };
	}
	*/

	/**
	* SUBROUTINE for preventPrematureSlideClose option
	*
	* @param {Object}		evt
	* @param {Object=}		el
	*/
,	isMouseOverElem: function (evt, el) {
		var
			$E	= $(el || this)
		,	d	= $E.offset()
		,	T	= d.top
		,	L	= d.left
		,	R	= L + $E.outerWidth()
		,	B	= T + $E.outerHeight()
		,	x	= evt.pageX	// evt.clientX ?
		,	y	= evt.pageY	// evt.clientY ?
		;
		// if X & Y are < 0, probably means is over an open SELECT
		return ($.layout.browser.msie && x < 0 && y < 0) || ((x >= L && x <= R) && (y >= T && y <= B));
	}

};

$.fn.layout = function (opts) {

/*
 * ###########################
 *   WIDGET CONFIG & OPTIONS
 * ###########################
 */
	var

	// LANGUAGE - for tips & messages
	lang = $.layout.language // internal alias

	// DEFAULT OPTIONS - CHANGE IF DESIRED
,	options = {
		name:						""			// Not required, but useful for buttons and used for the state-cookie
	,	containerClass:				"ui-layout-container" // layout-container element
	,	scrollToBookmarkOnLoad:		true		// after creating a layout, scroll to bookmark in URL (.../page.htm#myBookmark)
	,	resizeWithWindow:			true		// bind thisLayout.resizeAll() to the window.resize event
	,	resizeWithWindowDelay:		200			// delay calling resizeAll because makes window resizing very jerky
	,	resizeWithWindowMaxDelay:	0			// 0 = none - force resize every XX ms while window is being resized
	,	onresizeall_start:			null		// CALLBACK when resizeAll() STARTS	- NOT pane-specific
	,	onresizeall_end:			null		// CALLBACK when resizeAll() ENDS	- NOT pane-specific
	,	onload_start:				null		// CALLBACK when Layout inits - after options initialized, but before elements
	,	onload_end:					null		// CALLBACK when Layout inits - after EVERYTHING has been initialized
	,	onunload_start:				null		// CALLBACK when Layout is destroyed OR onWindowUnload
	,	onunload_end:				null		// CALLBACK when Layout is destroyed OR onWindowUnload
	,	autoBindCustomButtons:		false		// search for buttons with ui-layout-button class and auto-bind them
	,	zIndex:						null		// the PANE zIndex - resizers and masks will be +1
	,	initPanes:					true		// false = DO NOT initialize the panes onLoad - will init later
	,	showErrorMessages:			true		// enables fatal error messages to warn developers of common errors
	//	PANE SETTINGS
	,	defaults: { // default options for 'all panes' - will be overridden by 'per-pane settings'
			applyDemoStyles: 		false		// NOTE: renamed from applyDefaultStyles for clarity
		,	closable:				true		// pane can open & close
		,	resizable:				true		// when open, pane can be resized 
		,	slidable:				true		// when closed, pane can 'slide open' over other panes - closes on mouse-out
		,	initClosed:				false		// true = init pane as 'closed'
		,	initHidden: 			false 		// true = init pane as 'hidden' - no resizer-bar/spacing
		//	SELECTORS
		//,	paneSelector:			""			// MUST be pane-specific - jQuery selector for pane
		,	contentSelector:		".ui-layout-content" // INNER div/element to auto-size so only it scrolls, not the entire pane!
		,	contentIgnoreSelector:	".ui-layout-ignore"	// element(s) to 'ignore' when measuring 'content'
		,	findNestedContent:		false		// true = $P.find(contentSelector), false = $P.children(contentSelector)
		//	GENERIC ROOT-CLASSES - for auto-generated classNames
		,	paneClass:				"ui-layout-pane"	// border-Pane - default: 'ui-layout-pane'
		,	resizerClass:			"ui-layout-resizer"	// Resizer Bar		- default: 'ui-layout-resizer'
		,	togglerClass:			"ui-layout-toggler"	// Toggler Button	- default: 'ui-layout-toggler'
		,	buttonClass:			"ui-layout-button"	// CUSTOM Buttons	- default: 'ui-layout-button-toggle/-open/-close/-pin'
		//	ELEMENT SIZE & SPACING
		//,	size:					100			// MUST be pane-specific -initial size of pane
		,	minSize:				0			// when manually resizing a pane
		,	maxSize:				0			// ditto, 0 = no limit
		,	spacing_open:			6			// space between pane and adjacent panes - when pane is 'open'
		,	spacing_closed:			6			// ditto - when pane is 'closed'
		,	togglerLength_open:		50			// Length = WIDTH of toggler button on north/south sides - HEIGHT on east/west sides
		,	togglerLength_closed: 	50			// 100% OR -1 means 'full height/width of resizer bar' - 0 means 'hidden'
		,	togglerAlign_open:		"center"	// top/left, bottom/right, center, OR...
		,	togglerAlign_closed:	"center"	// 1 => nn = offset from top/left, -1 => -nn == offset from bottom/right
		,	togglerTip_open:		lang.Close	// Toggler tool-tip (title)
		,	togglerTip_closed:		lang.Open	// ditto
		,	togglerContent_open:	""			// text or HTML to put INSIDE the toggler
		,	togglerContent_closed:	""			// ditto
		//	RESIZING OPTIONS
		,	resizerDblClickToggle:	true		// 
		,	autoResize:				true		// IF size is 'auto' or a percentage, then recalc 'pixel size' whenever the layout resizes
		,	autoReopen:				true		// IF a pane was auto-closed due to noRoom, reopen it when there is room? False = leave it closed
		,	resizerDragOpacity:		1			// option for ui.draggable
		//,	resizerCursor:			""			// MUST be pane-specific - cursor when over resizer-bar
		,	maskIframesOnResize:	true		// true = all iframes OR = iframe-selector(s) - adds masking-div during resizing/dragging
		,	resizeNestedLayout:		true		// true = trigger nested.resizeAll() when a 'pane' of this layout is the 'container' for another
		,	resizeWhileDragging:	false		// true = LIVE Resizing as resizer is dragged
		,	resizeContentWhileDragging:	false	// true = re-measure header/footer heights as resizer is dragged
		//	TIPS & MESSAGES - also see lang object
		,	noRoomToOpenTip:		lang.noRoomToOpenTip
		,	resizerTip:				lang.Resize	// Resizer tool-tip (title)
		,	sliderTip:				lang.Slide	// resizer-bar triggers 'sliding' when pane is closed
		,	sliderCursor:			"pointer"	// cursor when resizer-bar will trigger 'sliding'
		,	slideTrigger_open:		"click"		// click, dblclick, mouseenter
		,	slideTrigger_close:		"mouseleave"// click, mouseleave
		,	slideDelay_open:		300			// applies only for mouseenter event - 0 = instant open
		,	slideDelay_close:		300			// applies only for mouseleave event (300ms is the minimum!)
		,	hideTogglerOnSlide:		false		// when pane is slid-open, should the toggler show?
		,	preventQuickSlideClose:	$.layout.browser.webkit // Chrome triggers slideClosed as it is opening
		,	preventPrematureSlideClose: false
		//	HOT-KEYS & MISC
		,	showOverflowOnHover:	false		// will bind allowOverflow() utility to pane.onMouseOver
		,	enableCursorHotkey:		true		// enabled 'cursor' hotkeys
		//,	customHotkey:			""			// MUST be pane-specific - EITHER a charCode OR a character
		,	customHotkeyModifier:	"SHIFT"		// either 'SHIFT', 'CTRL' or 'CTRL+SHIFT' - NOT 'ALT'
		//	PANE ANIMATION
		//	NOTE: fxSss_open & fxSss_close options (eg: fxName_open) are auto-generated if not passed
		,	fxName:					"slide" 	// ('none' or blank), slide, drop, scale
		,	fxSpeed:				null		// slow, normal, fast, 200, nnn - if passed, will OVERRIDE fxSettings.duration
		,	fxSettings:				{}			// can be passed, eg: { easing: "easeOutBounce", duration: 1500 }
		,	fxOpacityFix:			true		// tries to fix opacity in IE to restore anti-aliasing after animation
		//	CALLBACKS
		,	triggerEventsOnLoad:	false		// true = trigger onopen OR onclose callbacks when layout initializes
		,	triggerEventsWhileDragging: true	// true = trigger onresize callback REPEATEDLY if resizeWhileDragging==true
		,	onshow_start:			null		// CALLBACK when pane STARTS to Show	- BEFORE onopen/onhide_start
		,	onshow_end:				null		// CALLBACK when pane ENDS being Shown	- AFTER  onopen/onhide_end
		,	onhide_start:			null		// CALLBACK when pane STARTS to Close	- BEFORE onclose_start
		,	onhide_end:				null		// CALLBACK when pane ENDS being Closed	- AFTER  onclose_end
		,	onopen_start:			null		// CALLBACK when pane STARTS to Open
		,	onopen_end:				null		// CALLBACK when pane ENDS being Opened
		,	onclose_start:			null		// CALLBACK when pane STARTS to Close
		,	onclose_end:			null		// CALLBACK when pane ENDS being Closed
		,	onresize_start:			null		// CALLBACK when pane STARTS being Resized ***FOR ANY REASON***
		,	onresize_end:			null		// CALLBACK when pane ENDS being Resized ***FOR ANY REASON***
		,	onsizecontent_start:	null		// CALLBACK when sizing of content-element STARTS
		,	onsizecontent_end:		null		// CALLBACK when sizing of content-element ENDS
		,	onswap_start:			null		// CALLBACK when pane STARTS to Swap
		,	onswap_end:				null		// CALLBACK when pane ENDS being Swapped
		,	ondrag_start:			null		// CALLBACK when pane STARTS being ***MANUALLY*** Resized
		,	ondrag_end:				null		// CALLBACK when pane ENDS being ***MANUALLY*** Resized
		}
	,	north: {
			paneSelector:			".ui-layout-north"
		,	size:					"auto"		// eg: "auto", "30%", 200
		,	resizerCursor:			"n-resize"	// custom = url(myCursor.cur)
		,	customHotkey:			""			// EITHER a charCode OR a character
		}
	,	south: {
			paneSelector:			".ui-layout-south"
		,	size:					"auto"
		,	resizerCursor:			"s-resize"
		,	customHotkey:			""
		}
	,	east: {
			paneSelector:			".ui-layout-east"
		,	size:					200
		,	resizerCursor:			"e-resize"
		,	customHotkey:			""
		}
	,	west: {
			paneSelector:			".ui-layout-west"
		,	size:					200
		,	resizerCursor:			"w-resize"
		,	customHotkey:			""
		}
	,	center: {
			paneSelector:			".ui-layout-center"
		,	minWidth:				0
		,	minHeight:				0
		}

	//	STATE MANAGMENT
	,	useStateCookie:				false		// Enable cookie-based state-management - can fine-tune with cookie.autoLoad/autoSave
	,	cookie: {
			name:					""			// If not specified, will use Layout.name, else just "Layout"
		,	autoSave:				true		// Save a state cookie when page exits?
		,	autoLoad:				true		// Load the state cookie when Layout inits?
		//	Cookie Options
		,	domain:					""
		,	path:					""
		,	expires:				""			// 'days' to keep cookie - leave blank for 'session cookie'
		,	secure:					false
		//	List of options to save in the cookie - must be pane-specific
		,	keys:					"north.size,south.size,east.size,west.size,"+
									"north.isClosed,south.isClosed,east.isClosed,west.isClosed,"+
									"north.isHidden,south.isHidden,east.isHidden,west.isHidden"
		}
	}


	// PREDEFINED EFFECTS / DEFAULTS
,	effects = { // LIST *PREDEFINED EFFECTS* HERE, even if effect has no settings
		slide:	{
			all:	{ duration:  "fast"	} // eg: duration: 1000, easing: "easeOutBounce"
		,	north:	{ direction: "up"	}
		,	south:	{ direction: "down"	}
		,	east:	{ direction: "right"}
		,	west:	{ direction: "left"	}
		}
	,	drop:	{
			all:	{ duration:  "slow"	} // eg: duration: 1000, easing: "easeOutQuint"
		,	north:	{ direction: "up"	}
		,	south:	{ direction: "down"	}
		,	east:	{ direction: "right"}
		,	west:	{ direction: "left"	}
		}
	,	scale:	{
			all:	{ duration:  "fast"	}
		}
	}


	// DYNAMIC DATA - IS READ-ONLY EXTERNALLY!
,	state = {
		// generate unique ID to use for event.namespace so can unbind only events added by 'this layout'
		id:			"layout"+ new Date().getTime()	// code uses alias: sID
	,	initialized: false
	,	container:	{} // init all keys
	,	north:		{}
	,	south:		{}
	,	east:		{}
	,	west:		{}
	,	center:		{}
	,	cookie:		{} // State Managment data storage
	}


	// INTERNAL CONFIG DATA - DO NOT CHANGE THIS!
,	_c = {
		allPanes:		"north,south,west,east,center"
	,	borderPanes:	"north,south,west,east"
	,	altSide: {
			north:	"south"
		,	south:	"north"
		,	east: 	"west"
		,	west: 	"east"
		}
	//	CSS used in multiple places
	,	hidden:  { visibility: "hidden" }
	,	visible: { visibility: "visible" }
	//	layout element settings
	,	zIndex: { // set z-index values here
			pane_normal:	1		// normal z-index for panes
		,	resizer_normal:	2		// normal z-index for resizer-bars
		,	iframe_mask:	2		// overlay div used to mask pane(s) during resizing
		,	pane_sliding:	100		// applied to *BOTH* the pane and its resizer when a pane is 'slid open'
		,	pane_animate:	1000	// applied to the pane when being animated - not applied to the resizer
		,	resizer_drag:	10000	// applied to the CLONED resizer-bar when being 'dragged'
		}
	,	resizers: {
			cssReq: {
				position: 	"absolute"
			,	padding: 	0
			,	margin: 	0
			,	fontSize:	"1px"
			,	textAlign:	"left"	// to counter-act "center" alignment!
			,	overflow: 	"hidden" // prevent toggler-button from overflowing
			//	SEE c.zIndex.resizer_normal
			}
		,	cssDemo: { // DEMO CSS - applied if: options.PANE.applyDemoStyles=true
				background: "#DDD"
			,	border:		"none"
			}
		}
	,	togglers: {
			cssReq: {
				position: 	"absolute"
			,	display: 	"block"
			,	padding: 	0
			,	margin: 	0
			,	overflow:	"hidden"
			,	textAlign:	"center"
			,	fontSize:	"1px"
			,	cursor: 	"pointer"
			,	zIndex: 	1
			}
		,	cssDemo: { // DEMO CSS - applied if: options.PANE.applyDemoStyles=true
				background: "#AAA"
			}
		}
	,	content: {
			cssReq: {
				position:	"relative" /* contain floated or positioned elements */
			}
		,	cssDemo: { // DEMO CSS - applied if: options.PANE.applyDemoStyles=true
				overflow:	"auto"
			,	padding:	"10px"
			}
		,	cssDemoPane: { // DEMO CSS - REMOVE scrolling from 'pane' when it has a content-div
				overflow:	"hidden"
			,	padding:	0
			}
		}
	,	panes: { // defaults for ALL panes - overridden by 'per-pane settings' below
			cssReq: {
				position: 	"absolute"
			,	margin:		0
			//	SEE c.zIndex.pane_normal
			}
		,	cssDemo: { // DEMO CSS - applied if: options.PANE.applyDemoStyles=true
				padding:	"10px"
			,	background:	"#FFF"
			,	border:		"1px solid #BBB"
			,	overflow:	"auto"
			}
		}
	,	north: {
			side:			"Top"
		,	sizeType:		"Height"
		,	dir:			"horz"
		,	cssReq: {
				top: 		0
			,	bottom: 	"auto"
			,	left: 		0
			,	right: 		0
			,	width: 		"auto"
			//	height: 	DYNAMIC
			}
		,	pins:			[]	// array of 'pin buttons' to be auto-updated on open/close (classNames)
		}
	,	south: {
			side:			"Bottom"
		,	sizeType:		"Height"
		,	dir:			"horz"
		,	cssReq: {
				top: 		"auto"
			,	bottom: 	0
			,	left: 		0
			,	right: 		0
			,	width: 		"auto"
			//	height: 	DYNAMIC
			}
		,	pins:			[]
		}
	,	east: {
			side:			"Right"
		,	sizeType:		"Width"
		,	dir:			"vert"
		,	cssReq: {
				left: 		"auto"
			,	right: 		0
			,	top: 		"auto" // DYNAMIC
			,	bottom: 	"auto" // DYNAMIC
			,	height: 	"auto"
			//	width: 		DYNAMIC
			}
		,	pins:			[]
		}
	,	west: {
			side:			"Left"
		,	sizeType:		"Width"
		,	dir:			"vert"
		,	cssReq: {
				left: 		0
			,	right: 		"auto"
			,	top: 		"auto" // DYNAMIC
			,	bottom: 	"auto" // DYNAMIC
			,	height: 	"auto"
			//	width: 		DYNAMIC
			}
		,	pins:			[]
		}
	,	center: {
			dir:			"center"
		,	cssReq: {
				left: 		"auto" // DYNAMIC
			,	right: 		"auto" // DYNAMIC
			,	top: 		"auto" // DYNAMIC
			,	bottom: 	"auto" // DYNAMIC
			,	height: 	"auto"
			,	width: 		"auto"
			}
		}
	}


/*
 * ###########################
 *  INTERNAL HELPER FUNCTIONS
 * ###########################
 */

	/**
	* Manages all internal timers
	*/
,	timer = {
		data:	{}
	,	set:	function (s, fn, ms) { timer.clear(s); timer.data[s] = setTimeout(fn, ms); }
	,	clear:	function (s) { var t=timer.data; if (t[s]) {clearTimeout(t[s]); delete t[s];} }
	}

	/**
	* Returns true if passed param is EITHER a simple string OR a 'string object' - otherwise returns false
	*/
,	isStr = function (o) {
		try { return typeof o == "string"
				 || (typeof o == "object" && o.constructor.toString().match(/string/i) !== null); }
		catch (e) { return false; }
	}

	/**
	* Returns a simple string if passed EITHER a simple string OR a 'string object',
	* else returns the original object
	*/
,	str = function (o) { // trim converts 'String object' to a simple string
		return isStr(o) ? $.trim(o) : o == undefined || o == null ? "" : o;
	}

	/**
	* min / max
	*
	* Aliases for Math methods to simplify coding
	*/
,	min = function (x,y) { return Math.min(x,y); }
,	max = function (x,y) { return Math.max(x,y); }

	/**
	* Processes the options passed in and transforms them into the format used by layout()
	* Missing keys are added, and converts the data if passed in 'flat-format' (no sub-keys)
	* In flat-format, pane-specific-settings are prefixed like: north__optName  (2-underscores)
	* To update effects, options MUST use nested-keys format, with an effects key ???
	*
	* @see	initOptions()
	* @param	{Object}	d	Data/options passed by user - may be a single level or nested levels
	* @return	{Object}		Creates a data struture that perfectly matches 'options', ready to be imported
	*/
,	_transformData = function (d) {
		var a, json = { cookie:{}, defaults:{fxSettings:{}}, north:{fxSettings:{}}, south:{fxSettings:{}}, east:{fxSettings:{}}, west:{fxSettings:{}}, center:{fxSettings:{}} };
		d = d || {};
		if (d.effects || d.cookie || d.defaults || d.north || d.south || d.west || d.east || d.center)
			json = $.extend( true, json, d ); // already in json format - add to base keys
		else
			// convert 'flat' to 'nest-keys' format - also handles 'empty' user-options
			$.each( d, function (key,val) {
				a = key.split("__");
				if (!a[1] || json[a[0]]) // check for invalid keys
					json[ a[1] ? a[0] : "defaults" ][ a[1] ? a[1] : a[0] ] = val;
			});
		return json;
	}

	/**
	* Set an INTERNAL callback to avoid simultaneous animation
	* Runs only if needed and only if all callbacks are not 'already set'
	* Called by open() and close() when isLayoutBusy=true
	*
	* @param {string}		action	Either 'open' or 'close'
	* @param {string}		pane	A valid border-pane name, eg 'west'
	* @param {boolean=}		param	Extra param for callback (optional)
	*/
,	_queue = function (action, pane, param) {
		var tried = [];

		// if isLayoutBusy, then some pane must be 'moving'
		$.each(_c.borderPanes.split(","), function (i, p) {
			if (_c[p].isMoving) {
				bindCallback(p); // TRY to bind a callback
				return false;	// BREAK
			}
		});

		// if pane does NOT have a callback, then add one, else follow the callback chain...
		function bindCallback (p) {
			var c = _c[p];
			if (!c.doCallback) {
				c.doCallback = true;
				c.callback = action +","+ pane +","+ (param ? 1 : 0);
			}
			else { // try to 'chain' this callback
				tried.push(p);
				var cbPane = c.callback.split(",")[1]; // 2nd param of callback is 'pane'
				// ensure callback target NOT 'itself' and NOT 'target pane' and NOT already tried (avoid loop)
				if (cbPane != pane && !$.inArray(cbPane, tried) >= 0)
					bindCallback(cbPane); // RECURSE
			}
		}
	}

	/**
	* RUN the INTERNAL callback for this pane - if one exists
	*
	* @param {string}	pane	A valid border-pane name, eg 'west'
	*/
,	_dequeue = function (pane) {
		var c = _c[pane];

		// RESET flow-control flags
		_c.isLayoutBusy = false;
		delete c.isMoving;
		if (!c.doCallback || !c.callback) return;

		c.doCallback = false; // RESET logic flag

		// EXECUTE the callback
		var
			cb = c.callback.split(",")
		,	param = (cb[2] > 0 ? true : false)
		;
		if (cb[0] == "open")
			open( cb[1], param  );
		else if (cb[0] == "close")
			close( cb[1], param );

		if (!c.doCallback) c.callback = null; // RESET - unless callback above enabled it again!
	}

	/**
	* Executes a Callback function after a trigger event, like resize, open or close
	*
	* @param {?string}				pane	This is passed only so we can pass the 'pane object' to the callback
	* @param {(string|function())}	v_fn	Accepts a function name, OR a comma-delimited array: [0]=function name, [1]=argument
	*/
,	_execCallback = function (pane, v_fn) {
		if (!v_fn) return;
		var fn;
		try {
			if (typeof v_fn == "function")
				fn = v_fn;	
			else if (!isStr(v_fn))
				return;
			else if (v_fn.match(/,/)) {
				// function name cannot contain a comma, so must be a function name AND a 'name' parameter
				var args = v_fn.split(",");
				fn = eval(args[0]);
				if (typeof fn=="function" && args.length > 1)
					return fn(args[1]); // pass the argument parsed from 'list'
			}
			else // just the name of an external function?
				fn = eval(v_fn);

			if (typeof fn=="function") {
				if (pane && $Ps[pane])
					// pass data: pane-name, pane-element, pane-state, pane-options, and layout-name
					return fn( pane, $Ps[pane], state[pane], options[pane], options.name );
				else // must be a layout/container callback - pass suitable info
					return fn( Instance, state, options, options.name );
			}
		}
		catch (ex) {}
	}

	/**
	* cure iframe display issues in IE & other browsers
	*/
,	_fixIframe = function (pane) {
		if ($.layout.browser.mozilla) return; // skip FireFox - it auto-refreshes iframes onShow
		var $P = $Ps[pane];
		// if the 'pane' is an iframe, do it
		if (state[pane].tagName == "IFRAME")
			$P.css(_c.hidden).css(_c.visible); 
		else // ditto for any iframes INSIDE the pane
			$P.find('IFRAME').css(_c.hidden).css(_c.visible);
	}

	/**
	* cssW / cssH / cssSize / cssMinDims
	*
	* Contains logic to check boxModel & browser, and return the correct width/height for the current browser/doctype
	*
	* @see  initPanes(), sizeMidPanes(), initHandles(), sizeHandles()
	* @param  {(string|!Object)}	el			Can accept a 'pane' (east, west, etc) OR a DOM object OR a jQuery object
	* @param  {number=}				outerWidth	(optional) Can pass a width, allowing calculations BEFORE element is resized
	* @return {number}							Returns the innerWidth of el by subtracting padding and borders
	*/
,	cssW = function (el, outerWidth) {
		var	str	= isStr(el)
		,	$E	= str ? $Ps[el] : $(el)
		;
		if (!$E.length) return 0;
		if (isNaN(outerWidth)) // not specified
			outerWidth = str ? getPaneSize(el) : $E.outerWidth();
		return $.layout.cssWidth($E, outerWidth);
	}

	/**
	* @param  {(string|!Object)}	el			Can accept a 'pane' (east, west, etc) OR a DOM object OR a jQuery object
	* @param  {number=}				outerHeight	(optional) Can pass a width, allowing calculations BEFORE element is resized
	* @return {number}				Returns the innerHeight el by subtracting padding and borders
	*/
,	cssH = function (el, outerHeight) {
		var	str	= isStr(el)
		,	$E	= str ? $Ps[el] : $(el)
		;
		if (!$E.length) return 0;
		if (isNaN(outerHeight)) // not specified
			outerHeight = str ? getPaneSize(el) : $E.outerHeight();
		return $.layout.cssHeight($E, outerHeight);
	}

	/**
	* @param  {string}		pane		Can accept ONLY a 'pane' (east, west, etc)
	* @param  {number=}		outerSize	(optional) Can pass a width, allowing calculations BEFORE element is resized
	* @return {number}		Returns the innerHeight/Width of el by subtracting padding and borders
	*/
,	cssSize = function (pane, outerSize) {
		if (_c[pane].dir=="horz") // pane = north or south
			return cssH(pane, outerSize);
		else // pane = east or west
			return cssW(pane, outerSize);
	}

	/**
	* @param  {string}		pane		Can accept ONLY a 'pane' (east, west, etc)
	* @return {Object}		Returns hash of minWidth & minHeight
	*/
,	cssMinDims = function (pane) {
		// minWidth/Height means CSS width/height = 1px
		var
			dir = _c[pane].dir
		,	d = {
				minWidth:	1001 - cssW(pane, 1000)
			,	minHeight:	1001 - cssH(pane, 1000)
			}
		;
		if (dir == "horz") d.minSize = d.minHeight;
		if (dir == "vert") d.minSize = d.minWidth;
		return d;
	}

	// TODO: see if these methods can be made more useful...
	// TODO: *maybe* return cssW/H from these so caller can use this info

	/**
	* @param {(string|!Object)}		el
	* @param {number=}				outerWidth
	* @param {boolean=}				autoHide
	*/
,	setOuterWidth = function (el, outerWidth, autoHide) {
		var $E = el, w;
		if (isStr(el)) $E = $Ps[el]; // west
		else if (!el.jquery) $E = $(el);
		w = cssW($E, outerWidth);
		$E.css({ width: w });
		if (w > 0) {
			if (autoHide && $E.data('autoHidden') && $E.innerHeight() > 0) {
				$E.show().data('autoHidden', false);
				if (!$.layout.browser.mozilla) // FireFox refreshes iframes - IE does not
					// make hidden, then visible to 'refresh' display after animation
					$E.css(_c.hidden).css(_c.visible);
			}
		}
		else if (autoHide && !$E.data('autoHidden'))
			$E.hide().data('autoHidden', true);
	}

	/**
	* @param {(string|!Object)}		el
	* @param {number=}				outerHeight
	* @param {boolean=}				autoHide
	*/
,	setOuterHeight = function (el, outerHeight, autoHide) {
		var $E = el, h;
		if (isStr(el)) $E = $Ps[el]; // west
		else if (!el.jquery) $E = $(el);
		h = cssH($E, outerHeight);
		$E.css({ height: h, visibility: "visible" }); // may have been 'hidden' by sizeContent
		if (h > 0 && $E.innerWidth() > 0) {
			if (autoHide && $E.data('autoHidden')) {
				$E.show().data('autoHidden', false);
				if (!$.layout.browser.mozilla) // FireFox refreshes iframes - IE does not
					$E.css(_c.hidden).css(_c.visible);
			}
		}
		else if (autoHide && !$E.data('autoHidden'))
			$E.hide().data('autoHidden', true);
	}

	/**
	* @param {(string|!Object)}		el
	* @param {number=}				outerSize
	* @param {boolean=}				autoHide
	*/
,	setOuterSize = function (el, outerSize, autoHide) {
		if (_c[pane].dir=="horz") // pane = north or south
			setOuterHeight(el, outerSize, autoHide);
		else // pane = east or west
			setOuterWidth(el, outerSize, autoHide);
	}


	/**
	* Converts any 'size' params to a pixel/integer size, if not already
	* If 'auto' or a decimal/percentage is passed as 'size', a pixel-size is calculated
	*
	/**
	* @param  {string}				pane
	* @param  {(string|number)=}	size
	* @param  {string=}				dir
	* @return {number}
	*/
,	_parseSize = function (pane, size, dir) {
		if (!dir) dir = _c[pane].dir;

		if (isStr(size) && size.match(/%/))
			size = parseInt(size, 10) / 100; // convert % to decimal

		if (size === 0)
			return 0;
		else if (size >= 1)
			return parseInt(size, 10);
		else if (size > 0) { // percentage, eg: .25
			var o = options, avail;
			if (dir=="horz") // north or south or center.minHeight
				avail = sC.innerHeight - ($Ps.north ? o.north.spacing_open : 0) - ($Ps.south ? o.south.spacing_open : 0);
			else if (dir=="vert") // east or west or center.minWidth
				avail = sC.innerWidth - ($Ps.west ? o.west.spacing_open : 0) - ($Ps.east ? o.east.spacing_open : 0);
			return Math.floor(avail * size);
		}
		else if (pane=="center")
			return 0;
		else { // size < 0 || size=='auto' || size==Missing || size==Invalid
			// auto-size the pane
			var
				$P	= $Ps[pane]
			,	dim	= (dir == "horz" ? "height" : "width")
			,	vis	= $.layout.showInvisibly($P) // show pane invisibly if hidden
			,	s	= $P.css(dim); // SAVE current size
			;
			$P.css(dim, "auto");
			size = (dim == "height") ? $P.outerHeight() : $P.outerWidth(); // MEASURE
			$P.css(dim, s).css(vis); // RESET size & visibility
			return size;
		}
	}

	/**
	* Calculates current 'size' (outer-width or outer-height) of a border-pane - optionally with 'pane-spacing' added
	*
	* @param  {(string|!Object)}	pane
	* @param  {boolean=}			inclSpace
	* @return {number}				Returns EITHER Width for east/west panes OR Height for north/south panes - adjusted for boxModel & browser
	*/
,	getPaneSize = function (pane, inclSpace) {
		var 
			$P	= $Ps[pane]
		,	o	= options[pane]
		,	s	= state[pane]
		,	oSp	= (inclSpace ? o.spacing_open : 0)
		,	cSp	= (inclSpace ? o.spacing_closed : 0)
		;
		if (!$P || s.isHidden)
			return 0;
		else if (s.isClosed || (s.isSliding && inclSpace))
			return cSp;
		else if (_c[pane].dir == "horz")
			return $P.outerHeight() + oSp;
		else // dir == "vert"
			return $P.outerWidth() + oSp;
	}

	/**
	* Calculate min/max pane dimensions and limits for resizing
	*
	* @param  {string}		pane
	* @param  {boolean=}	slide
	*/
,	setSizeLimits = function (pane, slide) {
		if (!isInitialized()) return;
		var 
			o				= options[pane]
		,	s				= state[pane]
		,	c				= _c[pane]
		,	dir				= c.dir
		,	side			= c.side.toLowerCase()
		,	type			= c.sizeType.toLowerCase()
		,	isSliding		= (slide != undefined ? slide : s.isSliding) // only open() passes 'slide' param
		,	$P				= $Ps[pane]
		,	paneSpacing		= o.spacing_open
		//	measure the pane on the *opposite side* from this pane
		,	altPane			= _c.altSide[pane]
		,	altS			= state[altPane]
		,	$altP			= $Ps[altPane]
		,	altPaneSize		= (!$altP || altS.isVisible===false || altS.isSliding ? 0 : (dir=="horz" ? $altP.outerHeight() : $altP.outerWidth()))
		,	altPaneSpacing	= ((!$altP || altS.isHidden ? 0 : options[altPane][ altS.isClosed !== false ? "spacing_closed" : "spacing_open" ]) || 0)
		//	limitSize prevents this pane from 'overlapping' opposite pane
		,	containerSize	= (dir=="horz" ? sC.innerHeight : sC.innerWidth)
		,	minCenterDims	= cssMinDims("center")
		,	minCenterSize	= dir=="horz" ? max(options.center.minHeight, minCenterDims.minHeight) : max(options.center.minWidth, minCenterDims.minWidth)
		//	if pane is 'sliding', then ignore center and alt-pane sizes - because 'overlays' them
		,	limitSize		= (containerSize - paneSpacing - (isSliding ? 0 : (_parseSize("center", minCenterSize, dir) + altPaneSize + altPaneSpacing)))
		,	minSize			= s.minSize = max( _parseSize(pane, o.minSize), cssMinDims(pane).minSize )
		,	maxSize			= s.maxSize = min( (o.maxSize ? _parseSize(pane, o.maxSize) : 100000), limitSize )
		,	r				= s.resizerPosition = {} // used to set resizing limits
		,	top				= sC.insetTop
		,	left			= sC.insetLeft
		,	W				= sC.innerWidth
		,	H				= sC.innerHeight
		,	rW				= o.spacing_open // subtract resizer-width to get top/left position for south/east
		;
		switch (pane) {
			case "north":	r.min = top + minSize;
							r.max = top + maxSize;
							break;
			case "west":	r.min = left + minSize;
							r.max = left + maxSize;
							break;
			case "south":	r.min = top + H - maxSize - rW;
							r.max = top + H - minSize - rW;
							break;
			case "east":	r.min = left + W - maxSize - rW;
							r.max = left + W - minSize - rW;
							break;
		};
	}

	/**
	* Returns data for setting the size/position of center pane. Also used to set Height for east/west panes
	*
	* @return JSON  Returns a hash of all dimensions: top, bottom, left, right, (outer) width and (outer) height
	*/
,	calcNewCenterPaneDims = function () {
		var d = {
			top:	getPaneSize("north", true) // true = include 'spacing' value for pane
		,	bottom:	getPaneSize("south", true)
		,	left:	getPaneSize("west", true)
		,	right:	getPaneSize("east", true)
		,	width:	0
		,	height:	0
		};

		// NOTE: sC = state.container
		// calc center-pane outer dimensions
		d.width		= sC.innerWidth - d.left - d.right;  // outerWidth
		d.height	= sC.innerHeight - d.bottom - d.top; // outerHeight
		// add the 'container border/padding' to get final positions relative to the container
		d.top		+= sC.insetTop;
		d.bottom	+= sC.insetBottom;
		d.left		+= sC.insetLeft;
		d.right		+= sC.insetRight;

		return d;
	}


	/**
	* Returns data for setting size of an element (container or a pane).
	*
	* @see  _create(), onWindowResize() for container, plus others for pane
	* @return JSON  Returns a hash of all dimensions: top, bottom, left, right, outerWidth, innerHeight, etc
	*/
,	elDims = function ($E) { return $.layout.getElementDimensions($E); }

,	elCSS = function ($E, list) { return $.layout.getElementCSS($E, list); }


	/**
	* @param {!Object}		el
	* @param {boolean=}		allStates
	*/
,	getHoverClasses = function (el, allStates) {
		var
			$El		= $(el)
		,	type	= $El.data("layoutRole")
		,	pane	= $El.data("layoutEdge")
		,	o		= options[pane]
		,	root	= o[type +"Class"]
		,	_pane	= "-"+ pane // eg: "-west"
		,	_open	= "-open"
		,	_closed	= "-closed"
		,	_slide	= "-sliding"
		,	_hover	= "-hover " // NOTE the trailing space
		,	_state	= $El.hasClass(root+_closed) ? _closed : _open
		,	_alt	= _state == _closed ? _open : _closed
		,	classes = (root+_hover) + (root+_pane+_hover) + (root+_state+_hover) + (root+_pane+_state+_hover)
		;
		if (allStates) // when 'removing' classes, also remove alternate-state classes
			classes += (root+_alt+_hover) + (root+_pane+_alt+_hover);

		if (type=="resizer" && $El.hasClass(root+_slide))
			classes += (root+_slide+_hover) + (root+_pane+_slide+_hover);

		return $.trim(classes);
	}
,	addHover	= function (evt, el) {
		var $E = $(el || this);
		if (evt && $E.data("layoutRole") == "toggler")
			evt.stopPropagation(); // prevent triggering 'slide' on Resizer-bar
		$E.addClass( getHoverClasses($E) );
	}
,	removeHover	= function (evt, el) {
		var $E = $(el || this);
		$E.removeClass( getHoverClasses($E, true) );
	}

,	onResizerEnter	= function (evt) {
		$('body').disableSelection();
		addHover(evt, this);
	}
,	onResizerLeave	= function (evt, el) {
		var
			e = el || this // el is only passed when called by the timer
		,	pane = $(e).data("layoutEdge")
		,	name = pane +"ResizerLeave"
		;
		timer.clear(pane+"_openSlider"); // cancel slideOpen timer, if set
		timer.clear(name); // cancel enableSelection timer - may re/set below
		if (!el) { // 1st call - mouseleave event
			removeHover(evt, this); // do this on initial call
			// this method calls itself on a timer because it needs to allow
			// enough time for dragging to kick-in and set the isResizing flag
			// dragging has a 100ms delay set, so this delay must be higher
			timer.set(name, function(){ onResizerLeave(evt, e); }, 200);
		}
		// if user is resizing, then dragStop will enableSelection() when done
		else if (!state[pane].isResizing) // 2nd call - by timer
			$('body').enableSelection();
	}

/*
 * ###########################
 *   INITIALIZATION METHODS
 * ###########################
 */

	/**
	* Initialize the layout - called automatically whenever an instance of layout is created
	*
	* @see  none - triggered onInit
	* @return  mixed	true = fully initialized | false = panes not initialized (yet) | 'cancel' = abort
	*/
,	_create = function () {
		// initialize config/options
		initOptions();
		var o = options;

		$.layout.browser.boxModel = $.support.boxModel;

		// update options with saved state, if option enabled
		if (o.useStateCookie && o.cookie.autoLoad)
			loadCookie(); // Update options from state-cookie

		// TEMP state so isInitialized returns true during init process
		state.creatingLayout = true;

		// options & state have been initialized, so now run beforeLoad callback
		// onload will CANCEL layout creation if it returns false
		if (false === _execCallback(null, o.onload_start))
			return 'cancel';

		// initialize the container element
		_initContainer();

		// bind hotkey function - keyDown - if required
		initHotkeys();

		// search for and bind custom-buttons
		if (o.autoBindCustomButtons) initButtons();

		// bind window.onunload
		$(window).bind("unload."+ sID, unload);

		// if layout elements are hidden, then layout WILL NOT complete initialization!
		// initLayoutElements will set initialized=true and run the onload callback IF successful
		if (o.initPanes) _initLayoutElements();

		delete state.creatingLayout;

		return state.initialized;
	}

	/**
	* Initialize the layout IF not already
	*
	* @see  All methods in Instance run this test
	* @return  boolean	true = layoutElements have been initialized | false = panes are not initialized (yet)
	*/
,	isInitialized = function () {
		if (state.initialized || state.creatingLayout) return true;	// already initialized
		else return _initLayoutElements();	// try to init panes NOW
	}

	/**
	* Initialize the layout - called automatically whenever an instance of layout is created
	*
	* @see  _create() & isInitialized
	* @return  An object pointer to the instance created
	*/
,	_initLayoutElements = function () {
		// initialize config/options
		var o = options;

		// CANNOT init panes inside a hidden container!
		if (!$N.is(":visible"))
			return false;
		// a center pane is required, so make sure it exists
		if (!getPane('center').length) {
			if (o.showErrorMessages) alert( lang.errCenterPaneMissing );
			return false;
		}

		// TEMP state so isInitialized returns true during init process
		state.creatingLayout = true;

		// update Container dims
		$.extend(sC, elDims( $N ));

 		// initialize all layout elements
		initPanes();	// size & position panes - calls initHandles() - which calls initResizable()
		sizeContent();	// AFTER panes & handles have been initialized, size 'content' divs

		if (o.scrollToBookmarkOnLoad) {
			var l = self.location;
			if (l.hash) l.replace( l.hash ); // scrollTo Bookmark
		}

		// bind resizeAll() for 'this layout instance' to window.resize event
		if (o.resizeWithWindow && !$N.data("layoutRole")) // skip if 'nested' inside a pane
			$(window).bind("resize."+ sID, windowResize);

		delete state.creatingLayout;
		state.initialized = true;

		_execCallback(null, o.onload_end || o.onload);

		return true; // elements initialized successfully
	}


,	windowResize = function () {
		var delay = Number(options.resizeWithWindowDelay);
		if (delay < 10) delay = 100; // MUST have a delay!
		// resizing uses a delay-loop because the resize event fires repeatly - except in FF, but delay anyway
		timer.clear("winResize"); // if already running
		timer.set("winResize", function(){
			timer.clear("winResize");
			timer.clear("winResizeRepeater");
			var dims = elDims( $N );
			// only trigger resizeAll() if container has changed size
			if (dims.innerWidth !== sC.innerWidth || dims.innerHeight !== sC.innerHeight)
				resizeAll();
		}, delay);
		// ALSO set fixed-delay timer, if not already running
		if (!timer.data["winResizeRepeater"]) setWindowResizeRepeater();
	}

,	setWindowResizeRepeater = function () {
		var delay = Number(options.resizeWithWindowMaxDelay);
		if (delay > 0)
			timer.set("winResizeRepeater", function(){ setWindowResizeRepeater(); resizeAll(); }, delay);
	}

,	unload = function () {
		var o = options;
		state.cookie = getState(); // save state in case onunload has custom state-management
		_execCallback(null, o.onunload_start);
		if (o.useStateCookie && o.cookie.autoSave) saveCookie();
		_execCallback(null, o.onunload_end || o.onunload);
	}

	/**
	* Validate and initialize container CSS and events
	*
	* @see  _create()
	*/
,	_initContainer = function () {
		var
			tag		= sC.tagName = $N[0].tagName
		,	o		= options
		,	fullPage= (tag == "BODY")
		,	props	= "overflow,position,margin,padding,border"
		,	CSS		= {}
		,	hid		= "hidden" // used A LOT!
		,	isVis	= $N.is(":visible")
		;
		// sC -> state.container
		sC.selector = $N.selector.split(".slice")[0];
		sC.ref		= tag +"/"+ sC.selector; // used in messages

		$N	.data("layout", Instance)
			.data("layoutContainer", sID)	// unique identifier for internal use
			.addClass(o.containerClass)
		;

		// SAVE original container CSS for use in destroy()
		if (!$N.data("layoutCSS")) {
			// handle props like overflow different for BODY & HTML - has 'system default' values
			if (fullPage) {
				CSS = $.extend( elCSS($N, props), {
					height:		$N.css("height")
				,	overflow:	$N.css("overflow")
				,	overflowX:	$N.css("overflowX")
				,	overflowY:	$N.css("overflowY")
				});
				// ALSO SAVE <HTML> CSS
				var $H = $("html");
				$H.data("layoutCSS", {
					height:		"auto" // FF would return a fixed px-size!
				,	overflow:	$H.css("overflow")
				,	overflowX:	$H.css("overflowX")
				,	overflowY:	$H.css("overflowY")
				});
			}
			else // handle props normally for non-body elements
				CSS = elCSS($N, props+",top,bottom,left,right,width,height,overflow,overflowX,overflowY");

			$N.data("layoutCSS", CSS);
		}

		try { // format html/body if this is a full page layout
			if (fullPage) {
				$("html").css({
					height:		"100%"
				,	overflow:	hid
				,	overflowX:	hid
				,	overflowY:	hid
				});
				$("body").css({
					position:	"relative"
				,	height:		"100%"
				,	overflow:	hid
				,	overflowX:	hid
				,	overflowY:	hid
				,	margin:		0
				,	padding:	0		// TODO: test whether body-padding could be handled?
				,	border:		"none"	// a body-border creates problems because it cannot be measured!
				});

				// set current layout-container dimensions
				$.extend(sC, elDims( $N ));
			}
			else { // set required CSS for overflow and position
				// ENSURE container will not 'scroll'
				CSS = { overflow: hid, overflowX: hid, overflowY: hid }
				var
					p = $N.css("position")
				,	h = $N.css("height")
				;
				// if this is a NESTED layout, then container/outer-pane ALREADY has position and height
				if (!$N.data("layoutRole")) {
					if (!p || !p.match(/fixed|absolute|relative/))
						CSS.position = "relative"; // container MUST have a 'position'
					/*
					if (!h || h=="auto")
						CSS.height = "100%"; // container MUST have a 'height'
					*/
				}
				$N.css( CSS );

				// set current layout-container dimensions
				if (isVis) {
					$.extend(sC, elDims( $N ));
					if (o.showErrorMessages && sC.innerHeight < 2)
						alert( lang.errContainerHeight.replace(/CONTAINER/, sC.ref) );
				}
			}
		} catch (ex) {}
	}

	/**
	* Bind layout hotkeys - if options enabled
	*
	* @see  _create() and addPane()
	* @param {string=}	panes		The edge(s) to process, blank = all
	*/
,	initHotkeys = function (panes) {
		if (!panes || panes == "all") panes = _c.borderPanes;
		// bind keyDown to capture hotkeys, if option enabled for ANY pane
		$.each(panes.split(","), function (i, pane) {
			var o = options[pane];
			if (o.enableCursorHotkey || o.customHotkey) {
				$(document).bind("keydown."+ sID, keyDown); // only need to bind this ONCE
				return false; // BREAK - binding was done
			}
		});
	}

	/**
	* Build final OPTIONS data
	*
	* @see  _create()
	*/
,	initOptions = function () {
		// simplify logic by making sure passed 'opts' var has basic keys
		opts = _transformData( opts );

		// TODO: create a compatibility add-on for new UI widget that will transform old option syntax
		var newOpts = {
			applyDefaultStyles:		"applyDemoStyles"
		};
		renameOpts(opts.defaults);
		$.each(_c.allPanes.split(","), function (i, pane) {
			renameOpts(opts[pane]);
		});

		// update default effects, if case user passed key
		if (opts.effects) {
			$.extend( effects, opts.effects );
			delete opts.effects;
		}
		$.extend( options.cookie, opts.cookie );

		// see if any 'global options' were specified
		var globals = "name,containerClass,zIndex,scrollToBookmarkOnLoad,resizeWithWindow,resizeWithWindowDelay,resizeWithWindowMaxDelay,"+
			"onresizeall,onresizeall_start,onresizeall_end,onload,onload_start,onload_end,onunload,onunload_start,onunload_end,"+
			"autoBindCustomButtons,useStateCookie,showErrorMessages";
		$.each(globals.split(","), function (i, key) {
			if (opts[key] !== undefined)
				options[key] = opts[key];
			else if (opts.defaults[key] !== undefined) {
				options[key] = opts.defaults[key];
				delete opts.defaults[key];
			}
		});

		// remove any 'defaults' that MUST be set 'per-pane'
		$.each("paneSelector,resizerCursor,customHotkey".split(","),
			function (i, key) { delete opts.defaults[key]; } // is OK if key does not exist
		);

		// now update options.defaults
		$.extend( true, options.defaults, opts.defaults );

		// merge config for 'center-pane' - border-panes handled in the loop below
		_c.center = $.extend( true, {}, _c.panes, _c.center );
		// update config.zIndex values if zIndex option specified
		var z = options.zIndex;
		if (z === 0 || z > 0) {
			_c.zIndex.pane_normal		= z;
			_c.zIndex.resizer_normal	= z+1;
			_c.zIndex.iframe_mask		= z+1;
		}

		// merge options for 'center-pane' - border-panes handled in the loop below
		$.extend( options.center, opts.center );
		// Most 'default options' do not apply to 'center', so add only those that DO
		var o_Center = $.extend( true, {}, options.defaults, opts.defaults, options.center ); // TEMP data
		var optionsCenter = ("paneClass,contentSelector,applyDemoStyles,triggerEventsOnLoad,showOverflowOnHover,"
		+	"onresize,onresize_start,onresize_end,resizeNestedLayout,resizeContentWhileDragging,findNestedContent,"
		+	"onsizecontent,onsizecontent_start,onsizecontent_end").split(",");
		$.each(optionsCenter,
			function (i, key) { options.center[key] = o_Center[key]; }
		);

		var o, defs = options.defaults;

		// create a COMPLETE set of options for EACH border-pane
		$.each(_c.borderPanes.split(","), function (i, pane) {

			// apply 'pane-defaults' to CONFIG.[PANE]
			_c[pane] = $.extend( true, {}, _c.panes, _c[pane] );

			// apply 'pane-defaults' +  user-options to OPTIONS.PANE
			o = options[pane] = $.extend( true, {}, options.defaults, options[pane], opts.defaults, opts[pane] );

			// make sure we have base-classes
			if (!o.paneClass)		o.paneClass		= "ui-layout-pane";
			if (!o.resizerClass)	o.resizerClass	= "ui-layout-resizer";
			if (!o.togglerClass)	o.togglerClass	= "ui-layout-toggler";

			// create FINAL fx options for each pane, ie: options.PANE.fxName/fxSpeed/fxSettings[_open|_close]
			$.each(["_open","_close",""], function (i,n) { 
				var
					sName		= "fxName"+n
				,	sSpeed		= "fxSpeed"+n
				,	sSettings	= "fxSettings"+n
				;
				// recalculate fxName according to specificity rules
				o[sName] =
					opts[pane][sName]		// opts.west.fxName_open
				||	opts[pane].fxName		// opts.west.fxName
				||	opts.defaults[sName]	// opts.defaults.fxName_open
				||	opts.defaults.fxName	// opts.defaults.fxName
				||	o[sName]				// options.west.fxName_open
				||	o.fxName				// options.west.fxName
				||	defs[sName]				// options.defaults.fxName_open
				||	defs.fxName				// options.defaults.fxName
				||	"none"
				;
				// validate fxName to be sure is a valid effect
				var fxName = o[sName];
				if (fxName == "none" || !$.effects || !$.effects[fxName] || (!effects[fxName] && !o[sSettings] && !o.fxSettings))
					fxName = o[sName] = "none"; // effect not loaded, OR undefined FX AND fxSettings not passed
				// set vars for effects subkeys to simplify logic
				var
					fx = effects[fxName]	|| {} // effects.slide
				,	fx_all	= fx.all		|| {} // effects.slide.all
				,	fx_pane	= fx[pane]		|| {} // effects.slide.west
				;
				// RECREATE the fxSettings[_open|_close] keys using specificity rules
				o[sSettings] = $.extend(
					{}
				,	fx_all						// effects.slide.all
				,	fx_pane						// effects.slide.west
				,	defs.fxSettings || {}		// options.defaults.fxSettings
				,	defs[sSettings] || {}		// options.defaults.fxSettings_open
				,	o.fxSettings				// options.west.fxSettings
				,	o[sSettings]				// options.west.fxSettings_open
				,	opts.defaults.fxSettings	// opts.defaults.fxSettings
				,	opts.defaults[sSettings] || {} // opts.defaults.fxSettings_open
				,	opts[pane].fxSettings		// opts.west.fxSettings
				,	opts[pane][sSettings] || {}	// opts.west.fxSettings_open
				);
				// recalculate fxSpeed according to specificity rules
				o[sSpeed] =
					opts[pane][sSpeed]		// opts.west.fxSpeed_open
				||	opts[pane].fxSpeed		// opts.west.fxSpeed (pane-default)
				||	opts.defaults[sSpeed]	// opts.defaults.fxSpeed_open
				||	opts.defaults.fxSpeed	// opts.defaults.fxSpeed
				||	o[sSpeed]				// options.west.fxSpeed_open
				||	o[sSettings].duration	// options.west.fxSettings_open.duration
				||	o.fxSpeed				// options.west.fxSpeed
				||	o.fxSettings.duration	// options.west.fxSettings.duration
				||	defs.fxSpeed			// options.defaults.fxSpeed
				||	defs.fxSettings.duration// options.defaults.fxSettings.duration
				||	fx_pane.duration		// effects.slide.west.duration
				||	fx_all.duration			// effects.slide.all.duration
				||	"normal"				// DEFAULT
				;
			});

		});

		function renameOpts (O) {
			for (var key in newOpts) {
				if (O[key] != undefined) {
					O[newOpts[key]] = O[key];
					delete O[key];
				}
			}
		}
	}

	/**
	* Initialize module objects, styling, size and position for all panes
	*
	* @see  _create()
	* @param {string}	pane		The pane to process
	*/
,	getPane = function (pane) {
		var sel = options[pane].paneSelector
		if (sel.substr(0,1)==="#") // ID selector
			// NOTE: elements selected 'by ID' DO NOT have to be 'children'
			return $N.find(sel).eq(0);
		else { // class or other selector
			var $P = $N.children(sel).eq(0);
			// look for the pane nested inside a 'form' element
			return $P.length ? $P : $N.children("form:first").children(sel).eq(0);
		}
	}
,	initPanes = function () {
		// NOTE: do north & south FIRST so we can measure their height - do center LAST
		$.each(_c.allPanes.split(","), function (idx, pane) {
			addPane( pane, true );
		});

		// init the pane-handles NOW in case we have to hide or close the pane below
		initHandles();

		// now that all panes have been initialized and initially-sized,
		// make sure there is really enough space available for each pane
		$.each(_c.borderPanes.split(","), function (i, pane) {
			if ($Ps[pane] && state[pane].isVisible) { // pane is OPEN
				setSizeLimits(pane);
				makePaneFit(pane); // pane may be Closed, Hidden or Resized by makePaneFit()
			}
		});
		// size center-pane AGAIN in case we 'closed' a border-pane in loop above
		sizeMidPanes("center");

		// Chrome fires callback BEFORE it completes resizing, so add a delay before handling children
		setTimeout(function(){
			$.each(_c.allPanes.split(","), function (i, pane) {
				var o = options[pane];
				if ($Ps[pane] && state[pane].isVisible) { // pane is OPEN
					// trigger onResize callbacks for all panes with triggerEventsOnLoad = true
					if (o.triggerEventsOnLoad)
						_execCallback(pane, o.onresize_end || o.onresize);
					resizeNestedLayout(pane);
				}
			});
		}, 50 ); // 50ms delay is enough

		if (options.showErrorMessages && $N.innerHeight() < 2)
			alert( lang.errContainerHeight.replace(/CONTAINER/, sC.ref) );
	}

	/**
	* Remove a pane from the layout - subroutine of destroy()
	*
	* @see  initPanes()
	* @param {string}	pane		The pane to process
	*/
,	addPane = function (pane, force) {
		if (!force && !isInitialized()) return;
		var
			o		= options[pane]
		,	s		= state[pane]
		,	c		= _c[pane]
		,	fx		= s.fx
		,	dir		= c.dir
		,	spacing	= o.spacing_open || 0
		,	isCenter = (pane == "center")
		,	CSS		= {}
		,	$P		= $Ps[pane]
		,	size, minSize, maxSize
		;

		// if pane-pointer already exists, remove the old one first
		if ($P)
			removePane( pane );
		else
			$Cs[pane] = false; // init

		$P = $Ps[pane] = getPane(pane);
		if (!$P.length) {
			$Ps[pane] = false; // logic
			return;
		}

		// SAVE original Pane CSS
		if (!$P.data("layoutCSS")) {
			var props = "position,top,left,bottom,right,width,height,overflow,zIndex,display,backgroundColor,padding,margin,border";
			$P.data("layoutCSS", elCSS($P, props));
		}

		// add basic classes & attributes
		$P
			.data("parentLayout", Instance)
			.data("layoutRole", "pane")
			.data("layoutEdge", pane)
			.css(c.cssReq).css("zIndex", _c.zIndex.pane_normal)
			.css(o.applyDemoStyles ? c.cssDemo : {}) // demo styles
			.addClass( o.paneClass +" "+ o.paneClass+"-"+pane ) // default = "ui-layout-pane ui-layout-pane-west" - may be a dupe of 'paneSelector'
			.bind("mouseenter."+ sID, addHover )
			.bind("mouseleave."+ sID, removeHover )
		;

		// see if this pane has a 'scrolling-content element'
		initContent(pane, false); // false = do NOT sizeContent() - called later

		if (!isCenter) {
			// call _parseSize AFTER applying pane classes & styles - but before making visible (if hidden)
			// if o.size is auto or not valid, then MEASURE the pane and use that as its 'size'
			size	= s.size = _parseSize(pane, o.size);
			minSize	= _parseSize(pane,o.minSize) || 1;
			maxSize	= _parseSize(pane,o.maxSize) || 100000;
			if (size > 0) size = max(min(size, maxSize), minSize);

			// state for border-panes
			s.isClosed  = false; // true = pane is closed
			s.isSliding = false; // true = pane is currently open by 'sliding' over adjacent panes
			s.isResizing= false; // true = pane is in process of being resized
			s.isHidden	= false; // true = pane is hidden - no spacing, resizer or toggler is visible!
		}
		//	state common to ALL panes
			s.tagName	= $P[0].tagName;
			s.edge		= pane   // useful if pane is (or about to be) 'swapped' - easy find out where it is (or is going)
			s.noRoom	= false; // true = pane 'automatically' hidden due to insufficient room - will unhide automatically
			s.isVisible	= true;  // false = pane is invisible - closed OR hidden - simplify logic

		// set css-position to account for container borders & padding
		switch (pane) {
			case "north": 	CSS.top 	= sC.insetTop;
							CSS.left 	= sC.insetLeft;
							CSS.right	= sC.insetRight;
							break;
			case "south": 	CSS.bottom	= sC.insetBottom;
							CSS.left 	= sC.insetLeft;
							CSS.right 	= sC.insetRight;
							break;
			case "west": 	CSS.left 	= sC.insetLeft; // top, bottom & height set by sizeMidPanes()
							break;
			case "east": 	CSS.right 	= sC.insetRight; // ditto
							break;
			case "center":	// top, left, width & height set by sizeMidPanes()
		}

		if (dir == "horz") // north or south pane
			CSS.height = max(1, cssH(pane, size));
		else if (dir == "vert") // east or west pane
			CSS.width = max(1, cssW(pane, size));
		//else if (isCenter) {}

		$P.css(CSS); // apply size -- top, bottom & height will be set by sizeMidPanes
		if (dir != "horz") sizeMidPanes(pane, true); // true = skipCallback

		// close or hide the pane if specified in settings
		if (o.initClosed && o.closable && !o.initHidden)
			close(pane, true, true); // true, true = force, noAnimation
		else if (o.initHidden || o.initClosed)
			hide(pane); // will be completely invisible - no resizer or spacing
		else if (!s.noRoom)
			// make the pane visible - in case was initially hidden
			$P.css("display","block");
		// ELSE setAsOpen() - called later by initHandles()

		// RESET visibility now - pane will appear IF display:block
		$P.css("visibility","visible");

		// check option for auto-handling of pop-ups & drop-downs
		if (o.showOverflowOnHover)
			$P.hover( allowOverflow, resetOverflow );

		// if adding a pane AFTER initialization, then...
		if (state.initialized) {
			initHandles( pane );
			initHotkeys( pane );
			resizeAll(); // will sizeContent if pane is visible
			if (s.isVisible) { // pane is OPEN
				if (o.triggerEventsOnLoad)
					_execCallback(pane, o.onresize_end || o.onresize); 
				resizeNestedLayout(pane);
			}
		}
	}

	/**
	* Initialize module objects, styling, size and position for all resize bars and toggler buttons
	*
	* @see  _create()
	* @param {string=}	panes		The edge(s) to process, blank = all
	*/
,	initHandles = function (panes) {
		if (!panes || panes == "all") panes = _c.borderPanes;

		// create toggler DIVs for each pane, and set object pointers for them, eg: $R.north = north toggler DIV
		$.each(panes.split(","), function (i, pane) {
			var $P		= $Ps[pane];
			$Rs[pane]	= false; // INIT
			$Ts[pane]	= false;
			if (!$P) return; // pane does not exist - skip

			var 
				o		= options[pane]
			,	s		= state[pane]
			,	c		= _c[pane]
			,	rClass	= o.resizerClass
			,	tClass	= o.togglerClass
			,	side	= c.side.toLowerCase()
			,	spacing	= (s.isVisible ? o.spacing_open : o.spacing_closed)
			,	_pane	= "-"+ pane // used for classNames
			,	_state	= (s.isVisible ? "-open" : "-closed") // used for classNames
				// INIT RESIZER BAR
			,	$R		= $Rs[pane] = $("<div></div>")
				// INIT TOGGLER BUTTON
			,	$T		= (o.closable ? $Ts[pane] = $("<div></div>") : false)
			;

			//if (s.isVisible && o.resizable) ... handled by initResizable
			if (!s.isVisible && o.slidable)
				$R.attr("title", o.sliderTip).css("cursor", o.sliderCursor);

			$R
				// if paneSelector is an ID, then create a matching ID for the resizer, eg: "#paneLeft" => "paneLeft-resizer"
				.attr("id", (o.paneSelector.substr(0,1)=="#" ? o.paneSelector.substr(1) + "-resizer" : ""))
				.data("parentLayout", Instance)
				.data("layoutRole", "resizer")
				.data("layoutEdge", pane)
				.css(_c.resizers.cssReq).css("zIndex", _c.zIndex.resizer_normal)
				.css(o.applyDemoStyles ? _c.resizers.cssDemo : {}) // add demo styles
				.addClass(rClass +" "+ rClass+_pane)
				.appendTo($N) // append DIV to container
			;

			if ($T) {
				$T
					// if paneSelector is an ID, then create a matching ID for the resizer, eg: "#paneLeft" => "#paneLeft-toggler"
					.attr("id", (o.paneSelector.substr(0,1)=="#" ? o.paneSelector.substr(1) + "-toggler" : ""))
					.data("parentLayout", Instance)
					.data("layoutRole", "toggler")
					.data("layoutEdge", pane)
					.css(_c.togglers.cssReq) // add base/required styles
					.css(o.applyDemoStyles ? _c.togglers.cssDemo : {}) // add demo styles
					.addClass(tClass +" "+ tClass+_pane)
					.appendTo($R) // append SPAN to resizer DIV
				;
				// ADD INNER-SPANS TO TOGGLER
				if (o.togglerContent_open) // ui-layout-open
					$("<span>"+ o.togglerContent_open +"</span>")
						.data("layoutRole", "togglerContent")
						.data("layoutEdge", pane)
						.addClass("content content-open")
						.css("display","none")
						.appendTo( $T )
						//.hover( addHover, removeHover ) // use ui-layout-toggler-west-hover .content-open instead!
					;
				if (o.togglerContent_closed) // ui-layout-closed
					$("<span>"+ o.togglerContent_closed +"</span>")
						.data("layoutRole", "togglerContent")
						.data("layoutEdge", pane)
						.addClass("content content-closed")
						.css("display","none")
						.appendTo( $T )
						//.hover( addHover, removeHover ) // use ui-layout-toggler-west-hover .content-closed instead!
					;
				// ADD TOGGLER.click/.hover
				enableClosable(pane);
			}

			// add Draggable events
			initResizable(pane);

			// ADD CLASSNAMES & SLIDE-BINDINGS - eg: class="resizer resizer-west resizer-open"
			if (s.isVisible)
				setAsOpen(pane);	// onOpen will be called, but NOT onResize
			else {
				setAsClosed(pane);	// onClose will be called
				bindStartSlidingEvent(pane, true); // will enable events IF option is set
			}

		});

		// SET ALL HANDLE DIMENSIONS
		sizeHandles("all");
	}


	/**
	* Initialize scrolling ui-layout-content div - if exists
	*
	* @see  initPane() - or externally after an Ajax injection
	* @param {string}	pane		The pane to process
	* @param {boolean=}	resize		Size content after init, default = true
	*/
,	initContent = function (pane, resize) {
		if (!isInitialized()) return;
		var 
			o	= options[pane]
		,	sel	= o.contentSelector
		,	$P	= $Ps[pane]
		,	$C
		;
		if (sel) $C = $Cs[pane] = (o.findNestedContent)
			? $P.find(sel).eq(0) // match 1-element only
			: $P.children(sel).eq(0)
		;
		if ($C && $C.length) {
			// SAVE original Pane CSS
			if (!$C.data("layoutCSS"))
				$C.data("layoutCSS", elCSS($C, "height"));
			$C.css( _c.content.cssReq );
			if (o.applyDemoStyles) {
				$C.css( _c.content.cssDemo ); // add padding & overflow: auto to content-div
				$P.css( _c.content.cssDemoPane ); // REMOVE padding/scrolling from pane
			}
			state[pane].content = {}; // init content state
			if (resize !== false) sizeContent(pane);
			// sizeContent() is called AFTER init of all elements
		}
		else
			$Cs[pane] = false;
	}


	/**
	* Searches for .ui-layout-button-xxx elements and auto-binds them as layout-buttons
	*
	* @see  _create()
	*/
,	initButtons = function () {
		var pre	= "ui-layout-button-", name;
		$.each("toggle,open,close,pin,toggle-slide,open-slide".split(","), function (i, action) {
			$.each(_c.borderPanes.split(","), function (ii, pane) {
				$("."+pre+action+"-"+pane).each(function(){
					// if button was previously 'bound', data.layoutName was set, but is blank if layout has no 'name'
					name = $(this).data("layoutName") || $(this).attr("layoutName");
					if (name == undefined || name == options.name)
						bindButton(this, action, pane);
				});
			});
		});
	}

	/**
	* Add resize-bars to all panes that specify it in options
	* -dependancy: $.fn.resizable - will skip if not found
	*
	* @see			_create()
	* @param {string=}	panes		The edge(s) to process, blank = all
	*/
,	initResizable = function (panes) {
		var
			draggingAvailable = (typeof $.fn.draggable == "function")
		,	$Frames, side // set in start()
		;
		if (!panes || panes == "all") panes = _c.borderPanes;

		$.each(panes.split(","), function (idx, pane) {
			var 
				o	= options[pane]
			,	s	= state[pane]
			,	c	= _c[pane]
			,	side = (c.dir=="horz" ? "top" : "left")
			,	r, live // set in start because may change
			;
			if (!draggingAvailable || !$Ps[pane] || !o.resizable) {
				o.resizable = false;
				return true; // skip to next
			}

			var 
				$P 		= $Ps[pane]
			,	$R		= $Rs[pane]
			,	base	= o.resizerClass
			//	'drag' classes are applied to the ORIGINAL resizer-bar while dragging is in process
			,	resizerClass		= base+"-drag"				// resizer-drag
			,	resizerPaneClass	= base+"-"+pane+"-drag"		// resizer-north-drag
			//	'helper' class is applied to the CLONED resizer-bar while it is being dragged
			,	helperClass			= base+"-dragging"			// resizer-dragging
			,	helperPaneClass		= base+"-"+pane+"-dragging" // resizer-north-dragging
			,	helperLimitClass	= base+"-dragging-limit"	// resizer-drag
			,	helperPaneLimitClass = base+"-"+pane+"-dragging-limit"	// resizer-north-drag
			,	helperClassesSet	= false 					// logic var
			;

			if (!s.isClosed)
				$R
					.attr("title", o.resizerTip)
					.css("cursor", o.resizerCursor) // n-resize, s-resize, etc
				;

			$R.bind("mouseenter."+ sID, onResizerEnter)
			  .bind("mouseleave."+ sID, onResizerLeave);

			$R.draggable({
				containment:	$N[0] // limit resizing to layout container
			,	axis:			(c.dir=="horz" ? "y" : "x") // limit resizing to horz or vert axis
			,	delay:			0
			,	distance:		1
			//	basic format for helper - style it using class: .ui-draggable-dragging
			,	helper:			"clone"
			,	opacity:		o.resizerDragOpacity
			,	addClasses:		false // avoid ui-state-disabled class when disabled
			//,	iframeFix:		o.draggableIframeFix // TODO: consider using when bug is fixed
			,	zIndex:			_c.zIndex.resizer_drag

			,	start: function (e, ui) {
					// REFRESH options & state pointers in case we used swapPanes
					o = options[pane];
					s = state[pane];
					// re-read options
					live = o.resizeWhileDragging;

					// ondrag_start callback - will CANCEL hide if returns false
					// TODO: dragging CANNOT be cancelled like this, so see if there is a way?
					if (false === _execCallback(pane, o.ondrag_start)) return false;

					_c.isLayoutBusy	= true; // used by sizePane() logic during a liveResize
					s.isResizing	= true; // prevent pane from closing while resizing
					timer.clear(pane+"_closeSlider"); // just in case already triggered

					// SET RESIZER LIMITS - used in drag()
					setSizeLimits(pane); // update pane/resizer state
					r = s.resizerPosition;

					$R.addClass( resizerClass +" "+ resizerPaneClass ); // add drag classes
					helperClassesSet = false; // reset logic var - see drag()

					// MASK PANES WITH IFRAMES OR OTHER TROUBLESOME ELEMENTS
					$Frames = $(o.maskIframesOnResize === true ? "iframe" : o.maskIframesOnResize).filter(":visible");
					var id, i=0; // ID incrementer - used when 'resizing' masks during dynamic resizing
					$Frames.each(function() {					
						id = "ui-layout-mask-"+ (++i);
						$(this).data("layoutMaskID", id); // tag iframe with corresponding maskID
						$('<div id="'+ id +'" class="ui-layout-mask ui-layout-mask-'+ pane +'"/>')
							.css({
								background:	"#fff"
							,	opacity:	"0.001"
							,	zIndex:		_c.zIndex.iframe_mask
							,	position:	"absolute"
							,	width:		this.offsetWidth+"px"
							,	height:		this.offsetHeight+"px"
							})
							.css($(this).position()) // top & left -- changed from offset()
							.appendTo(this.parentNode) // put mask-div INSIDE pane to avoid zIndex issues
						;
					});

					// DISABLE TEXT SELECTION (probably already done by resizer.mouseOver)
					$('body').disableSelection(); 
				}

			,	drag: function (e, ui) {
					if (!helperClassesSet) { // can only add classes after clone has been added to the DOM
						//$(".ui-draggable-dragging")
						ui.helper
							.addClass( helperClass +" "+ helperPaneClass ) // add helper classes
							.css({ right: "auto", bottom: "auto" })	// fix dir="rtl" issue
							.children().css("visibility","hidden")	// hide toggler inside dragged resizer-bar
						;
						helperClassesSet = true;
						// draggable bug!? RE-SET zIndex to prevent E/W resize-bar showing through N/S pane!
						if (s.isSliding) $Ps[pane].css("zIndex", _c.zIndex.pane_sliding);
					}
					// CONTAIN RESIZER-BAR TO RESIZING LIMITS
					var limit = 0;
					if (ui.position[side] < r.min) {
						ui.position[side] = r.min;
						limit = -1;
					}
					else if (ui.position[side] > r.max) {
						ui.position[side] = r.max;
						limit = 1;
					}
					// ADD/REMOVE dragging-limit CLASS
					if (limit) {
						ui.helper.addClass( helperLimitClass +" "+ helperPaneLimitClass ); // at dragging-limit
						window.defaultStatus = "Panel has reached its " +
							((limit>0 && pane.match(/north|west/)) || (limit<0 && pane.match(/south|east/)) ? "maximum" : "minimum") +" size";
					}
					else {
						ui.helper.removeClass( helperLimitClass +" "+ helperPaneLimitClass ); // not at dragging-limit
						window.defaultStatus = "";
					}
					// DYNAMICALLY RESIZE PANES IF OPTION ENABLED
					if (live) resizePanes(e, ui, pane);
				}

			,	stop: function (e, ui) {
					$('body').enableSelection(); // RE-ENABLE TEXT SELECTION
					window.defaultStatus = ""; // clear 'resizing limit' message from statusbar
					$R.removeClass( resizerClass +" "+ resizerPaneClass ); // remove drag classes from Resizer
					s.isResizing = false;
					_c.isLayoutBusy	= false; // set BEFORE resizePanes so other logic can pick it up
					resizePanes(e, ui, pane, true); // true = resizingDone
				}

			});

			/**
			* resizePanes
			*
			* Sub-routine called from stop() and optionally drag()
			*
			* @param {!Object}		evt
			* @param {!Object}		ui
			* @param {string}		pane
			* @param {boolean=}		resizingDone
			*/
			var resizePanes = function (evt, ui, pane, resizingDone) {
				var 
					dragPos	= ui.position
				,	c		= _c[pane]
				,	resizerPos, newSize
				,	i = 0 // ID incrementer
				;
				switch (pane) {
					case "north":	resizerPos = dragPos.top; break;
					case "west":	resizerPos = dragPos.left; break;
					case "south":	resizerPos = sC.offsetHeight - dragPos.top  - o.spacing_open; break;
					case "east":	resizerPos = sC.offsetWidth  - dragPos.left - o.spacing_open; break;
				};

				if (resizingDone) {
					// Remove OR Resize MASK(S) created in drag.start
					$("div.ui-layout-mask").each(function() { this.parentNode.removeChild(this); });
					//$("div.ui-layout-mask").remove(); // TODO: Is this less efficient?

					// ondrag_start callback - will CANCEL hide if returns false
					if (false === _execCallback(pane, o.ondrag_end || o.ondrag)) return false;
				}
				else
					$Frames.each(function() {
						$("#"+ $(this).data("layoutMaskID")) // get corresponding mask by ID
							.css($(this).position()) // update top & left
							.css({ // update width & height
								width:	this.offsetWidth +"px"
							,	height:	this.offsetHeight+"px"
							})
						;
					});

				// remove container margin from resizer position to get the pane size
				newSize = resizerPos - sC["inset"+ c.side];
				manualSizePane(pane, newSize);
			}
		});
	}


	/**
	*	Destroy this layout and reset all elements
	*/
,	destroy = function () {
		// UNBIND layout events and remove global object
		$(window).unbind("."+ sID);
		$(document).unbind("."+ sID);

		// loop all panes to remove layout classes, attributes and bindings
		$.each(_c.allPanes.split(","), function (i, pane) {
			removePane( pane, false, true ); // true = skipResize
		});

		// reset layout-container
		$N	.removeData("layout")
			.removeData("layoutContainer")
			.removeClass(options.containerClass)
		;

		// do NOT reset container CSS if is a 'pane' in an outer-layout - ie, THIS layout is 'nested'
		if (!$N.data("layoutEdge") && $N.data("layoutCSS")) // RESET CSS
			$N.css( $N.data("layoutCSS") ).removeData("layoutCSS");

		// for full-page layouts, also reset the <HTML> CSS
		if (sC.tagName == "BODY" && ($N = $("html")).data("layoutCSS")) // RESET <HTML> CSS
			$N.css( $N.data("layoutCSS") ).removeData("layoutCSS");

		// trigger state-management and onunload callback
		unload();
	}

	/**
	* Remove a pane from the layout - subroutine of destroy()
	*
	* @see  destroy()
	* @param {string}	pane		The pane to process
	* @param {boolean=}	remove		Remove the DOM element?		default = false
	* @param {boolean=}	skipResize	Skip calling resizeAll()?	default = false
	*/
,	removePane = function (pane, remove, skipResize) {
		if (!isInitialized()) return;
		if (!$Ps[pane]) return; // NO SUCH PANE
		var
			$P		= $Ps[pane]
		,	$C		= $Cs[pane]
		,	$R		= $Rs[pane]
		,	$T		= $Ts[pane]
		//	create list of ALL pane-classes that need to be removed
		,	_open	= "-open"
		,	_sliding= "-sliding"
		,	_closed	= "-closed"
		,	root	= options[pane].paneClass // default="ui-layout-pane"
		,	pRoot	= root +"-"+ pane // eg: "ui-layout-pane-west"
		,	classes	= [	root, root+_open, root+_closed, root+_sliding,		// generic classes
						pRoot, pRoot+_open, pRoot+_closed, pRoot+_sliding ]	// pane-specific classes
		;
		$.merge(classes, getHoverClasses($P, true)); // ADD hover-classes

		if (!$P || !$P.length) {
			} // pane has already been deleted!
		else if (remove && !$P.data("layoutContainer") && (!$C || !$C.length || !$C.data("layoutContainer")))
			$P.remove();
		else {
			$P	.removeClass( classes.join(" ") ) // remove ALL pane-classes
				.removeData("layoutParent")
				.removeData("layoutRole")
				.removeData("layoutEdge")
				.removeData("autoHidden")	// in case set
				.unbind("."+ sID) // remove ALL Layout events
				// TODO: remove these extra unbind commands when jQuery is fixed
				//.unbind("mouseenter"+ sID)
				//.unbind("mouseleave"+ sID)
			;
			// do NOT reset CSS if this pane is STILL the container of a nested layout!
			// the nested layout will reset its 'container' when/if it is destroyed
			if (!$P.data("layoutContainer"))
				$P.css( $P.data("layoutCSS") ).removeData("layoutCSS");
			// DITTO for the Content elem
			if ($C && $C.length && !$C.data("layoutContainer"))
				$C.css( $C.data("layoutCSS") ).removeData("layoutCSS");
		}

		// REMOVE pane resizer and toggler elements
		if ($T && $T.length) $T.remove();
		if ($R && $R.length) $R.remove();

		// CLEAR all pointers and data
		$Ps[pane] = $Cs[pane] = $Rs[pane] = $Ts[pane] = false;

		// skip resize & state-clear when called from destroy()
		if (!skipResize) {
			resizeAll();
			state[pane] = {};
		}
	}


/*
 * ###########################
 *	   ACTION METHODS
 * ###########################
 */

	/**
	* Completely 'hides' a pane, including its spacing - as if it does not exist
	* The pane is not actually 'removed' from the source, so can use 'show' to un-hide it
	*
	* @param {string}	pane		The pane being hidden, ie: north, south, east, or west
	* @param {boolean=}	noAnimation	
	*/
,	hide = function (pane, noAnimation) {
		if (!isInitialized()) return;
		var
			o	= options[pane]
		,	s	= state[pane]
		,	$P	= $Ps[pane]
		,	$R	= $Rs[pane]
		;
		if (!$P || s.isHidden) return; // pane does not exist OR is already hidden

		// onhide_start callback - will CANCEL hide if returns false
		if (state.initialized && false === _execCallback(pane, o.onhide_start)) return;

		s.isSliding = false; // just in case

		// now hide the elements
		if ($R) $R.hide(); // hide resizer-bar
		if (!state.initialized || s.isClosed) {
			s.isClosed = true; // to trigger open-animation on show()
			s.isHidden  = true;
			s.isVisible = false;
			$P.hide(); // no animation when loading page
			sizeMidPanes(_c[pane].dir == "horz" ? "all" : "center");
			if (state.initialized || o.triggerEventsOnLoad)
				_execCallback(pane, o.onhide_end || o.onhide);
		}
		else {
			s.isHiding = true; // used by onclose
			close(pane, false, noAnimation); // adjust all panes to fit
		}
	}

	/**
	* Show a hidden pane - show as 'closed' by default unless openPane = true
	*
	* @param {string}	pane		The pane being opened, ie: north, south, east, or west
	* @param {boolean=}	openPane
	* @param {boolean=}	noAnimation
	* @param {boolean=}	noAlert
	*/
,	show = function (pane, openPane, noAnimation, noAlert) {
		if (!isInitialized()) return;
		var
			o	= options[pane]
		,	s	= state[pane]
		,	$P	= $Ps[pane]
		,	$R	= $Rs[pane]
		;
		if (!$P || !s.isHidden) return; // pane does not exist OR is not hidden

		// onshow_start callback - will CANCEL show if returns false
		if (false === _execCallback(pane, o.onshow_start)) return;

		s.isSliding = false; // just in case
		s.isShowing = true; // used by onopen/onclose
		//s.isHidden  = false; - will be set by open/close - if not cancelled

		// now show the elements
		//if ($R) $R.show(); - will be shown by open/close
		if (openPane === false)
			close(pane, true); // true = force
		else
			open(pane, false, noAnimation, noAlert); // adjust all panes to fit
	}


	/**
	* Toggles a pane open/closed by calling either open or close
	*
	* @param {string}	pane   The pane being toggled, ie: north, south, east, or west
	* @param {boolean=}	slide
	*/
,	toggle = function (pane, slide) {
		if (!isInitialized()) return;
		if (!isStr(pane)) {
			pane.stopImmediatePropagation(); // pane = event
			pane = $(this).data("layoutEdge"); // bound to $R.dblclick
		}
		var s = state[str(pane)];
		if (s.isHidden)
			show(pane); // will call 'open' after unhiding it
		else if (s.isClosed)
			open(pane, !!slide);
		else
			close(pane);
	}


	/**
	* Utility method used during init or other auto-processes
	*
	* @param {string}	pane   The pane being closed
	* @param {boolean=}	setHandles
	*/
,	_closePane = function (pane, setHandles) {
		var
			$P	= $Ps[pane]
		,	s	= state[pane]
		;
		$P.hide();
		s.isClosed = true;
		s.isVisible = false;
		// UNUSED: if (setHandles) setAsClosed(pane, true); // true = force
	}

	/**
	* Close the specified pane (animation optional), and resize all other panes as needed
	*
	* @param {string}	pane		The pane being closed, ie: north, south, east, or west
	* @param {boolean=}	force	
	* @param {boolean=}	noAnimation	
	* @param {boolean=}	skipCallback	
	*/
,	close = function (pane, force, noAnimation, skipCallback) {
		if (!state.initialized && $Ps[pane]) {
			_closePane(pane); // INIT pane as closed
			return;
		}
		if (!isInitialized()) return;
		var
			$P		= $Ps[pane]
		,	$R		= $Rs[pane]
		,	$T		= $Ts[pane]
		,	o		= options[pane]
		,	s		= state[pane]
		,	doFX	= !noAnimation && !s.isClosed && (o.fxName_close != "none")
		// 	transfer logic vars to temp vars
		,	isShowing	= s.isShowing
		,	isHiding	= s.isHiding
		,	wasSliding	= s.isSliding
		;
		// now clear the logic vars
		delete s.isShowing;
		delete s.isHiding;

		if (!$P || (!o.closable && !isShowing && !isHiding)) return; // invalid request // (!o.resizable && !o.closable) ???
		else if (!force && s.isClosed && !isShowing) return; // already closed

		if (_c.isLayoutBusy) { // layout is 'busy' - probably with an animation
			_queue("close", pane, force); // set a callback for this action, if possible
			return; // ABORT 
		}

		// onclose_start callback - will CANCEL hide if returns false
		// SKIP if just 'showing' a hidden pane as 'closed'
		if (!isShowing && false === _execCallback(pane, o.onclose_start)) return;

		// SET flow-control flags
		_c[pane].isMoving = true;
		_c.isLayoutBusy = true;

		s.isClosed = true;
		s.isVisible = false;
		// update isHidden BEFORE sizing panes
		if (isHiding) s.isHidden = true;
		else if (isShowing) s.isHidden = false;

		if (s.isSliding) // pane is being closed, so UNBIND trigger events
			bindStopSlidingEvents(pane, false); // will set isSliding=false
		else // resize panes adjacent to this one
			sizeMidPanes(_c[pane].dir == "horz" ? "all" : "center", false); // false = NOT skipCallback

		// if this pane has a resizer bar, move it NOW - before animation
		setAsClosed(pane);

		// CLOSE THE PANE
		if (doFX) { // animate the close
			lockPaneForFX(pane, true); // need to set left/top so animation will work
			$P.hide( o.fxName_close, o.fxSettings_close, o.fxSpeed_close, function () {
				lockPaneForFX(pane, false); // undo
				close_2();
			});
		}
		else { // hide the pane without animation
			$P.hide();
			close_2();
		};

		// SUBROUTINE
		function close_2 () {
			if (s.isClosed) { // make sure pane was not 'reopened' before animation finished!

				bindStartSlidingEvent(pane, true); // will enable if o.slidable = true

				// if opposite-pane was autoClosed, see if it can be autoOpened now
				var altPane = _c.altSide[pane];
				if (state[ altPane ].noRoom) {
					setSizeLimits( altPane );
					makePaneFit( altPane );
				}

				if (!skipCallback && (state.initialized || o.triggerEventsOnLoad)) {
					// onclose callback - UNLESS just 'showing' a hidden pane as 'closed'
					if (!isShowing) _execCallback(pane, o.onclose_end || o.onclose);
					// onhide OR onshow callback
					if (isShowing)	_execCallback(pane, o.onshow_end || o.onshow);
					if (isHiding)	_execCallback(pane, o.onhide_end || o.onhide);
				}
			}
			// execute internal flow-control callback
			_dequeue(pane);
		}
	}

	/**
	* @param {string}	pane	The pane just closed, ie: north, south, east, or west
	*/
,	setAsClosed = function (pane) {
		var
			$P		= $Ps[pane]
		,	$R		= $Rs[pane]
		,	$T		= $Ts[pane]
		,	o		= options[pane]
		,	s		= state[pane]
		,	side	= _c[pane].side.toLowerCase()
		,	inset	= "inset"+ _c[pane].side
		,	rClass	= o.resizerClass
		,	tClass	= o.togglerClass
		,	_pane	= "-"+ pane // used for classNames
		,	_open	= "-open"
		,	_sliding= "-sliding"
		,	_closed	= "-closed"
		;
		$R
			.css(side, sC[inset]) // move the resizer
			.removeClass( rClass+_open +" "+ rClass+_pane+_open )
			.removeClass( rClass+_sliding +" "+ rClass+_pane+_sliding )
			.addClass( rClass+_closed +" "+ rClass+_pane+_closed )
			.unbind("dblclick."+ sID)
		;
		// DISABLE 'resizing' when closed - do this BEFORE bindStartSlidingEvent?
		if (o.resizable && typeof $.fn.draggable == "function")
			$R
				.draggable("disable")
				.removeClass("ui-state-disabled") // do NOT apply disabled styling - not suitable here
				.css("cursor", "default")
				.attr("title","")
			;

		// if pane has a toggler button, adjust that too
		if ($T) {
			$T
				.removeClass( tClass+_open +" "+ tClass+_pane+_open )
				.addClass( tClass+_closed +" "+ tClass+_pane+_closed )
				.attr("title", o.togglerTip_closed) // may be blank
			;
			// toggler-content - if exists
			$T.children(".content-open").hide();
			$T.children(".content-closed").css("display","block");
		}

		// sync any 'pin buttons'
		syncPinBtns(pane, false);

		if (state.initialized) {
			// resize 'length' and position togglers for adjacent panes
			sizeHandles("all");
		}
	}

	/**
	* Open the specified pane (animation optional), and resize all other panes as needed
	*
	* @param {string}	pane		The pane being opened, ie: north, south, east, or west
	* @param {boolean=}	slide	
	* @param {boolean=}	noAnimation	
	* @param {boolean=}	noAlert	
	*/
,	open = function (pane, slide, noAnimation, noAlert) {
		if (!isInitialized()) return;
		var 
			$P		= $Ps[pane]
		,	$R		= $Rs[pane]
		,	$T		= $Ts[pane]
		,	o		= options[pane]
		,	s		= state[pane]
		,	doFX	= !noAnimation && s.isClosed && (o.fxName_open != "none")
		// 	transfer logic var to temp var
		,	isShowing = s.isShowing
		;
		// now clear the logic var
		delete s.isShowing;

		if (!$P || (!o.resizable && !o.closable && !isShowing)) return; // invalid request
		else if (s.isVisible && !s.isSliding) return; // already open

		// pane can ALSO be unhidden by just calling show(), so handle this scenario
		if (s.isHidden && !isShowing) {
			show(pane, true);
			return;
		}

		if (_c.isLayoutBusy) { // layout is 'busy' - probably with an animation
			_queue("open", pane, slide); // set a callback for this action, if possible
			return; // ABORT
		}

		setSizeLimits(pane, slide); // update pane-state

		// onopen_start callback - will CANCEL hide if returns false
		if (false === _execCallback(pane, o.onopen_start)) return;

		// make sure there is enough space available to open the pane
		setSizeLimits(pane, slide); // update pane-state
		if (s.minSize > s.maxSize) { // INSUFFICIENT ROOM FOR PANE TO OPEN!
			syncPinBtns(pane, false); // make sure pin-buttons are reset
			if (!noAlert && o.noRoomToOpenTip)
				alert(o.noRoomToOpenTip);
			return; // ABORT
		}

		// SET flow-control flags
		_c[pane].isMoving = true;
		_c.isLayoutBusy = true;

		if (slide) // START Sliding - will set isSliding=true
			bindStopSlidingEvents(pane, true); // BIND trigger events to close sliding-pane
		else if (s.isSliding) // PIN PANE (stop sliding) - open pane 'normally' instead
			bindStopSlidingEvents(pane, false); // UNBIND trigger events - will set isSliding=false
		else if (o.slidable)
			bindStartSlidingEvent(pane, false); // UNBIND trigger events

		s.noRoom = false; // will be reset by makePaneFit if 'noRoom'
		makePaneFit(pane);

		s.isVisible = true;
		s.isClosed	= false;
		// update isHidden BEFORE sizing panes - WHY??? Old?
		if (isShowing) s.isHidden = false;

		if (doFX) { // ANIMATE
			lockPaneForFX(pane, true); // need to set left/top so animation will work
			$P.show( o.fxName_open, o.fxSettings_open, o.fxSpeed_open, function() {
				lockPaneForFX(pane, false); // undo
				open_2(); // continue
			});
		}
		else {// no animation
			$P.show();	// just show pane and...
			open_2();	// continue
		};

		// SUBROUTINE
		function open_2 () {
			if (s.isVisible) { // make sure pane was not closed or hidden before animation finished!

				// cure iframe display issues
				_fixIframe(pane);

				// NOTE: if isSliding, then other panes are NOT 'resized'
				if (!s.isSliding) // resize all panes adjacent to this one
					sizeMidPanes(_c[pane].dir=="vert" ? "center" : "all", false); // false = NOT skipCallback

				// set classes, position handles and execute callbacks...
				setAsOpen(pane);
			}

			// internal flow-control callback
			_dequeue(pane);
		};
	
	}

	/**
	* @param {string}	pane		The pane just opened, ie: north, south, east, or west
	* @param {boolean=}	skipCallback	
	*/
,	setAsOpen = function (pane, skipCallback) {
		var 
			$P		= $Ps[pane]
		,	$R		= $Rs[pane]
		,	$T		= $Ts[pane]
		,	o		= options[pane]
		,	s		= state[pane]
		,	side	= _c[pane].side.toLowerCase()
		,	inset	= "inset"+ _c[pane].side
		,	rClass	= o.resizerClass
		,	tClass	= o.togglerClass
		,	_pane	= "-"+ pane // used for classNames
		,	_open	= "-open"
		,	_closed	= "-closed"
		,	_sliding= "-sliding"
		;
		$R
			.css(side, sC[inset] + getPaneSize(pane)) // move the resizer
			.removeClass( rClass+_closed +" "+ rClass+_pane+_closed )
			.addClass( rClass+_open +" "+ rClass+_pane+_open )
		;
		if (s.isSliding)
			$R.addClass( rClass+_sliding +" "+ rClass+_pane+_sliding )
		else // in case 'was sliding'
			$R.removeClass( rClass+_sliding +" "+ rClass+_pane+_sliding )

		if (o.resizerDblClickToggle)
			$R.bind("dblclick", toggle );
		removeHover( 0, $R ); // remove hover classes
		if (o.resizable && typeof $.fn.draggable == "function")
			$R
				.draggable("enable")
				.css("cursor", o.resizerCursor)
				.attr("title", o.resizerTip)
			;
		else if (!s.isSliding)
			$R.css("cursor", "default"); // n-resize, s-resize, etc

		// if pane also has a toggler button, adjust that too
		if ($T) {
			$T
				.removeClass( tClass+_closed +" "+ tClass+_pane+_closed )
				.addClass( tClass+_open +" "+ tClass+_pane+_open )
				.attr("title", o.togglerTip_open) // may be blank
			;
			removeHover( 0, $T ); // remove hover classes
			// toggler-content - if exists
			$T.children(".content-closed").hide();
			$T.children(".content-open").css("display","block");
		}

		// sync any 'pin buttons'
		syncPinBtns(pane, !s.isSliding);

		// update pane-state dimensions - BEFORE resizing content
		$.extend(s, elDims($P));

		if (state.initialized) {
			// resize resizer & toggler sizes for all panes
			sizeHandles("all");
			// resize content every time pane opens - to be sure
			sizeContent(pane, true); // true = remeasure headers/footers, even if 'isLayoutBusy'
		}

		if (!skipCallback && (state.initialized || o.triggerEventsOnLoad) && $P.is(":visible")) {
			// onopen callback
			_execCallback(pane, o.onopen_end || o.onopen);
			// onshow callback - TODO: should this be here?
			if (s.isShowing) _execCallback(pane, o.onshow_end || o.onshow);
			// ALSO call onresize because layout-size *may* have changed while pane was closed
			if (state.initialized) {
				_execCallback(pane, o.onresize_end || o.onresize);
				resizeNestedLayout(pane);
			}
		}
	}


	/**
	* slideOpen / slideClose / slideToggle
	*
	* Pass-though methods for sliding
	*/
,	slideOpen = function (evt_or_pane) {
		if (!isInitialized()) return;
		var
			evt		= isStr(evt_or_pane) ? null : evt_or_pane
		,	pane	= evt ? $(this).data("layoutEdge") : evt_or_pane
		,	s		= state[pane]
		,	delay	= options[pane].slideDelay_open
		;
		// prevent event from triggering on NEW resizer binding created below
		if (evt) evt.stopImmediatePropagation();

		if (s.isClosed && evt && evt.type == "mouseenter" && delay > 0)
			// trigger = mouseenter - use a delay
			timer.set(pane+"_openSlider", open_NOW, delay);
		else
			open_NOW(); // will unbind events if is already open

		/**
		* SUBROUTINE for timed open
		*/
		function open_NOW (evt) {
			if (!s.isClosed) // skip if no longer closed!
				bindStopSlidingEvents(pane, true); // BIND trigger events to close sliding-pane
			else if (!_c[pane].isMoving)
				open(pane, true); // true = slide - open() will handle binding
		};
	}

,	slideClose = function (evt_or_pane) {
		if (!isInitialized()) return;
		var
			evt		= isStr(evt_or_pane) ? null : evt_or_pane
		,	pane	= evt ? $(this).data("layoutEdge") : evt_or_pane
		,	o		= options[pane]
		,	s		= state[pane]
		,	delay	= _c[pane].isMoving ? 1000 : 300 // MINIMUM delay - option may override
		;

		if (s.isClosed || s.isResizing)
			return; // skip if already closed OR in process of resizing
		else if (o.slideTrigger_close == "click")
			close_NOW(); // close immediately onClick
		else if (o.preventQuickSlideClose && _c.isLayoutBusy)
			return; // handle Chrome quick-close on slide-open
		else if (o.preventPrematureSlideClose && evt && $.layout.isMouseOverElem(evt, $Ps[pane]))
			return; // handle incorrect mouseleave trigger, like when over a SELECT-list in IE
		else if (evt) // trigger = mouseleave - use a delay
			// 1 sec delay if 'opening', else .3 sec
			timer.set(pane+"_closeSlider", close_NOW, max(o.slideDelay_close, delay));
		else // called programically
			close_NOW();

		/**
		* SUBROUTINE for timed close
		*/
		function close_NOW () {
			if (s.isClosed) // skip 'close' if already closed!
				bindStopSlidingEvents(pane, false); // UNBIND trigger events - TODO: is this needed here?
			else if (!_c[pane].isMoving)
				close(pane); // close will handle unbinding
		};
	}

,	slideToggle = function (pane) { toggle(pane, true); }


	/**
	* Must set left/top on East/South panes so animation will work properly
	*
	* @param {string}  pane  The pane to lock, 'east' or 'south' - any other is ignored!
	* @param {boolean}  doLock  true = set left/top, false = remove
	*/
,	lockPaneForFX = function (pane, doLock) {
		var $P = $Ps[pane];
		if (doLock) {
			$P.css({ zIndex: _c.zIndex.pane_animate }); // overlay all elements during animation
			if (pane=="south")
				$P.css({ top: sC.insetTop + sC.innerHeight - $P.outerHeight() });
			else if (pane=="east")
				$P.css({ left: sC.insetLeft + sC.innerWidth - $P.outerWidth() });
		}
		else { // animation DONE - RESET CSS
			// TODO: see if this can be deleted. It causes a quick-close when sliding in Chrome
			$P.css({ zIndex: (state[pane].isSliding ? _c.zIndex.pane_sliding : _c.zIndex.pane_normal) });
			if (pane=="south")
				$P.css({ top: "auto" });
			else if (pane=="east")
				$P.css({ left: "auto" });
			// fix anti-aliasing in IE - only needed for animations that change opacity
			var o = options[pane];
			if ($.layout.browser.msie && o.fxOpacityFix && o.fxName_open != "slide" && $P.css("filter") && $P.css("opacity") == 1)
				$P[0].style.removeAttribute('filter');
		}
	}


	/**
	* Toggle sliding functionality of a specific pane on/off by adding removing 'slide open' trigger
	*
	* @see  open(), close()
	* @param {string}	pane	The pane to enable/disable, 'north', 'south', etc.
	* @param {boolean}	enable	Enable or Disable sliding?
	*/
,	bindStartSlidingEvent = function (pane, enable) {
		var 
			o		= options[pane]
		,	$P		= $Ps[pane]
		,	$R		= $Rs[pane]
		,	trigger	= o.slideTrigger_open.toLowerCase()
		;
		if (!$R || (enable && !o.slidable)) return;

		// make sure we have a valid event
		if (trigger.match(/mouseover/))
			trigger = o.slideTrigger_open = "mouseenter";
		else if (!trigger.match(/click|dblclick|mouseenter/)) 
			trigger = o.slideTrigger_open = "click";

		$R
			// add or remove trigger event
			[enable ? "bind" : "unbind"](trigger +'.'+ sID, slideOpen)
			// set the appropriate cursor & title/tip
			.css("cursor", enable ? o.sliderCursor : "default")
			.attr("title", enable ? o.sliderTip : "")
		;
	}

	/**
	* Add or remove 'mouseleave' events to 'slide close' when pane is 'sliding' open or closed
	* Also increases zIndex when pane is sliding open
	* See bindStartSlidingEvent for code to control 'slide open'
	*
	* @see  slideOpen(), slideClose()
	* @param {string}	pane	The pane to process, 'north', 'south', etc.
	* @param {boolean}	enable	Enable or Disable events?
	*/
,	bindStopSlidingEvents = function (pane, enable) {
		var 
			o		= options[pane]
		,	s		= state[pane]
		,	z		= _c.zIndex
		,	trigger	= o.slideTrigger_close.toLowerCase()
		,	action	= (enable ? "bind" : "unbind")
		,	$P		= $Ps[pane]
		,	$R		= $Rs[pane]
		;
		s.isSliding = enable; // logic
		timer.clear(pane+"_closeSlider"); // just in case

		// remove 'slideOpen' trigger event from resizer
		// ALSO will raise the zIndex of the pane & resizer
		if (enable) bindStartSlidingEvent(pane, false);

		// RE/SET zIndex - increases when pane is sliding-open, resets to normal when not
		$P.css("zIndex", enable ? z.pane_sliding : z.pane_normal);
		$R.css("zIndex", enable ? z.pane_sliding : z.resizer_normal);

		// make sure we have a valid event
		if (!trigger.match(/click|mouseleave/))
			trigger = o.slideTrigger_close = "mouseleave"; // also catches 'mouseout'

		// add/remove slide triggers
		$R[action](trigger, slideClose); // base event on resize
		// need extra events for mouseleave
		if (trigger == "mouseleave") {
			// also close on pane.mouseleave
			$P[action]("mouseleave."+ sID, slideClose);
			// cancel timer when mouse moves between 'pane' and 'resizer'
			$R[action]("mouseenter."+ sID, cancelMouseOut);
			$P[action]("mouseenter."+ sID, cancelMouseOut);
		}

		if (!enable)
			timer.clear(pane+"_closeSlider");
		else if (trigger == "click" && !o.resizable) {
			// IF pane is not resizable (which already has a cursor and tip) 
			// then set the a cursor & title/tip on resizer when sliding
			$R.css("cursor", enable ? o.sliderCursor : "default");
			$R.attr("title", enable ? o.togglerTip_open : ""); // use Toggler-tip, eg: "Close Pane"
		}

		// SUBROUTINE for mouseleave timer clearing
		function cancelMouseOut (evt) {
			timer.clear(pane+"_closeSlider");
			evt.stopPropagation();
		}
	}


	/**
	* Hides/closes a pane if there is insufficient room - reverses this when there is room again
	* MUST have already called setSizeLimits() before calling this method
	*
	* @param {string}		pane			The pane being resized
	* @param {boolean=}	isOpening		Called from onOpen?
	* @param {boolean=}	skipCallback	Should the onresize callback be run?
	* @param {boolean=}	force
	*/
,	makePaneFit = function (pane, isOpening, skipCallback, force) {
		var
			o	= options[pane]
		,	s	= state[pane]
		,	c	= _c[pane]
		,	$P	= $Ps[pane]
		,	$R	= $Rs[pane]
		,	isSidePane 	= c.dir=="vert"
		,	hasRoom		= false
		;

		// special handling for center & east/west panes
		if (pane == "center" || (isSidePane && s.noVerticalRoom)) {
			// see if there is enough room to display the pane
			// ERROR: hasRoom = s.minHeight <= s.maxHeight && (isSidePane || s.minWidth <= s.maxWidth);
			hasRoom = (s.maxHeight > 0);
			if (hasRoom && s.noRoom) { // previously hidden due to noRoom, so show now
				$P.show();
				if ($R) $R.show();
				s.isVisible = true;
				s.noRoom = false;
				if (isSidePane) s.noVerticalRoom = false;
				_fixIframe(pane);
			}
			else if (!hasRoom && !s.noRoom) { // not currently hidden, so hide now
				$P.hide();
				if ($R) $R.hide();
				s.isVisible = false;
				s.noRoom = true;
			}
		}

		// see if there is enough room to fit the border-pane
		if (pane == "center") {
			// ignore center in this block
		}
		else if (s.minSize <= s.maxSize) { // pane CAN fit
			hasRoom = true;
			if (s.size > s.maxSize) // pane is too big - shrink it
				sizePane(pane, s.maxSize, skipCallback, force);
			else if (s.size < s.minSize) // pane is too small - enlarge it
				sizePane(pane, s.minSize, skipCallback, force);
			else if ($R && $P.is(":visible")) {
				// make sure resizer-bar is positioned correctly
				// handles situation where nested layout was 'hidden' when initialized
				var
					side = c.side.toLowerCase()
				,	pos  = s.size + sC["inset"+ c.side]
				;
				if ($.layout.cssNum($R, side) != pos) $R.css( side, pos );
			}

			// if was previously hidden due to noRoom, then RESET because NOW there is room
			if (s.noRoom) {
				// s.noRoom state will be set by open or show
				if (s.wasOpen && o.closable) {
					if (o.autoReopen)
						open(pane, false, true, true); // true = noAnimation, true = noAlert
					else // leave the pane closed, so just update state
						s.noRoom = false;
				}
				else
					show(pane, s.wasOpen, true, true); // true = noAnimation, true = noAlert
			}
		}
		else { // !hasRoom - pane CANNOT fit
			if (!s.noRoom) { // pane not set as noRoom yet, so hide or close it now...
				s.noRoom = true; // update state
				s.wasOpen = !s.isClosed && !s.isSliding;
				if (s.isClosed){} // SKIP
				else if (o.closable) // 'close' if possible
					close(pane, true, true); // true = force, true = noAnimation
				else // 'hide' pane if cannot just be closed
					hide(pane, true); // true = noAnimation
			}
		}
	}


	/**
	* sizePane / manualSizePane
	* sizePane is called only by internal methods whenever a pane needs to be resized
	* manualSizePane is an exposed flow-through method allowing extra code when pane is 'manually resized'
	*
	* @param {string}		pane			The pane being resized
	* @param {number}		size			The *desired* new size for this pane - will be validated
	* @param {boolean=}		skipCallback	Should the onresize callback be run?
	*/
,	manualSizePane = function (pane, size, skipCallback) {
		if (!isInitialized()) return;
		// ANY call to sizePane will disabled autoResize
		var
			o = options[pane]
		//	if resizing callbacks have been delayed and resizing is now DONE, force resizing to complete...
		,	forceResize = o.resizeWhileDragging && !_c.isLayoutBusy //  && !o.triggerEventsWhileDragging
		;
		o.autoResize = false;
		// flow-through...
		sizePane(pane, size, skipCallback, forceResize);
	}

	/**
	* @param {string}		pane			The pane being resized
	* @param {number}		size			The *desired* new size for this pane - will be validated
	* @param {boolean=}		skipCallback	Should the onresize callback be run?
	* @param {boolean=}		force			Force resizing even if does not seem necessary
	*/
,	sizePane = function (pane, size, skipCallback, force) {
		if (!isInitialized()) return;
		var 
			o		= options[pane]
		,	s		= state[pane]
		,	$P		= $Ps[pane]
		,	$R		= $Rs[pane]
		,	side	= _c[pane].side.toLowerCase()
		,	dimName	= _c[pane].sizeType.toLowerCase()
		,	inset	= "inset"+ _c[pane].side
		,	skipResizeWhileDragging = _c.isLayoutBusy && !o.triggerEventsWhileDragging
		,	oldSize
		;
		// calculate 'current' min/max sizes
		setSizeLimits(pane); // update pane-state
		oldSize = s.size;

		size = _parseSize(pane, size); // handle percentages & auto
		size = max(size, _parseSize(pane, o.minSize));
		size = min(size, s.maxSize);
		if (size < s.minSize) { // not enough room for pane!
			makePaneFit(pane, false, skipCallback);	// will hide or close pane
			return;
		}

		// IF newSize is same as oldSize, then nothing to do - abort
		if (!force && size == oldSize) return;

		// onresize_start callback CANNOT cancel resizing because this would break the layout!
		if (!skipCallback && state.initialized && s.isVisible)
			_execCallback(pane, o.onresize_start);

		// resize the pane, and make sure its visible
		$P.css( dimName, max(1, cssSize(pane, size)) );

/*
var
	edge	= _c[pane].sizeType.toLowerCase()
,	test	= [{
			  	target:		size
			,	attempt:	size
			,	actual:		edge=='width' ? $P.outerWidth() : $P.outerHeight()
			}]
,	lastTest = test[0]
,	thisTest = {}
;
while (lastTest.actual != size) {
	test.push( {} );
	thisTest = test[ test.length - 1 ];

	if (lastTest.actual > size)
		thisTest.attempt = Math.max(1, lastTest.attempt - (lastTest.actual - size));
	else // lastTest.actual < size
		thisTest.attempt = Math.max(1, lastTest.attempt + (size - lastTest.actual));

	$P.css( edge, cssSize(pane, thisTest.attempt) );

	thisTest.actual = edge=='width' ? $P.outerWidth() : $P.outerHeight()

	// after 3 tries, is as close as its gonna get!
	if (test.length == 3) break;
	else lastTest = thisTest;
}
debugData( test, pane );
*/

		// update pane-state dimensions
		s.size = size;
		$.extend(s, elDims($P));

		// reposition the resizer-bar
		if ($R && $P.is(":visible")) $R.css( side, size + sC[inset] );

		sizeContent(pane);

		if (!skipCallback && !skipResizeWhileDragging && state.initialized && s.isVisible) {
			_execCallback(pane, o.onresize_end || o.onresize);
			resizeNestedLayout(pane);
		}

		// resize all the adjacent panes, and adjust their toggler buttons
		// when skipCallback passed, it means the controlling method will handle 'other panes'
		if (!skipCallback) {
			// also no callback if live-resize is in progress and NOT triggerEventsWhileDragging
			if (!s.isSliding) sizeMidPanes(_c[pane].dir=="horz" ? "all" : "center", skipResizeWhileDragging, force);
			sizeHandles("all");
		}

		// if opposite-pane was autoClosed, see if it can be autoOpened now
		var altPane = _c.altSide[pane];
		if (size < oldSize && state[ altPane ].noRoom) {
			setSizeLimits( altPane );
			makePaneFit( altPane, false, skipCallback );
		}
	}

	/**
	* @see  initPanes(), sizePane(), resizeAll(), open(), close(), hide()
	* @param {string}	panes			The pane(s) being resized, comma-delmited string
	* @param {boolean=}	skipCallback	Should the onresize callback be run?
	* @param {boolean=}	force
	*/
,	sizeMidPanes = function (panes, skipCallback, force) {
		if (!panes || panes == "all") panes = "east,west,center";

		$.each(panes.split(","), function (i, pane) {
			if (!$Ps[pane]) return; // NO PANE - skip
			var 
				o		= options[pane]
			,	s		= state[pane]
			,	$P		= $Ps[pane]
			,	$R		= $Rs[pane]
			,	isCenter= (pane=="center")
			,	hasRoom	= true
			,	CSS		= {}
			,	newCenter	= calcNewCenterPaneDims()
			;
			// update pane-state dimensions
			$.extend(s, elDims($P));

			if (pane == "center") {
				if (!force && s.isVisible && newCenter.width == s.outerWidth && newCenter.height == s.outerHeight)
					return true; // SKIP - pane already the correct size
				// set state for makePaneFit() logic
				$.extend(s, cssMinDims(pane), {
					maxWidth:	newCenter.width
				,	maxHeight:	newCenter.height
				});
				CSS = newCenter;
				// convert OUTER width/height to CSS width/height 
				CSS.width	= cssW(pane, CSS.width);
				CSS.height	= cssH(pane, CSS.height);
				hasRoom		= CSS.width > 0 && CSS.height > 0;
				// during layout init, try to shrink east/west panes to make room for center
				if (!hasRoom && !state.initialized && o.minWidth > 0) {
					var
						reqPx	= o.minWidth - s.outerWidth
					,	minE	= options.east.minSize || 0
					,	minW	= options.west.minSize || 0
					,	sizeE	= state.east.size
					,	sizeW	= state.west.size
					,	newE	= sizeE
					,	newW	= sizeW
					;
					if (reqPx > 0 && state.east.isVisible && sizeE > minE) {
						newE = max( sizeE-minE, sizeE-reqPx );
						reqPx -= sizeE-newE;
					}
					if (reqPx > 0 && state.west.isVisible && sizeW > minW) {
						newW = max( sizeW-minW, sizeW-reqPx );
						reqPx -= sizeW-newW;
					}
					// IF we found enough extra space, then resize the border panes as calculated
					if (reqPx == 0) {
						if (sizeE != minE)
							sizePane('east', newE, true); // true = skipCallback - initPanes will handle when done
						if (sizeW != minW)
							sizePane('west', newW, true);
						// now start over!
						sizeMidPanes('center', skipCallback, force);
						return; // abort this loop
					}
				}
			}
			else { // for east and west, set only the height, which is same as center height
				// set state.min/maxWidth/Height for makePaneFit() logic
				if (s.isVisible && !s.noVerticalRoom)
					$.extend(s, elDims($P), cssMinDims(pane))
				if (!force && !s.noVerticalRoom && newCenter.height == s.outerHeight)
					return true; // SKIP - pane already the correct size
				// east/west have same top, bottom & height as center
				CSS.top		= newCenter.top;
				CSS.bottom	= newCenter.bottom;
				CSS.height	= cssH(pane, newCenter.height);
				s.maxHeight	= max(0, CSS.height);
				hasRoom		= (s.maxHeight > 0);
				if (!hasRoom) s.noVerticalRoom = true; // makePaneFit() logic
			}

			if (hasRoom) {
				// resizeAll passes skipCallback because it triggers callbacks after ALL panes are resized
				if (!skipCallback && state.initialized)
					_execCallback(pane, o.onresize_start);

				$P.css(CSS); // apply the CSS to pane
				if (s.noRoom && !s.isClosed && !s.isHidden)
					makePaneFit(pane); // will re-open/show auto-closed/hidden pane
				if (s.isVisible) {
					$.extend(s, elDims($P)); // update pane dimensions
					if (state.initialized) sizeContent(pane); // also resize the contents, if exists
				}
			}
			else if (!s.noRoom && s.isVisible) // no room for pane
				makePaneFit(pane); // will hide or close pane

			if (!s.isVisible)
				return true; // DONE - next pane

			/*
			* Extra CSS for IE6 or IE7 in Quirks-mode - add 'width' to NORTH/SOUTH panes
			* Normally these panes have only 'left' & 'right' positions so pane auto-sizes
			* ALSO required when pane is an IFRAME because will NOT default to 'full width'
			*/
			if (pane == "center") { // finished processing midPanes
				var b = $.layout.browser;
				var fix = b.isIE6 || (b.msie && !b.boxModel);
				if ($Ps.north && (fix || state.north.tagName=="IFRAME")) 
					$Ps.north.css("width", cssW($Ps.north, sC.innerWidth));
				if ($Ps.south && (fix || state.south.tagName=="IFRAME"))
					$Ps.south.css("width", cssW($Ps.south, sC.innerWidth));
			}

			// resizeAll passes skipCallback because it triggers callbacks after ALL panes are resized
			if (!skipCallback && state.initialized) {
				_execCallback(pane, o.onresize_end || o.onresize);
				resizeNestedLayout(pane);
			}
		});
	}


	/**
	* @see  window.onresize(), callbacks or custom code
	*/
,	resizeAll = function () {
		if (!state.initialized) {
			_initLayoutElements();
			return; // no need to resize since we just initialized!
		}
		var	oldW	= sC.innerWidth
		,	oldH	= sC.innerHeight
		;
		// cannot size layout when 'container' is hidden or collapsed
		if (!$N.is(":visible:") ) return;
		$.extend( state.container, elDims( $N ) ); // UPDATE container dimensions
		if (!sC.outerHeight) return;

		// onresizeall_start will CANCEL resizing if returns false
		// state.container has already been set, so user can access this info for calcuations
		if (false === _execCallback(null, options.onresizeall_start)) return false;

		var	// see if container is now 'smaller' than before
			shrunkH	= (sC.innerHeight < oldH)
		,	shrunkW	= (sC.innerWidth < oldW)
		,	$P, o, s, dir
		;
		// NOTE special order for sizing: S-N-E-W
		$.each(["south","north","east","west"], function (i, pane) {
			if (!$Ps[pane]) return; // no pane - SKIP
			s	= state[pane];
			o	= options[pane];
			dir	= _c[pane].dir;

			if (o.autoResize && s.size != o.size) // resize pane to original size set in options
				sizePane(pane, o.size, true, true); // true=skipCallback, true=forceResize
			else {
				setSizeLimits(pane);
				makePaneFit(pane, false, true, true); // true=skipCallback, true=forceResize
			}
		});

		sizeMidPanes("all", true, true); // true=skipCallback, true=forceResize
		sizeHandles("all"); // reposition the toggler elements

		// trigger all individual pane callbacks AFTER layout has finished resizing
		o = options; // reuse alias
		$.each(_c.allPanes.split(","), function (i, pane) {
			$P = $Ps[pane];
			if (!$P) return; // SKIP
			if (state[pane].isVisible) { // undefined for non-existent panes
				_execCallback(pane, o[pane].onresize_end || o[pane].onresize); // callback - if exists
				resizeNestedLayout(pane);
			}
		});

		_execCallback(null, o.onresizeall_end || o.onresizeall); // onresizeall callback, if exists
	}


	/**
	* Whenever a pane resizes or opens that has a nested layout, trigger resizeAll
	*
	* @param {string}		pane		The pane just resized or opened
	*/
,	resizeNestedLayout = function (pane) {
		var
			$P	= $Ps[pane]
		,	$C	= $Cs[pane]
		,	d	= "layoutContainer"
		;
		if (options[pane].resizeNestedLayout) {
			if ($P.data( d ))
				$P.layout().resizeAll();
			else if ($C && $C.data( d ))
				$C.layout().resizeAll();
		}
	}


	/**
	* IF pane has a content-div, then resize all elements inside pane to fit pane-height
	*
	* @param {string=}		panes		The pane(s) being resized
	* @param {boolean=}	remeasure	Should the content (header/footer) be remeasured?
	*/
,	sizeContent = function (panes, remeasure) {
		if (!isInitialized()) return;
		if (!panes || panes == "all") panes = _c.allPanes;
		$.each(panes.split(","), function (idx, pane) {
			var
				$P	= $Ps[pane]
			,	$C	= $Cs[pane]
			,	o	= options[pane]
			,	s	= state[pane]
			,	m	= s.content // m = measurements
			;
			if (!$P || !$C || !$P.is(":visible")) return true; // NOT VISIBLE - skip

			// onsizecontent_start will CANCEL resizing if returns false
			if (false === _execCallback(null, o.onsizecontent_start)) return;

			// skip re-measuring offsets if live-resizing
			if (!_c.isLayoutBusy || m.top == undefined || remeasure || o.resizeContentWhileDragging) {
				_measure();
				// if any footers are below pane-bottom, they may not measure correctly,
				// so allow pane overflow and re-measure
				if (m.hiddenFooters > 0 && $P.css("overflow") == "hidden") {
					$P.css("overflow", "visible");
					_measure(); // remeasure while overflowing
					$P.css("overflow", "hidden");
				}
			}
			// NOTE: spaceAbove/Below *includes* the pane paddingTop/Bottom, but not pane.borders
			var newH = s.innerHeight - (m.spaceAbove - s.css.paddingTop) - (m.spaceBelow - s.css.paddingBottom);
			if (!$C.is(":visible") || m.height != newH) {
				// size the Content element to fit new pane-size - will autoHide if not enough room
				setOuterHeight($C, newH, true); // true=autoHide
				m.height = newH; // save new height
			};

			if (state.initialized) {
				_execCallback(pane, o.onsizecontent_end || o.onsizecontent);
				resizeNestedLayout(pane);
			}


			function _below ($E) {
				return max(s.css.paddingBottom, (parseInt($E.css("marginBottom"), 10) || 0));
			};

			function _measure () {
				var
					ignore	= options[pane].contentIgnoreSelector
				,	$Fs		= $C.nextAll().not(ignore || ':lt(0)') // not :lt(0) = ALL
				,	$Fs_vis	= $Fs.filter(':visible')
				,	$F		= $Fs_vis.filter(':last')
				;
				m = {
					top:			$C[0].offsetTop
				,	height:			$C.outerHeight()
				,	numFooters:		$Fs.length
				,	hiddenFooters:	$Fs.length - $Fs_vis.length
				,	spaceBelow:		0 // correct if no content footer ($E)
				}
					m.spaceAbove	= m.top; // just for state - not used in calc
					m.bottom		= m.top + m.height;
				if ($F.length)
					//spaceBelow = (LastFooter.top + LastFooter.height) [footerBottom] - Content.bottom + max(LastFooter.marginBottom, pane.paddingBotom)
					m.spaceBelow = ($F[0].offsetTop + $F.outerHeight()) - m.bottom + _below($F);
				else // no footer - check marginBottom on Content element itself
					m.spaceBelow = _below($C);
			};
		});
	}


	/**
	* Called every time a pane is opened, closed, or resized to slide the togglers to 'center' and adjust their length if necessary
	*
	* @see  initHandles(), open(), close(), resizeAll()
	* @param {string=}		panes		The pane(s) being resized
	*/
,	sizeHandles = function (panes) {
		if (!panes || panes == "all") panes = _c.borderPanes;

		$.each(panes.split(","), function (i, pane) {
			var 
				o	= options[pane]
			,	s	= state[pane]
			,	$P	= $Ps[pane]
			,	$R	= $Rs[pane]
			,	$T	= $Ts[pane]
			,	$TC
			;
			if (!$P || !$R) return;

			var
				dir			= _c[pane].dir
			,	_state		= (s.isClosed ? "_closed" : "_open")
			,	spacing		= o["spacing"+ _state]
			,	togAlign	= o["togglerAlign"+ _state]
			,	togLen		= o["togglerLength"+ _state]
			,	paneLen
			,	offset
			,	CSS = {}
			;

			if (spacing == 0) {
				$R.hide();
				return;
			}
			else if (!s.noRoom && !s.isHidden) // skip if resizer was hidden for any reason
				$R.show(); // in case was previously hidden

			// Resizer Bar is ALWAYS same width/height of pane it is attached to
			if (dir == "horz") { // north/south
				paneLen = $P.outerWidth(); // s.outerWidth || 
				s.resizerLength = paneLen;
				$R.css({
					width:	max(1, cssW($R, paneLen)) // account for borders & padding
				,	height:	max(0, cssH($R, spacing)) // ditto
				,	left:	$.layout.cssNum($P, "left")
				});
			}
			else { // east/west
				paneLen = $P.outerHeight(); // s.outerHeight || 
				s.resizerLength = paneLen;
				$R.css({
					height:	max(1, cssH($R, paneLen)) // account for borders & padding
				,	width:	max(0, cssW($R, spacing)) // ditto
				,	top:	sC.insetTop + getPaneSize("north", true) // TODO: what if no North pane?
				//,	top:	$.layout.cssNum($Ps["center"], "top")
				});
			}

			// remove hover classes
			removeHover( o, $R );

			if ($T) {
				if (togLen == 0 || (s.isSliding && o.hideTogglerOnSlide)) {
					$T.hide(); // always HIDE the toggler when 'sliding'
					return;
				}
				else
					$T.show(); // in case was previously hidden

				if (!(togLen > 0) || togLen == "100%" || togLen > paneLen) {
					togLen = paneLen;
					offset = 0;
				}
				else { // calculate 'offset' based on options.PANE.togglerAlign_open/closed
					if (isStr(togAlign)) {
						switch (togAlign) {
							case "top":
							case "left":	offset = 0;
											break;
							case "bottom":
							case "right":	offset = paneLen - togLen;
											break;
							case "middle":
							case "center":
							default:		offset = Math.floor((paneLen - togLen) / 2); // 'default' catches typos
						}
					}
					else { // togAlign = number
						var x = parseInt(togAlign, 10); //
						if (togAlign >= 0) offset = x;
						else offset = paneLen - togLen + x; // NOTE: x is negative!
					}
				}

				if (dir == "horz") { // north/south
					var width = cssW($T, togLen);
					$T.css({
						width:	max(0, width)  // account for borders & padding
					,	height:	max(1, cssH($T, spacing)) // ditto
					,	left:	offset // TODO: VERIFY that toggler  positions correctly for ALL values
					,	top:	0
					});
					// CENTER the toggler content SPAN
					$T.children(".content").each(function(){
						$TC = $(this);
						$TC.css("marginLeft", Math.floor((width-$TC.outerWidth())/2)); // could be negative
					});
				}
				else { // east/west
					var height = cssH($T, togLen);
					$T.css({
						height:	max(0, height)  // account for borders & padding
					,	width:	max(1, cssW($T, spacing)) // ditto
					,	top:	offset // POSITION the toggler
					,	left:	0
					});
					// CENTER the toggler content SPAN
					$T.children(".content").each(function(){
						$TC = $(this);
						$TC.css("marginTop", Math.floor((height-$TC.outerHeight())/2)); // could be negative
					});
				}

				// remove ALL hover classes
				removeHover( 0, $T );
			}

			// DONE measuring and sizing this resizer/toggler, so can be 'hidden' now
			if (!state.initialized && (o.initHidden || s.noRoom)) {
				$R.hide();
				if ($T) $T.hide();
			}
		});
	}


,	enableClosable = function (pane) {
		if (!isInitialized()) return;
		var $T = $Ts[pane], o = options[pane];
		if (!$T) return;
		o.closable = true;
		$T	.bind("click."+ sID, function(evt){ evt.stopPropagation(); toggle(pane); })
			.bind("mouseenter."+ sID, addHover)
			.bind("mouseleave."+ sID, removeHover)
			.css("visibility", "visible")
			.css("cursor", "pointer")
			.attr("title", state[pane].isClosed ? o.togglerTip_closed : o.togglerTip_open) // may be blank
			.show()
		;
	}

,	disableClosable = function (pane, hide) {
		if (!isInitialized()) return;
		var $T = $Ts[pane];
		if (!$T) return;
		options[pane].closable = false;
		// is closable is disable, then pane MUST be open!
		if (state[pane].isClosed) open(pane, false, true);
		$T	.unbind("."+ sID)
			.css("visibility", hide ? "hidden" : "visible") // instead of hide(), which creates logic issues
			.css("cursor", "default")
			.attr("title", "")
		;
	}


,	enableSlidable = function (pane) {
		if (!isInitialized()) return;
		var $R = $Rs[pane], o = options[pane];
		if (!$R || !$R.data('draggable')) return;
		options[pane].slidable = true; 
		if (s.isClosed)
			bindStartSlidingEvent(pane, true);
	}

,	disableSlidable = function (pane) {
		if (!isInitialized()) return;
		var $R = $Rs[pane];
		if (!$R) return;
		options[pane].slidable = false; 
		if (state[pane].isSliding)
			close(pane, false, true);
		else {
			bindStartSlidingEvent(pane, false);
			$R	.css("cursor", "default")
				.attr("title", "")
			;
			removeHover(null, $R[0]); // in case currently hovered
		}
	}


,	enableResizable = function (pane) {
		if (!isInitialized()) return;
		var $R = $Rs[pane], o = options[pane];
		if (!$R || !$R.data('draggable')) return;
		o.resizable = true; 
		$R	.draggable("enable")
			.bind("mouseenter."+ sID, onResizerEnter)
			.bind("mouseleave."+ sID, onResizerLeave)
		;
		if (!state[pane].isClosed)
			$R	.css("cursor", o.resizerCursor)
			 	.attr("title", o.resizerTip)
			;
	}

,	disableResizable = function (pane) {
		if (!isInitialized()) return;
		var $R = $Rs[pane];
		if (!$R || !$R.data('draggable')) return;
		options[pane].resizable = false; 
		$R	.draggable("disable")
			.unbind("."+ sID)
			.css("cursor", "default")
			.attr("title", "")
		;
		removeHover(null, $R[0]); // in case currently hovered
	}


	/**
	* Move a pane from source-side (eg, west) to target-side (eg, east)
	* If pane exists on target-side, move that to source-side, ie, 'swap' the panes
	*
	* @param {string}	pane1		The pane/edge being swapped
	* @param {string}	pane2		ditto
	*/
,	swapPanes = function (pane1, pane2) {
		if (!isInitialized()) return;
		// change state.edge NOW so callbacks can know where pane is headed...
		state[pane1].edge = pane2;
		state[pane2].edge = pane1;
		// run these even if NOT state.initialized
		var cancelled = false;
		if (false === _execCallback(pane1, options[pane1].onswap_start)) cancelled = true;
		if (!cancelled && false === _execCallback(pane2, options[pane2].onswap_start)) cancelled = true;
		if (cancelled) {
			state[pane1].edge = pane1; // reset
			state[pane2].edge = pane2;
			return;
		}

		var
			oPane1	= copy( pane1 )
		,	oPane2	= copy( pane2 )
		,	sizes	= {}
		;
		sizes[pane1] = oPane1 ? oPane1.state.size : 0;
		sizes[pane2] = oPane2 ? oPane2.state.size : 0;

		// clear pointers & state
		$Ps[pane1] = false; 
		$Ps[pane2] = false;
		state[pane1] = {};
		state[pane2] = {};
		
		// ALWAYS remove the resizer & toggler elements
		if ($Ts[pane1]) $Ts[pane1].remove();
		if ($Ts[pane2]) $Ts[pane2].remove();
		if ($Rs[pane1]) $Rs[pane1].remove();
		if ($Rs[pane2]) $Rs[pane2].remove();
		$Rs[pane1] = $Rs[pane2] = $Ts[pane1] = $Ts[pane2] = false;

		// transfer element pointers and data to NEW Layout keys
		move( oPane1, pane2 );
		move( oPane2, pane1 );

		// cleanup objects
		oPane1 = oPane2 = sizes = null;

		// make panes 'visible' again
		if ($Ps[pane1]) $Ps[pane1].css(_c.visible);
		if ($Ps[pane2]) $Ps[pane2].css(_c.visible);

		// fix any size discrepancies caused by swap
		resizeAll();

		// run these even if NOT state.initialized
		_execCallback(pane1, options[pane1].onswap_end || options[pane1].onswap);
		_execCallback(pane2, options[pane2].onswap_end || options[pane2].onswap);

		return;

		function copy (n) { // n = pane
			var
				$P	= $Ps[n]
			,	$C	= $Cs[n]
			;
			return !$P ? false : {
				pane:		n
			,	P:			$P ? $P[0] : false
			,	C:			$C ? $C[0] : false
			,	state:		$.extend({}, state[n])
			,	options:	$.extend({}, options[n])
			}
		};

		function move (oPane, pane) {
			if (!oPane) return;
			var
				P		= oPane.P
			,	C		= oPane.C
			,	oldPane = oPane.pane
			,	c		= _c[pane]
			,	side	= c.side.toLowerCase()
			,	inset	= "inset"+ c.side
			//	save pane-options that should be retained
			,	s		= $.extend({}, state[pane])
			,	o		= options[pane]
			//	RETAIN side-specific FX Settings - more below
			,	fx		= { resizerCursor: o.resizerCursor }
			,	re, size, pos
			;
			$.each("fxName,fxSpeed,fxSettings".split(","), function (i, k) {
				fx[k] = o[k];
				fx[k +"_open"]  = o[k +"_open"];
				fx[k +"_close"] = o[k +"_close"];
			});

			// update object pointers and attributes
			$Ps[pane] = $(P)
				.data("layoutEdge", pane)
				.css(_c.hidden)
				.css(c.cssReq)
			;
			$Cs[pane] = C ? $(C) : false;

			// set options and state
			options[pane]	= $.extend({}, oPane.options, fx);
			state[pane]		= $.extend({}, oPane.state);

			// change classNames on the pane, eg: ui-layout-pane-east ==> ui-layout-pane-west
			re = new RegExp(o.paneClass +"-"+ oldPane, "g");
			P.className = P.className.replace(re, o.paneClass +"-"+ pane);

			// ALWAYS regenerate the resizer & toggler elements
			initHandles(pane); // create the required resizer & toggler

			// if moving to different orientation, then keep 'target' pane size
			if (c.dir != _c[oldPane].dir) {
				size = sizes[pane] || 0;
				setSizeLimits(pane); // update pane-state
				size = max(size, state[pane].minSize);
				// use manualSizePane to disable autoResize - not useful after panes are swapped
				manualSizePane(pane, size, true); // true = skipCallback
			}
			else // move the resizer here
				$Rs[pane].css(side, sC[inset] + (state[pane].isVisible ? getPaneSize(pane) : 0));


			// ADD CLASSNAMES & SLIDE-BINDINGS
			if (oPane.state.isVisible && !s.isVisible)
				setAsOpen(pane, true); // true = skipCallback
			else {
				setAsClosed(pane);
				bindStartSlidingEvent(pane, true); // will enable events IF option is set
			}

			// DESTROY the object
			oPane = null;
		};
	}


;	// END var DECLARATIONS

	/**
	* Capture keys when enableCursorHotkey - toggle pane if hotkey pressed
	*
	* @see  document.keydown()
	*/
	function keyDown (evt) {
		if (!evt) return true;
		var code = evt.keyCode;
		if (code < 33) return true; // ignore special keys: ENTER, TAB, etc

		var
			PANE = {
				38: "north" // Up Cursor	- $.ui.keyCode.UP
			,	40: "south" // Down Cursor	- $.ui.keyCode.DOWN
			,	37: "west"  // Left Cursor	- $.ui.keyCode.LEFT
			,	39: "east"  // Right Cursor	- $.ui.keyCode.RIGHT
			}
		,	ALT		= evt.altKey // no worky!
		,	SHIFT	= evt.shiftKey
		,	CTRL	= evt.ctrlKey
		,	CURSOR	= (CTRL && code >= 37 && code <= 40)
		,	o, k, m, pane
		;

		if (CURSOR && options[PANE[code]].enableCursorHotkey) // valid cursor-hotkey
			pane = PANE[code];
		else if (CTRL || SHIFT) // check to see if this matches a custom-hotkey
			$.each(_c.borderPanes.split(","), function (i, p) { // loop each pane to check its hotkey
				o = options[p];
				k = o.customHotkey;
				m = o.customHotkeyModifier; // if missing or invalid, treated as "CTRL+SHIFT"
				if ((SHIFT && m=="SHIFT") || (CTRL && m=="CTRL") || (CTRL && SHIFT)) { // Modifier matches
					if (k && code == (isNaN(k) || k <= 9 ? k.toUpperCase().charCodeAt(0) : k)) { // Key matches
						pane = p;
						return false; // BREAK
					}
				}
			});

		// validate pane
		if (!pane || !$Ps[pane] || !options[pane].closable || state[pane].isHidden)
			return true;

		toggle(pane);

		evt.stopPropagation();
		evt.returnValue = false; // CANCEL key
		return false;
	};


/*
 * ######################################
 *	UTILITY METHODS
 *	called externally or by initButtons
 * ######################################
 */

	/**
	* Change/reset a pane overflow setting & zIndex to allow popups/drop-downs to work
	*
	* @param {Object=}   el		(optional) Can also be 'bound' to a click, mouseOver, or other event
	*/
	function allowOverflow (el) {
		if (!isInitialized()) return;
		if (this && this.tagName) el = this; // BOUND to element
		var $P;
		if (isStr(el))
			$P = $Ps[el];
		else if ($(el).data("layoutRole"))
			$P = $(el);
		else
			$(el).parents().each(function(){
				if ($(this).data("layoutRole")) {
					$P = $(this);
					return false; // BREAK
				}
			});
		if (!$P || !$P.length) return; // INVALID

		var
			pane	= $P.data("layoutEdge")
		,	s		= state[pane]
		;

		// if pane is already raised, then reset it before doing it again!
		// this would happen if allowOverflow is attached to BOTH the pane and an element 
		if (s.cssSaved)
			resetOverflow(pane); // reset previous CSS before continuing

		// if pane is raised by sliding or resizing, or its closed, then abort
		if (s.isSliding || s.isResizing || s.isClosed) {
			s.cssSaved = false;
			return;
		}

		var
			newCSS	= { zIndex: (_c.zIndex.pane_normal + 2) }
		,	curCSS	= {}
		,	of		= $P.css("overflow")
		,	ofX		= $P.css("overflowX")
		,	ofY		= $P.css("overflowY")
		;
		// determine which, if any, overflow settings need to be changed
		if (of != "visible") {
			curCSS.overflow = of;
			newCSS.overflow = "visible";
		}
		if (ofX && !ofX.match(/visible|auto/)) {
			curCSS.overflowX = ofX;
			newCSS.overflowX = "visible";
		}
		if (ofY && !ofY.match(/visible|auto/)) {
			curCSS.overflowY = ofX;
			newCSS.overflowY = "visible";
		}

		// save the current overflow settings - even if blank!
		s.cssSaved = curCSS;

		// apply new CSS to raise zIndex and, if necessary, make overflow 'visible'
		$P.css( newCSS );

		// make sure the zIndex of all other panes is normal
		$.each(_c.allPanes.split(","), function(i, p) {
			if (p != pane) resetOverflow(p);
		});

	};

	function resetOverflow (el) {
		if (!isInitialized()) return;
		if (this && this.tagName) el = this; // BOUND to element
		var $P;
		if (isStr(el))
			$P = $Ps[el];
		else if ($(el).data("layoutRole"))
			$P = $(el);
		else
			$(el).parents().each(function(){
				if ($(this).data("layoutRole")) {
					$P = $(this);
					return false; // BREAK
				}
			});
		if (!$P || !$P.length) return; // INVALID

		var
			pane	= $P.data("layoutEdge")
		,	s		= state[pane]
		,	CSS		= s.cssSaved || {}
		;
		// reset the zIndex
		if (!s.isSliding && !s.isResizing)
			$P.css("zIndex", _c.zIndex.pane_normal);

		// reset Overflow - if necessary
		$P.css( CSS );

		// clear var
		s.cssSaved = false;
	};


	/**
	* Helper function to validate params received by addButton utilities
	*
	* Two classes are added to the element, based on the buttonClass...
	* The type of button is appended to create the 2nd className:
	*  - ui-layout-button-pin
	*  - ui-layout-pane-button-toggle
	*  - ui-layout-pane-button-open
	*  - ui-layout-pane-button-close
	*
	* @param  {(string|!Object)}	selector	jQuery selector (or element) for button, eg: ".ui-layout-north .toggle-button"
	* @param  {string}   			pane 		Name of the pane the button is for: 'north', 'south', etc.
	* @return {Array.<Object>}		If both params valid, the element matching 'selector' in a jQuery wrapper - otherwise returns null
	*/
	function getBtn (selector, pane, action) {
		var $E	= $(selector)
		,	err	= options.showErrorMessages;
		if (!$E.length) { // element not found
			if (err) alert(lang.errButton + lang.selector +": "+ selector);
		}
		else if (_c.borderPanes.indexOf(pane) == -1) { // invalid 'pane' sepecified
			if (err) alert(lang.errButton + lang.pane +": "+ pane);
		}
		else { // VALID
			var btn = options[pane].buttonClass +"-"+ action;
			$E
				.addClass( btn +" "+ btn +"-"+ pane )
				.data("layoutName", options.name) // add layout identifier - even if blank!
			;
			return $E;
		}
		return null;  // INVALID
	};


	/**
	* NEW syntax for binding layout-buttons - will eventually replace addToggleBtn, addOpenBtn, etc.
	*
	* @param {(string|!Object)}	selector	jQuery selector (or element) for button, eg: ".ui-layout-north .toggle-button"
	* @param {string}			action
	* @param {string}			pane
	*/
	function bindButton (selector, action, pane) {
		switch (action.toLowerCase()) {
			case "toggle":			addToggleBtn(selector, pane);		break;	
			case "open":			addOpenBtn(selector, pane);			break;
			case "close":			addCloseBtn(selector, pane);		break;
			case "pin":				addPinBtn(selector, pane);			break;
			case "toggle-slide":	addToggleBtn(selector, pane, true);	break;	
			case "open-slide":		addOpenBtn(selector, pane, true);	break;
		}
	};

	/**
	* Add a custom Toggler button for a pane
	*
	* @param {(string|!Object)}	selector	jQuery selector (or element) for button, eg: ".ui-layout-north .toggle-button"
	* @param {string}  			pane 		Name of the pane the button is for: 'north', 'south', etc.
	* @param {boolean=}			slide 		true = slide-open, false = pin-open
	*/
	function addToggleBtn (selector, pane, slide) {
		var $E = getBtn(selector, pane, "toggle");
		if ($E)
			$E.click(function (evt) {
				toggle(pane, !!slide);
				evt.stopPropagation();
			});
	};

	/**
	* Add a custom Open button for a pane
	*
	* @param {(string|!Object)}	selector	jQuery selector (or element) for button, eg: ".ui-layout-north .toggle-button"
	* @param {string}			pane 		Name of the pane the button is for: 'north', 'south', etc.
	* @param {boolean=}			slide 		true = slide-open, false = pin-open
	*/
	function addOpenBtn (selector, pane, slide) {
		var $E = getBtn(selector, pane, "open");
		if ($E)
			$E
				.attr("title", lang.Open)
				.click(function (evt) {
					open(pane, !!slide);
					evt.stopPropagation();
				})
			;
	};

	/**
	* Add a custom Close button for a pane
	*
	* @param {(string|!Object)}	selector	jQuery selector (or element) for button, eg: ".ui-layout-north .toggle-button"
	* @param {string}   		pane 		Name of the pane the button is for: 'north', 'south', etc.
	*/
	function addCloseBtn (selector, pane) {
		var $E = getBtn(selector, pane, "close");
		if ($E)
			$E
				.attr("title", lang.Close)
				.click(function (evt) {
					close(pane);
					evt.stopPropagation();
				})
			;
	};

	/**
	* addPinBtn
	*
	* Add a custom Pin button for a pane
	*
	* Four classes are added to the element, based on the paneClass for the associated pane...
	* Assuming the default paneClass and the pin is 'up', these classes are added for a west-pane pin:
	*  - ui-layout-pane-pin
	*  - ui-layout-pane-west-pin
	*  - ui-layout-pane-pin-up
	*  - ui-layout-pane-west-pin-up
	*
	* @param {(string|!Object)}	selector	jQuery selector (or element) for button, eg: ".ui-layout-north .toggle-button"
	* @param {string}   		pane 		Name of the pane the pin is for: 'north', 'south', etc.
	*/
	function addPinBtn (selector, pane) {
		var $E = getBtn(selector, pane, "pin");
		if ($E) {
			var s = state[pane];
			$E.click(function (evt) {
				setPinState($(this), pane, (s.isSliding || s.isClosed));
				if (s.isSliding || s.isClosed) open( pane ); // change from sliding to open
				else close( pane ); // slide-closed
				evt.stopPropagation();
			});
			// add up/down pin attributes and classes
			setPinState($E, pane, (!s.isClosed && !s.isSliding));
			// add this pin to the pane data so we can 'sync it' automatically
			// PANE.pins key is an array so we can store multiple pins for each pane
			_c[pane].pins.push( selector ); // just save the selector string
		}
	};

	/**
	* INTERNAL function to sync 'pin buttons' when pane is opened or closed
	* Unpinned means the pane is 'sliding' - ie, over-top of the adjacent panes
	*
	* @see  open(), close()
	* @param {string}	pane   These are the params returned to callbacks by layout()
	* @param {boolean}	doPin  True means set the pin 'down', False means 'up'
	*/
	function syncPinBtns (pane, doPin) {
		$.each(_c[pane].pins, function (i, selector) {
			setPinState($(selector), pane, doPin);
		});
	};

	/**
	* Change the class of the pin button to make it look 'up' or 'down'
	*
	* @see  addPinBtn(), syncPinBtns()
	* @param {Array.<Object>}	$Pin	The pin-span element in a jQuery wrapper
	* @param {string}	pane	These are the params returned to callbacks by layout()
	* @param {boolean}	doPin	true = set the pin 'down', false = set it 'up'
	*/
	function setPinState ($Pin, pane, doPin) {
		var updown = $Pin.attr("pin");
		if (updown && doPin == (updown=="down")) return; // already in correct state
		var
			pin		= options[pane].buttonClass +"-pin"
		,	side	= pin +"-"+ pane
		,	UP		= pin +"-up "+	side +"-up"
		,	DN		= pin +"-down "+side +"-down"
		;
		$Pin
			.attr("pin", doPin ? "down" : "up") // logic
			.attr("title", doPin ? lang.Unpin : lang.Pin)
			.removeClass( doPin ? UP : DN ) 
			.addClass( doPin ? DN : UP ) 
		;
	};


	/*
	* LAYOUT STATE MANAGEMENT
	*
	* @example .layout({ cookie: { name: "myLayout", keys: "west.isClosed,east.isClosed" } })
	* @example .layout({ cookie__name: "myLayout", cookie__keys: "west.isClosed,east.isClosed" })
	* @example myLayout.getState( "west.isClosed,north.size,south.isHidden" );
	* @example myLayout.saveCookie( "west.isClosed,north.size,south.isHidden", {expires: 7} );
	* @example myLayout.deleteCookie();
	* @example myLayout.loadCookie();
	* @example var hSaved = myLayout.state.cookie;
	*/

	function isCookiesEnabled () {
		// TODO: is the cookieEnabled property common enough to be useful???
		return (navigator.cookieEnabled != 0);
	};
	
	/**
	* Read & return data from the cookie - as JSON
	*
	* @param {Object=}	opts
	*/
	function getCookie (opts) {
		var
			o		= $.extend( {}, options.cookie, opts || {} )
		,	name	= o.name || options.name || "Layout"
		,	c		= document.cookie
		,	cs		= c ? c.split(';') : []
		,	pair	// loop var
		;
		for (var i=0, n=cs.length; i < n; i++) {
			pair = $.trim(cs[i]).split('='); // name=value pair
			if (pair[0] == name) // found the layout cookie
				// convert cookie string back to a hash
				return decodeJSON( decodeURIComponent(pair[1]) );
		}
		return "";
	};

	/**
	* Get the current layout state and save it to a cookie
	*
	* @param {(string|Array)=}	keys
	* @param {Object=}	opts
	*/
	function saveCookie (keys, opts) {
		var
			o		= $.extend( {}, options.cookie, opts || {} )
		,	name	= o.name || options.name || "Layout"
		,	params	= ''
		,	date	= ''
		,	clear	= false
		;
		if (o.expires.toUTCString)
			date = o.expires;
		else if (typeof o.expires == 'number') {
			date = new Date();
			if (o.expires > 0)
				date.setDate(date.getDate() + o.expires);
			else {
				date.setYear(1970);
				clear = true;
			}
		}
		if (date)		params += ';expires='+ date.toUTCString();
		if (o.path)		params += ';path='+ o.path;
		if (o.domain)	params += ';domain='+ o.domain;
		if (o.secure)	params += ';secure';

		if (clear) {
			state.cookie = {}; // clear data
			document.cookie = name +'='+ params; // expire the cookie
		}
		else {
			state.cookie = getState(keys || o.keys); // read current panes-state
			document.cookie = name +'='+ encodeURIComponent( encodeJSON(state.cookie) ) + params; // write cookie
		}

		return $.extend({}, state.cookie); // return COPY of state.cookie
	};

	/**
	* Remove the state cookie
	*/
	function deleteCookie () {
		saveCookie('', { expires: -1 });
	};

	/**
	* Get data from the cookie and USE IT to loadState
	*
	* @param {Object=}	opts
	*/
	function loadCookie (opts) {
		var o = getCookie(opts); // READ the cookie
		if (o) {
			state.cookie = $.extend({}, o); // SET state.cookie
			loadState(o);	// LOAD the retrieved state
		}
		return o;
	};

	/**
	* Update layout options from the cookie, if one exists
	*
	* @param {Object=}	opts
	* @param {boolean=}	animate
	*/
	function loadState (opts, animate) {
		opts = _transformData(opts);
		$.extend( true, options, opts ); // update layout options
		// if layout has already been initialized, then UPDATE layout state
		if (state.initialized) {
			var pane, o, s, h, c, a = !animate;
			$.each(_c.allPanes.split(","), function (idx, pane) {
				o = opts[ pane ];
				if (typeof o != 'object') return; // no key, continue
				s = o.size;
				c = o.initClosed;
				h = o.initHidden;
				if (s > 0 || s=="auto") sizePane(pane, s);
				if (h === true)			hide(pane, a);
				else if (c === false)	open(pane, false, a );
				else if (c === true)	close(pane, false, a);
				else if (h === false)	show(pane, false, a);
			});
		}
	};

	/**
	* Get the *current layout state* and return it as a hash
	*
	* @param {(string|Array)=}	keys
	*/
	function getState (keys) {
		var
			data	= {}
		,	alt		= { isClosed: 'initClosed', isHidden: 'initHidden' }
		,	pair, pane, key, val
		;
		if (!keys) keys = options.cookie.keys; // if called by user
		if ($.isArray(keys)) keys = keys.join(",");
		// convert keys to an array and change delimiters from '__' to '.'
		keys = keys.replace(/__/g, ".").split(',');
		// loop keys and create a data hash
		for (var i=0,n=keys.length; i < n; i++) {
			pair = keys[i].split(".");
			pane = pair[0];
			key  = pair[1];
			if (_c.allPanes.indexOf(pane) < 0) continue; // bad pane!
			val = state[ pane ][ key ];
			if (val == undefined) continue;
			if (key=="isClosed" && state[pane]["isSliding"])
				val = true; // if sliding, then *really* isClosed
			( data[pane] || (data[pane]={}) )[ alt[key] ? alt[key] : key ] = val;
		}
		return data;
	};

	/**
	* Stringify a JSON hash so can save in a cookie or db-field
	*/
	function encodeJSON (JSON) {
		return parse( JSON );
		function parse (h) {
			var D=[], i=0, k, v, t; // k = key, v = value
			for (k in h) {
				v = h[k];
				t = typeof v;
				if (t == 'string')		// STRING - add quotes
					v = '"'+ v +'"';
				else if (t == 'object')	// SUB-KEY - recurse into it
					v = parse(v);
				D[i++] = '"'+ k +'":'+ v;
			}
			return "{"+ D.join(",") +"}";
		};
	};

	/**
	* Convert stringified JSON back to a hash object
	*/
	function decodeJSON (str) {
		try { return window["eval"]("("+ str +")") || {}; }
		catch (e) { return {}; }
	};


/*
 * #####################
 * CREATE/RETURN LAYOUT
 * #####################
 */

	// validate that container exists
	var $N = $(this).eq(0); // FIRST matching Container element
	if (!$N.length) {
		if (options.showErrorMessages)
			alert( lang.errContainerMissing );
		return null;
	};

	// Users retreive Instance of a layout with: $N.layout() OR $N.data("layout")
	// return the Instance-pointer if layout has already been initialized
	if ($N.data("layoutContainer") && $N.data("layout"))
		return $N.data("layout"); // cached pointer

	// init global vars
	var 
		$Ps	= {} // Panes x5	- set in initPanes()
	,	$Cs	= {} // Content x5	- set in initPanes()
	,	$Rs	= {} // Resizers x4	- set in initHandles()
	,	$Ts	= {} // Togglers x4	- set in initHandles()
	//	aliases for code brevity
	,	sC	= state.container // alias for easy access to 'container dimensions'
	,	sID	= state.id // alias for unique layout ID/namespace - eg: "layout435"
	;

	// create Instance object to expose data & option Properties, and primary action Methods
	var Instance = {
		options:		options			// property - options hash
	,	state:			state			// property - dimensions hash
	,	container:		$N				// property - object pointers for layout container
	,	panes:			$Ps				// property - object pointers for ALL Panes: panes.north, panes.center
	,	contents:		$Cs				// property - object pointers for ALL Content: content.north, content.center
	,	resizers:		$Rs				// property - object pointers for ALL Resizers, eg: resizers.north
	,	togglers:		$Ts				// property - object pointers for ALL Togglers, eg: togglers.north
	,	toggle:			toggle			// method - pass a 'pane' ("north", "west", etc)
	,	hide:			hide			// method - ditto
	,	show:			show			// method - ditto
	,	open:			open			// method - ditto
	,	close:			close			// method - ditto
	,	slideOpen:		slideOpen		// method - ditto
	,	slideClose:		slideClose		// method - ditto
	,	slideToggle:	slideToggle		// method - ditto
	,	initContent:	initContent		// method - ditto
	,	sizeContent:	sizeContent		// method - ditto
	,	sizePane:		manualSizePane	// method - pass a 'pane' AND an 'outer-size' in pixels or percent, or 'auto'
	,	swapPanes:		swapPanes		// method - pass TWO 'panes' - will swap them
	,	resizeAll:		resizeAll		// method - no parameters
	,	initPanes:		isInitialized	// method - no parameters
	,	destroy:		destroy			// method - no parameters
	,	addPane:		addPane			// method - pass a 'pane'
	,	removePane:		removePane		// method - pass a 'pane' to remove from layout, add 'true' to delete the pane-elem
	,	setSizeLimits:	setSizeLimits	// method - pass a 'pane' - update state min/max data
	,	bindButton:		bindButton		// utility - pass element selector, 'action' and 'pane' (E, "toggle", "west")
	,	addToggleBtn:	addToggleBtn	// utility - pass element selector and 'pane' (E, "west")
	,	addOpenBtn:		addOpenBtn		// utility - ditto
	,	addCloseBtn:	addCloseBtn		// utility - ditto
	,	addPinBtn:		addPinBtn		// utility - ditto
	,	allowOverflow:	allowOverflow	// utility - pass calling element (this)
	,	resetOverflow:	resetOverflow	// utility - ditto
	,	encodeJSON:		encodeJSON		// method - pass a JSON object
	,	decodeJSON:		decodeJSON		// method - pass a string of encoded JSON
	,	getState:		getState		// method - returns hash of current layout-state
	,	getCookie:		getCookie		// method - update options from cookie - returns hash of cookie data
	,	saveCookie:		saveCookie		// method - optionally pass keys-list and cookie-options (hash)
	,	deleteCookie:	deleteCookie	// method
	,	loadCookie:		loadCookie		// method - update options from cookie - returns hash of cookie data
	,	loadState:		loadState		// method - pass a hash of state to use to update options
	,	cssWidth:		cssW			// utility - pass element and target outerWidth
	,	cssHeight:		cssH			// utility - ditto
	,	enableClosable: enableClosable
	,	disableClosable: disableClosable
	,	enableSlidable: enableSlidable
	,	disableSlidable: disableSlidable
	,	enableResizable: enableResizable
	,	disableResizable: disableResizable
	};

	// create the border layout NOW
	if (_create() === 'cancel') // onload_start callback returned false to CANCEL layout creation
		return null;
	else // true OR false -- if layout-elements did NOT init (hidden or do not exist), can auto-init later
		return Instance; // return the Instance object

}
})( jQuery );