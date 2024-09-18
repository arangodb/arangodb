import { expect, test } from "./fixtures";
import { COMPUTED_VALUES, createCollection, DOCUMENT } from "./utils";

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

  test("setting computed values", async ({ page }) => {
    await createCollection({
      collectionName: "computedValuesCollection",
      page
    });
    // set computed values
    await page.getByRole("link", { name: "computedValuesCollection" }).click();
    await page.locator(".subMenuEntry", { hasText: "Computed Values" }).click();
    await page.locator("#computedValuesEditor").click();
    await page.keyboard.press("ControlOrMeta+A");
    await page.keyboard.press("Backspace");
    await page.fill(
      "#computedValuesEditor textarea",
      JSON.stringify(COMPUTED_VALUES)
    );
    await page.getByRole("button", { name: "Save" }).click();
    expect(page.locator("#computedValuesEditor")).toBeVisible();

    // check if computed values are set when adding a document
    await page.locator(".subMenuEntry", { hasText: "Content" }).click();
    await page.locator("#addDocumentButton").click();
    await page.locator("#jsoneditor .ace_content").click();
    await page.keyboard.press("ControlOrMeta+A");
    await page.keyboard.press("Backspace");
    await page.fill("#jsoneditor textarea", JSON.stringify(DOCUMENT));
    await page.getByRole("button", { name: "Create" }).click();
    expect(
      page.locator(".jsoneditor-field").filter({
        hasText: "dateCreatedForIndexing"
      })
    ).toBeVisible();
  });

  test("adding a document to a collection", async ({ page }) => {
    await createCollection({ collectionName: "myCollection4", page });
    await page.getByRole("link", { name: "myCollection4" }).click();
    await page.locator(".subMenuEntry", { hasText: "Content" }).click();
    await page.locator("#addDocumentButton").click();

    await page.locator("#jsoneditor").click();
    await page.fill("#jsoneditor textarea", JSON.stringify(DOCUMENT));
    await page.getByRole("button", { name: "Create" }).click();

    expect(
      page.locator(".jsoneditor-field").filter({
        hasText: "name"
      })
    ).toBeVisible();
  });
});
