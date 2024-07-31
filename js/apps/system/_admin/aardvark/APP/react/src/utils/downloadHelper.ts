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
    const blobUrl = makeBlobUrl(response.parsedBody);
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
  const blobUrl = window.URL.createObjectURL(response.parsedBody);
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

export const downloadBlob = (blobUrl: string, filename?: string) => {
  const a = document.createElement("a");
  a.href = blobUrl;
  a.download = filename || "download";
  document.body.appendChild(a);
  a.click();
  window.setTimeout(() => {
    window.URL.revokeObjectURL(blobUrl);
    document.body.removeChild(a);
  }, 500);
};

const typeToBlobType = (type?: "csv" | "json" | "text") => {
  switch (type) {
    case "csv":
      return "text/csv";
    case "json":
      return "application/json";
    case "text":
      return "text/plain";
    default:
      return "application/octet-stream";
  }
};
export const downloadLocalData = ({
  data,
  fileName,
  type
}: {
  data: any;
  fileName?: string;
  type?: "csv" | "json" | "text";
}) => {
  const blobType = typeToBlobType(type);
  if (!blobType) {
    throw new Error(`Unknown blob type: ${type}`);
  }
  const blob = new Blob([data], { type: blobType });
  const blobUrl = window.URL.createObjectURL(blob);
  downloadBlob(blobUrl, fileName);
};
