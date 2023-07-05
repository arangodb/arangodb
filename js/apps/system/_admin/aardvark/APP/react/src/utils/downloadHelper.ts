import { getAardvarkRouteForCurrentDb } from "./arangoClient";
export const downloadPost = async ({
  url,
  body
}: {
  url: string;
  body: any;
}) => {
  const route = getAardvarkRouteForCurrentDb(url);
  try {
    const response = await route.request({
      expectBinary: true,
      method: "POST",
      body
    });
    const blobUrl = makeBlobUrl(response.body);
    const filename = makeBlobFilename(response.headers) || "download";
    downloadBlob(blobUrl, filename);
  } catch (e: any) {
    window.arangoHelper.arangoError(
      "Error",
      `Could not download: ${e.message} `
    );
  }
};

export const download = async (url: string) => {
  const route = getAardvarkRouteForCurrentDb(url);
  const response = await route.request({
    expectBinary: true,
    method: "GET"
  });
  const blobUrl = window.URL.createObjectURL(response.body);
  const filename = makeBlobFilename(response.headers) || "download";
  downloadBlob(blobUrl, filename);
};

const makeBlobUrl = (body: any) => {
  return window.URL.createObjectURL(body);
};
const makeBlobFilename = (headers: any) => {
  return headers?.["content-disposition"]?.replace(
    /.* filename="([^")]*)"/,
    "$1"
  );
};

const downloadBlob = (blobUrl: string, filename: string) => {
  const a = document.createElement("a");
  a.href = blobUrl;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  window.setTimeout(() => {
    window.URL.revokeObjectURL(blobUrl);
    document.body.removeChild(a);
  }, 500);
};
