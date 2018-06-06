# TypeScript

Have all code in subfolder
```
  manifest.json
  src/
    api/
      index.js
      ...
    scripts/
      setup.js
```
Use `tsc` with `tsconfig.json` like
```json
{
  "compilerOptions": {
    // ...
    "outDir": "./build/",
    "rootDir": "./src/"
  }
}
```
Add build dir to `.gitignore`, add src dir to `.foxxignore`, use `foxx-cli` to install.
