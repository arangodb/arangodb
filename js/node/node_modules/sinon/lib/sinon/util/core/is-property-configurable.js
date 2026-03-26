"use strict";

const getPropertyDescriptor = require("./get-property-descriptor");

function isPropertyConfigurable(obj, propName) {
    const propertyDescriptor = getPropertyDescriptor(obj, propName);

    return propertyDescriptor ? propertyDescriptor.configurable : true;
}

module.exports = isPropertyConfigurable;
