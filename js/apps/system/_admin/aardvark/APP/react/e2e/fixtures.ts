import { expect, test as base } from "@playwright/test";
import fs from "fs";
import path from "path";

export const test = base.extend({
  page: async ({ page }, use) => {
    const sessionStorageData = JSON.parse(
      fs.readFileSync(
        path.join(__dirname, "../playwright/.auth/session.json"),
        "utf-8"
      )
    );
    await page.context().addInitScript(storage => {
      if (window.location.hostname === "localhost") {
        for (const [key, value] of Object.entries(storage)) {
          window.sessionStorage.setItem(key, value as any);
        }
      }
    }, sessionStorageData);
    await page.goto("http://localhost:3001/");
    // switch db
    await expect(page.locator("#dbStatus .state")).toBeVisible();
    await page.locator("#dbStatus .state").click();
    // expect option 'test' to be available
    await expect(
      page.locator("#loginDatabase option", { hasText: "test" })
    ).toBeAttached();
    await page.locator("#loginDatabase").selectOption({ value: "test" });
    await page.getByRole("button", { name: /Select DB: test/i }).click();
    await expect(page.locator("#dbStatus .state")).toHaveText("test");
    use(page);
  }
});
export { expect } from "@playwright/test";
