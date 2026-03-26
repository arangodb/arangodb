"use strict";

module.exports = class Colorizer {
    constructor(supportsColor = require("supports-color")) {
        this.supportsColor = supportsColor;
    }

    /**
     * Should be renamed to true #privateField
     * when we can ensure ES2022 support
     *
     * @private
     */
    colorize(str, color) {
        if (this.supportsColor.stdout === false) {
            return str;
        }

        return `\x1b[${color}m${str}\x1b[0m`;
    }

    red(str) {
        return this.colorize(str, 31);
    }

    green(str) {
        return this.colorize(str, 32);
    }

    cyan(str) {
        return this.colorize(str, 96);
    }

    white(str) {
        return this.colorize(str, 39);
    }

    bold(str) {
        return this.colorize(str, 1);
    }
};
