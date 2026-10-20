// Transpilation config for application sources (src/).
//
// Historically this was `babel-preset-react-app`. That preset is unmaintained
// and dragged in vulnerable transitive dependencies, so it is replaced here by
// the underlying @babel presets it wrapped. Semantics are unchanged: CRA also
// compiled TypeScript via Babel (not tsc), so per-file transpilation and its
// limitations (e.g. no cross-file `const enum` inlining) already applied.
//
// `@babel/preset-env` reads browser targets from the `browserslist` field in
// package.json automatically.
module.exports = function (api) {
  const isEnvDevelopment = api.env("development");

  return {
    presets: [
      ["@babel/preset-env", { bugfixes: true }],
      ["@babel/preset-react", { runtime: "automatic" }],
      "@babel/preset-typescript",
    ],
    plugins: [
      // Fast Refresh during development only.
      isEnvDevelopment && require.resolve("react-refresh/babel"),
    ].filter(Boolean),
  };
};
