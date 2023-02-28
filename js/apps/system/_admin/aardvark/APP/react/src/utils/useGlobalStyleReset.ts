import { useEffect } from "react";

export const useGlobalStyleReset = () => {
  useEffect(() => {
    const contentWrapper = document.querySelector(".contentWrapper");
    const contentDiv = document.querySelector("#content");
    contentWrapper && contentWrapper.setAttribute("style", "padding: 0;")
    contentDiv && contentDiv.setAttribute("style", "margin: 0; padding: 0;")
    return () => {
      contentDiv && contentDiv.removeAttribute("style");
      contentWrapper && contentWrapper.removeAttribute("style");
    };
  }, []);
};
