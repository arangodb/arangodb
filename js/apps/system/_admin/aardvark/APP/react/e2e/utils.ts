import { Page } from "@playwright/test";
import { expect } from "./fixtures";

export const createCollection = async ({
  collectionName,
  page
}: {
  collectionName: string;
  page: Page;
}) => {
  // using dispatch to avoid flakiness (https://github.com/microsoft/playwright/issues/13576)
  await page
    .getByRole("button", { name: "Add collection" })
    .dispatchEvent("click");
  await page.locator("#name").fill(collectionName);
  await page.getByRole("button", { name: "Create" }).click();
  const newCollectionNotification = page.locator(".noty_body");
  await expect(newCollectionNotification).toHaveText(
    `The collection: ${collectionName} was successfully created`
  );

  // using force click to avoid flakiness
  await newCollectionNotification.click({ force: true });
};

export const COMPUTED_VALUES = [
  {
    name: "dateCreatedHumanReadable",
    expression: "RETURN DATE_ISO8601(DATE_NOW())",
    overwrite: true
  },
  {
    name: "dateCreatedForIndexing",
    expression: "RETURN DATE_NOW()",
    overwrite: true
  },
  {
    name: "FullName",
    expression:
      "RETURN MERGE(@doc.name, {full: CONCAT(@doc.name.first, ' ', @doc.name.last)})",
    overwrite: true,
    computeOn: ["insert", "update", "replace"]
  }
];

export const DOCUMENT = {
  name: {
    first: "Sam",
    last: "Smith"
  },
  address: "Hans-Sachs-Str",
  x: 12.9,
  y: -284
};
