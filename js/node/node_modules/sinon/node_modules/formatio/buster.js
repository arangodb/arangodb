exports["Browser"] = {
    // TODO: Needs fixing
    libs: [
        "node_modules/samsam/node_modules/lodash/lodash.js",
        "node_modules/samsam/lib/samsam.js"
    ],
    sources: ["lib/*.js"],
    tests: ["test/*-test.js"]
};

exports["Node"] = {
    extends: "Browser",
    environment: "node"
};
