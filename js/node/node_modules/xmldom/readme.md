# XMLDOM

[![license](https://img.shields.io/npm/l/xmldom?color=blue&style=flat-square)](LICENSE)
[![npm](https://img.shields.io/npm/v/xmldom?style=flat-square)](https://www.npmjs.com/package/xmldom)
[![bug issues](https://img.shields.io/github/issues/xmldom/xmldom/bug?color=red&style=flat-square)](https://github.com/xmldom/xmldom/issues?q=is%3Aissue+is%3Aopen+label%3Abug)
[!["help wanted" issues](https://img.shields.io/github/issues/xmldom/xmldom/help%20wanted?color=darkgreen&style=flat-square)](https://github.com/xmldom/xmldom/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22)
[![Mutation report](https://camo.githubusercontent.com/ee312c4ebce7784ce9f785757eba5d6e33e6d950/68747470733a2f2f696d672e736869656c64732e696f2f656e64706f696e743f7374796c653d666c61742675726c3d687474707325334125324625324662616467652d6170692e737472796b65722d6d757461746f722e696f2532466769746875622e636f6d25324662726f647962697473253246786d6c646f6d2532466d6173746572)](https://dashboard.stryker-mutator.io/reports/github.com/brodybits/xmldom/master)

A JavaScript implementation of W3C DOM for Node.js, Rhino and the browser. Fully
compatible with `W3C DOM level2`; and some compatible with `level3`. Supports
`DOMParser` and `XMLSerializer` interface such as in browser.

**Original project location:** <https://github.com/jindw/xmldom>

Install:
-------
>npm install xmldom

Example:
====
```javascript
const { DOMParser } = require('xmldom')

const doc = new DOMParser().parseFromString(
    '<xml xmlns="a" xmlns:c="./lite">\n' +
        '\t<child>test</child>\n' +
        '\t<child></child>\n' +
        '\t<child/>\n' +
        '</xml>',
    'text/xml'
)
doc.documentElement.setAttribute('x', 'y')
doc.documentElement.setAttributeNS('./lite', 'c:x', 'y2')
console.info(doc)

const nsAttr = doc.documentElement.getAttributeNS('./lite', 'x')
console.info(nsAttr)
```

Note: in Typescript and ES6 you can use the import approach, as follows:

```javascript
import { DOMParser } from 'xmldom'
```

API Reference
=====

 * [DOMParser](https://developer.mozilla.org/en-US/docs/Web/API/DOMParser):

	```javascript
	parseFromString(xmlsource,mimeType)
	```
	* **options extension** _by xmldom_(not BOM standard!!)

	```javascript
	//added the options argument
	new DOMParser(options)
	
	//errorHandler is supported
	new DOMParser({
		/**
		 * locator is always need for error position info
		 */
		locator:{},
		/**
		 * you can override the errorHandler for xml parser
		 * @link http://www.saxproject.org/apidoc/org/xml/sax/ErrorHandler.html
		 */
		errorHandler:{warning:function(w){console.warn(w)},error:callback,fatalError:callback}
		//only callback model
		//errorHandler:function(level,msg){console.log(level,msg)}
	})
		
	```

 * [XMLSerializer](https://developer.mozilla.org/en-US/docs/Web/API/XMLSerializer)
 
	```javascript
	serializeToString(node)
	```
DOM level2 method and attribute:
------

 * [Node](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-1950641247)
	
		attribute:
			nodeValue|prefix
		readonly attribute:
			nodeName|nodeType|parentNode|childNodes|firstChild|lastChild|previousSibling|nextSibling|attributes|ownerDocument|namespaceURI|localName
		method:	
			insertBefore(newChild, refChild)
			replaceChild(newChild, oldChild)
			removeChild(oldChild)
			appendChild(newChild)
			hasChildNodes()
			cloneNode(deep)
			normalize()
			isSupported(feature, version)
			hasAttributes()
* [DOMException](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/ecma-script-binding.html)
   The DOMException class has the following constants (and `value` of type `Number`):
  
  1. `DOMException.INDEX_SIZE_ERR` (`1`)
  1. `DOMException.DOMSTRING_SIZE_ERR` (`2`)
  1. `DOMException.HIERARCHY_REQUEST_ERR` (`3`)
  1. `DOMException.WRONG_DOCUMENT_ERR` (`4`)
  1. `DOMException.INVALID_CHARACTER_ERR` (`5`)
  1. `DOMException.NO_DATA_ALLOWED_ERR` (`6`)
  1. `DOMException.NO_MODIFICATION_ALLOWED_ERR` (`7`)
  1. `DOMException.NOT_FOUND_ERR` (`8`)
  1. `DOMException.NOT_SUPPORTED_ERR` (`9`)
  1. `DOMException.INUSE_ATTRIBUTE_ERR` (`10`)
  1. `DOMException.INVALID_STATE_ERR` (`11`)
  1. `DOMException.SYNTAX_ERR` (`12`)
  1. `DOMException.INVALID_MODIFICATION_ERR` (`13`)
  1. `DOMException.NAMESPACE_ERR` (`14`)
  1. `DOMException.INVALID_ACCESS_ERR` (`15`)
   
   The DOMException object has the following properties:
   code
   This property is of type Number.

	 * extends the Error type thrown as part of DOM API:

 * [DOMImplementation](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-102161490)
		
		method:
			hasFeature(feature, version)
			createDocumentType(qualifiedName, publicId, systemId)
			createDocument(namespaceURI, qualifiedName, doctype)

 * [Document](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#i-Document) : Node
		
		readonly attribute:
			doctype|implementation|documentElement
		method:
			createElement(tagName)
			createDocumentFragment()
			createTextNode(data)
			createComment(data)
			createCDATASection(data)
			createProcessingInstruction(target, data)
			createAttribute(name)
			createEntityReference(name)
			getElementsByTagName(tagname)
			importNode(importedNode, deep)
			createElementNS(namespaceURI, qualifiedName)
			createAttributeNS(namespaceURI, qualifiedName)
			getElementsByTagNameNS(namespaceURI, localName)
			getElementById(elementId)

 * [DocumentFragment](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-B63ED1A3) : Node
 * [Element](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-745549614) : Node
		
		readonly attribute:
			tagName
		method:
			getAttribute(name)
			setAttribute(name, value)
			removeAttribute(name)
			getAttributeNode(name)
			setAttributeNode(newAttr)
			removeAttributeNode(oldAttr)
			getElementsByTagName(name)
			getAttributeNS(namespaceURI, localName)
			setAttributeNS(namespaceURI, qualifiedName, value)
			removeAttributeNS(namespaceURI, localName)
			getAttributeNodeNS(namespaceURI, localName)
			setAttributeNodeNS(newAttr)
			getElementsByTagNameNS(namespaceURI, localName)
			hasAttribute(name)
			hasAttributeNS(namespaceURI, localName)

 * [Attr](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-637646024) : Node
	
		attribute:
			value
		readonly attribute:
			name|specified|ownerElement

 * [NodeList](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-536297177)
		
		readonly attribute:
			length
		method:
			item(index)
	
 * [NamedNodeMap](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-1780488922)

		readonly attribute:
			length
		method:
			getNamedItem(name)
			setNamedItem(arg)
			removeNamedItem(name)
			item(index)
			getNamedItemNS(namespaceURI, localName)
			setNamedItemNS(arg)
			removeNamedItemNS(namespaceURI, localName)
		
 * [CharacterData](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-FF21A306) : Node
	
		method:
			substringData(offset, count)
			appendData(arg)
			insertData(offset, arg)
			deleteData(offset, count)
			replaceData(offset, count, arg)
		
 * [Text](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-1312295772) : CharacterData
	
		method:
			splitText(offset)
			
 * [CDATASection](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-667469212)
 * [Comment](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-1728279322) : CharacterData
	
 * [DocumentType](http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-412266927)
	
		readonly attribute:
			name|entities|notations|publicId|systemId|internalSubset
			
 * Notation : Node
	
		readonly attribute:
			publicId|systemId
			
 * Entity : Node
	
		readonly attribute:
			publicId|systemId|notationName
			
 * EntityReference : Node 
 * ProcessingInstruction : Node 
	
		attribute:
			data
		readonly attribute:
			target
		
DOM level 3 support:
-----

 * [Node](http://www.w3.org/TR/DOM-Level-3-Core/core.html#Node3-textContent)
		
		attribute:
			textContent
		method:
			isDefaultNamespace(namespaceURI){
			lookupNamespaceURI(prefix)

DOM extension by xmldom
---

* [Node] Source position extension; 
		
		attribute:
			//Numbered starting from '1'
			lineNumber
			//Numbered starting from '1'
			columnNumber
