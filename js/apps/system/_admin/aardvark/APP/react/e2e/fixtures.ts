import { test as base } from "@playwright/test";
import fs from "fs";
import path from "path";

export const test = base.extend({
  page: async ({ page }, use) => {
    await page.addInitScript(async () => {
      const sessionStorage = JSON.parse(
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
      }, sessionStorage);
      use(page);
    });
  }
});
export { expect } from "@playwright/test";
