const js = require("@eslint/js");
const tseslint = require("typescript-eslint");
const reactHooks = require("eslint-plugin-react-hooks");
const react = require("eslint-plugin-react");
const globals = require("globals");

// Flat ESLint config for the application sources under src/. Replaces the deprecated
// `eslint-config-react-app` preset. Scoped to src/ (the build output and legacy
// ../frontend are out of scope); run via `yarn lint`.
module.exports = tseslint.config(
  { ignores: ["build/**"] },

  // TypeScript / TSX application sources.
  {
    files: ["src/**/*.{ts,tsx}"],
    extends: [js.configs.recommended, ...tseslint.configs.recommended],
    languageOptions: {
      globals: globals.browser,
      parserOptions: { ecmaFeatures: { jsx: true } },
    },
    // no-explicit-any is demoted to a (non-blocking) warning: the codebase has a large
    // amount of pre-existing `any` usage that is tracked tech debt to burn down
    // incrementally, not something that should fail `yarn lint`. Correctness rules stay
    // as errors.
    rules: {
      "@typescript-eslint/no-explicit-any": "warn",
      // `cond && sideEffect()` / `cond ? a() : b()` are used deliberately in this codebase.
      "@typescript-eslint/no-unused-expressions": [
        "error",
        { allowShortCircuit: true, allowTernary: true },
      ],
    },
  },

  // Plain JS/JSX: the .jsx components and the legacy App.js bootstrap, which mixes ESM
  // imports with webpack require() calls and relies on Node/browser/jQuery globals.
  {
    files: ["src/**/*.{js,jsx}"],
    extends: [js.configs.recommended],
    languageOptions: {
      globals: { ...globals.browser, ...globals.node, ...globals.jquery },
      parserOptions: { ecmaFeatures: { jsx: true } },
    },
    // These files use the classic JSX runtime (explicit `import React`). Mark React and
    // JSX-referenced components as used so no-unused-vars doesn't false-positive on them.
    // (TSX files don't need this: the TS parser's no-unused-vars is already JSX-aware.)
    plugins: { react },
    rules: {
      "react/jsx-uses-react": "error",
      "react/jsx-uses-vars": "error",
    },
  },

  // React Hooks correctness rules (rules-of-hooks / exhaustive-deps) for all React sources.
  {
    files: ["src/**/*.{ts,tsx,js,jsx}"],
    plugins: { "react-hooks": reactHooks },
    rules: reactHooks.configs.recommended.rules,
  },

  // Declaration-merging files legitimately restate a third-party interface's generic
  // type parameters (e.g. augmenting @tanstack/table-core's ColumnMeta<TData, TValue>)
  // even when a given fragment doesn't reference them all.
  {
    files: ["src/**/*.d.ts"],
    rules: { "@typescript-eslint/no-unused-vars": "off" },
  }
);
