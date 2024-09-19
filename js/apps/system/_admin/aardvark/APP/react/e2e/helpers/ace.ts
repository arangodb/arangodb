import { Page } from "@playwright/test";

export const clearAceEditor = async ({
  page,
  containerId
}: {
  page: Page;
  containerId: string;
}) => {
  await page.click(`#${containerId} .ace_editor`);
  await page.evaluate((containerId: string) => {
    const editor = (
      document.querySelector(`#${containerId} .ace_editor`) as any
    )?.env?.editor;
    console.log("editor", editor);
    editor?.setValue("");
  }, containerId);
};

export const fillAceEditor = async ({
  page,
  containerId,
  value
}: {
  page: Page;
  containerId: string;
  value: any;
}) => {
  await page.fill(`#${containerId} textarea`, JSON.stringify(value));
};
