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
  const a = document.createElement("a");
  a.href = blobUrl;
  a.download =
    response.headers?.["content-disposition"]?.replace(
      /.* filename="([^")]*)"/,
      "$1"
    ) || "";
  document.body.appendChild(a);
  a.click();
  window.setTimeout(() => {
    window.URL.revokeObjectURL(url);
    document.body.removeChild(a);
  }, 500);
};

export const download = async (url: string) => {
  const route = getAardvarkRouteForCurrentDb(url);
  const response = await route.request({
    expectBinary: true,
    method: "GET"
  });
  const blobUrl = window.URL.createObjectURL(response.body);
  const a = document.createElement("a");
  a.href = blobUrl;
  a.download =
    response.headers?.["content-disposition"]?.replace(
      /.* filename="([^")]*)"/,
      "$1"
    ) || "";
  document.body.appendChild(a);
  a.click();
  window.setTimeout(() => {
    window.URL.revokeObjectURL(url);
    document.body.removeChild(a);
  }, 500);
};
