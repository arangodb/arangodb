import { Page } from "@playwright/test";
import { expect } from "./fixtures";

export const createCollection = async ({
  collectionName,
  page
}: {
  collectionName: string;
  page: Page;
}) => {
  await page.getByRole("button", { name: "Add collection" }).click();
  await page.locator("#name").fill(collectionName);
  await page.getByRole("button", { name: "Create" }).click();
  const newCollectionNotification = page.locator(".noty_body");
  await expect(newCollectionNotification).toHaveText(
    `The collection: ${collectionName} was successfully created`
  );
  await newCollectionNotification.click();
};
