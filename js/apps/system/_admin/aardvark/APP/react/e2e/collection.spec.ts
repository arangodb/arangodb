import { expect, test } from "./fixtures";

test("has title", async ({ page }) => {
  await page.goto("http://localhost:3001/");

  // Expect a title "to contain" a substring.
  await expect(page).toHaveTitle(/ArangoDB/);
});

test("starts on Collections tab", async ({ page }) => {
  await page.goto("http://localhost:3001/");
  await expect(
    page.getByRole("heading", { name: "Collections" })
  ).toBeVisible();
});

// test("cannot create a collection without name", async ({ page }) => {
//   await page.goto("http://localhost:3001/");
//   await page.getByRole("button", { name: "Add collection" }).click();
//   await page.getByRole("button", { name: "Create" }).click();
//   const text = await page.getByText("Collection name is required.");
//   expect(text).toBeVisible();
// });

test("can create a collection", async ({ page }) => {
  await page.goto("http://localhost:3001/");
  await page.getByRole("button", { name: "Add collection" }).click();
  await page.locator("#name").fill("myCollection");
  await page.getByRole("button", { name: "Create" }).click();
  await page.waitForSelector("text=myCollection");
  const text = await page.getByText("myCollection");
  expect(text).toBeVisible();
});
