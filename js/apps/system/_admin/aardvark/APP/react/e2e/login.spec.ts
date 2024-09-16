import { test, expect } from "@playwright/test";

test("has title", async ({ page }) => {
  await page.goto("http://localhost:3001/");

  // Expect a title "to contain" a substring.
  await expect(page).toHaveTitle(/ArangoDB/);
});

test("login to DB", async ({ page }) => {
  await page.goto("http://localhost:3001/");
  await page.getByPlaceholder("Username").fill("root");
  await page.getByPlaceholder("Password").fill("");
  await page.getByRole("button", { name: "Login" }).click();
  await page.getByRole("button", { name: /Select DB/i }).click();

  await page.waitForURL("**/index.html#collections");
  // expect heading Collections
  await expect(
    page.getByRole("heading", { name: "Collections" })
  ).toBeVisible();
});

