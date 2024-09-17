import { test as setup } from "@playwright/test";
import { Database } from "arangojs";
import fs from "fs";
import path from "path";

const sessionFile = path.join(__dirname, "../playwright/.auth/session.json");

setup("authenticate", async ({ page }) => {
  console.log("Setting up authentication");
  const db = new Database({
    url: "http://localhost:3001",
    auth: { username: "root", password: "" }
  });
  try {
    await db.createDatabase("test");
    console.log("Database test created");
  } catch (e) {
    await db.dropDatabase("test");
    console.log("Database test removed");
    await db.createDatabase("test");
    console.log("Database test created");
  }

  // Perform authentication steps. Replace these actions with your own.
  await page.goto("http://localhost:3001/");
  await page.getByPlaceholder("Username").fill("root");
  await page.getByPlaceholder("Password").fill("");
  await page.getByRole("button", { name: "Login" }).click();
  await page.locator("#loginDatabase").selectOption({ value: "test" });
  await page.getByRole("button", { name: /Select DB: test/i }).click();

  // Wait until the page receives the cookies.
  //
  // Sometimes login flow sets cookies in the process of several redirects.
  // Wait for the final URL to ensure that the cookies are actually set.
  await page.waitForURL("**/index.html#collections");

  // End of authentication steps.

  // Get session storage and store as env variable
  const storageInfo: any = await page.evaluate(() =>
    JSON.stringify(sessionStorage)
  );
  fs.writeFileSync(sessionFile, storageInfo, "utf-8");
});
