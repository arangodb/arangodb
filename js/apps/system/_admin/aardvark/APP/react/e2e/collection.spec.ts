import { expect, test } from "./fixtures";
import { createCollection } from "./utils";

test.describe("Collection Page", () => {
  test("can create a collection", async ({ page }) => {
    // create collection
    await createCollection({ collectionName: "myCollection", page });
  });

  test("can delete a collection", async ({ page }) => {
    // create collection
    await createCollection({ collectionName: "myCollection2", page });

    // delete collection
    await page.getByRole("link", { name: "myCollection2" }).click();
    await page.locator(".subMenuEntry", { hasText: "Settings" }).click();
    await page.locator("button", { hasText: "Delete" }).click();
    await page.locator("button", { hasText: "Yes" }).click();
    const deleteCollectionNotification = page.locator(".noty_body").filter({
      hasText: "Collection successfully dropped"
    });
    await expect(deleteCollectionNotification).toHaveText(
      "Collection successfully dropped"
    );
  });

  test("computed values", async ({ page }) => {
    await createCollection({
      collectionName: "computedValuesCollection",
      page
    });
    await page.getByRole("link", { name: "computedValuesCollection" }).click();
    await page.locator(".subMenuEntry", { hasText: "Computed Values" }).click();
    await page.locator("#computedValuesEditor").click();
    await page.keyboard.press("Control+A");
    await page.keyboard.press("Backspace");
    await page.fill("#computedValuesEditor textarea", "return 1");
    expect(page.locator("#computedValuesEditor")).toBeVisible();
  });
});
