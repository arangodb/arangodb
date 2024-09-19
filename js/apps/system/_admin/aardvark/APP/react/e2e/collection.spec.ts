import { expect, test } from "./fixtures";
import { clearAceEditor, fillAceEditor } from "./helpers/ace";
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
    await clearAceEditor({ page, containerId: "computedValuesEditor" });
    await fillAceEditor({
      page,
      containerId: "computedValuesEditor",
      value: COMPUTED_VALUES
    });
    await page.getByRole("button", { name: "Save" }).click();
    expect(page.locator("#computedValuesEditor")).toBeVisible();

    // add a document & check if computed values are set
    await page.locator(".subMenuEntry", { hasText: "Content" }).click();
    await page.locator("#addDocumentButton").click();

    await clearAceEditor({ page, containerId: "jsoneditor" });
    await fillAceEditor({ page, containerId: "jsoneditor", value: DOCUMENT });

    await page.getByRole("button", { name: "Create" }).click();
    expect(
      page.locator(".jsoneditor-field").filter({
        hasText: "dateCreatedForIndexing"
      })
    ).toBeVisible();
    await page
      .locator(".jsoneditor-field")
      .filter({
        hasText: "dateCreatedForIndexing"
      })
      .click();
  });

  test("adding a document to a collection", async ({ page }) => {
    await createCollection({ collectionName: "myCollection4", page });
    await page.getByRole("link", { name: "myCollection4" }).click();
    await page.locator(".subMenuEntry", { hasText: "Content" }).click();
    await page.locator("#addDocumentButton").click();

    await clearAceEditor({ page, containerId: "jsoneditor" });
    await fillAceEditor({ page, containerId: "jsoneditor", value: DOCUMENT });

    await page.getByRole("button", { name: "Create" }).click();

    expect(
      page.locator(".jsoneditor-field").filter({
        hasText: "name"
      })
    ).toBeVisible();
  });
});
