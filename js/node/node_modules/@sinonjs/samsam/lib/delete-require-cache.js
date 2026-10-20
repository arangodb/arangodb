"use strict";

module.exports = function deleteRequireCache(requireFunc, moduleId) {
    if (
        typeof requireFunc !== "function" ||
        typeof requireFunc.resolve !== "function" ||
        !requireFunc.cache
    ) {
        return false;
    }

    delete requireFunc.cache[requireFunc.resolve(moduleId)];
    return true;
};
