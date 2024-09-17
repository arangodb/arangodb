import { expect, test } from "./fixtures";

test.describe("Collection Page", () => {
  test("can create a collection", async ({ page }) => {
    // create collection
    await page.getByRole("button", { name: "Add collection" }).click();
    await page.locator("#name").fill("myCollection");
    await page.getByRole("button", { name: "Create" }).click();
    const newCollectionNotification = page.locator(".noty_body");
    await expect(newCollectionNotification).toHaveText(
      "The collection: myCollection was successfully created"
    );
  });

  test("can delete a collection", async ({ page }) => {
    // create collection
    await page.getByRole("button", { name: "Add collection" }).click();
    await page.locator("#name").fill("myCollection2");
    await page.getByRole("button", { name: "Create" }).click();
    const newCollectionNotification = page.locator(".noty_body");
    await expect(newCollectionNotification).toHaveText(
      "The collection: myCollection2 was successfully created"
    );

    // delete collection
    await page.getByRole("link", { name: "myCollection2" }).click();
    await page.locator(".subMenuEntry", { hasText: "Settings" }).click();
    await page.locator("button", { hasText: "Delete" }).click();
    await page.locator("button", { hasText: "Yes" }).click();
    const deleteCollectionNotification = page.locator(".noty_body");
    await expect(deleteCollectionNotification).toHaveText(
      "Collection successfully dropped"
    );
  });
});
