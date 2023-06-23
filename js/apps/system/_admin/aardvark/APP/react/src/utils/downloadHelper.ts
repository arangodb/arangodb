import { getAardvarkRouteForCurrentDb } from "./arangoClient";
export const downloadPost = async ({
  url,
  body
}: {
  url: string;
  body: any;
}) => {
  const route = getAardvarkRouteForCurrentDb(url);
  const response = await route.request({
    expectBinary: true,
    method: "POST",
    body
  });
  const blobUrl = window.URL.createObjectURL(response.body);
  const filename =
    response.headers?.["content-disposition"]?.replace(
      /.* filename="([^")]*)"/,
      "$1"
    ) || "";
  downloadBlob(blobUrl, filename);
};

export const download = async (url: string) => {
  const route = getAardvarkRouteForCurrentDb(url);
  const response = await route.request({
    expectBinary: true,
    method: "GET"
  });
  const blobUrl = window.URL.createObjectURL(response.body);
  const filename =
    response.headers?.["content-disposition"]?.replace(
      /.* filename="([^")]*)"/,
      "$1"
    ) || "download";
  downloadBlob(blobUrl, filename);
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
