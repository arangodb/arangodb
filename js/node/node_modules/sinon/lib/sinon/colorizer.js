'use strict';

function getSupportsColor() {
    if (
        typeof process === "undefined" ||
        typeof process.stdout === "undefined"
    ) {
        return { stdout: false };
    }

    return { stdout: Boolean(process.stdout.isTTY) };
}

/**
 * Utility for colorizing console output.
 */
class Colorizer {
    /**
     * @param {object} [supportsColorModule] The supports-color module
     */
    constructor(supportsColorModule = getSupportsColor()) {
        this.supportsColor = supportsColorModule;
    }

    /**
     * Colorizes a string with the given color code.
     *
     * @param {string} str The string to colorize
     * @param {number} color The color code
     * @returns {string} The colorized string
     * @private
     */
    colorize(str, color) {
        if (this.supportsColor.stdout === false) {
            return str;
        }

        return `\x1b[${color}m${str}\x1b[0m`;
    }

    /**
     * Colorizes a string red.
     *
     * @param {string} str The string to colorize
     * @returns {string} The colorized string
     */
    red(str) {
        return this.colorize(str, 31);
    }

    /**
     * Colorizes a string green.
     *
     * @param {string} str The string to colorize
     * @returns {string} The colorized string
     */
    green(str) {
        return this.colorize(str, 32);
    }

    /**
     * Colorizes a string cyan.
     *
     * @param {string} str The string to colorize
     * @returns {string} The colorized string
     */
    cyan(str) {
        return this.colorize(str, 96);
    }

    /**
     * Colorizes a string white.
     *
     * @param {string} str The string to colorize
     * @returns {string} The colorized string
     */
    white(str) {
        return this.colorize(str, 39);
    }

    /**
     * Colorizes a string bold.
     *
     * @param {string} str The string to colorize
     * @returns {string} The colorized string
     */
    bold(str) {
        return this.colorize(str, 1);
    }
}

module.exports = Colorizer;
