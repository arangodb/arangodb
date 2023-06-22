import { getAardvarkRouteForCurrentDb } from "./arangoClient";
export const downloadPost = async ({
  url,
  body
}: {
  url: string;
  body: any;
}) => {
  // var xhr = new XMLHttpRequest();
  // xhr.onreadystatechange = function () {
  //   if (this.readyState === 4 && this.status === 200) {
  //     if (callback) {
  //       callback();
  //     }
  //     var a = document.createElement("a");
  //     a.download = this.getResponseHeader("Content-Disposition").replace(
  //       /.* filename="([^")]*)"/,
  //       "$1"
  //     );
  //     document.body.appendChild(a);
  //     var blobUrl = window.URL.createObjectURL(this.response);
  //     a.href = blobUrl;
  //     a.click();

  //     window.setTimeout(function () {
  //       window.URL.revokeObjectURL(blobUrl);
  //       document.body.removeChild(a);
  //     }, 500);
  //   } else {
  //     if (this.readyState === 4) {
  //       if (errorCB !== undefined) {
  //         errorCB(this.status, this.statusText);
  //       }
  //     }
  //   }
  // };
  // xhr.open("POST", url);
  // if (window.arangoHelper.getCurrentJwt()) {
  //   xhr.setRequestHeader(
  //     "Authorization",
  //     "bearer " + window.arangoHelper.getCurrentJwt()
  //   );
  // }
  // xhr.responseType = "blob";
  // xhr.send(body);
  // convert to arangojs
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
