import fs from "fs";

const setupSessionStorage = () => {
  // Set session storage in a new context
  const sessionStorage = JSON.parse(
    fs.readFileSync("playwright/.auth/session.json", "utf-8")
  );
  await context.addInitScript(storage => {
    if (window.location.hostname === "example.com") {
      for (const [key, value] of Object.entries(storage))
        window.sessionStorage.setItem(key, value);
    }
  }, sessionStorage);
};
