import { useEffect } from "react";

/**
 * This resets extra margin and padding present in these divs:
 * - .resizeobserver.contentWrapper
 * - #content
 * Adds it back on unmount.
 */
export const useGlobalStyleReset = () => {
  useEffect(() => {
    const contentWrapper = document.querySelector(".contentWrapper");
    const contentDiv = document.querySelector("#content");
    contentWrapper && contentWrapper.setAttribute("style", "padding: 0;");
    // todo - fix this display none
    contentDiv &&
      contentDiv.setAttribute("style", "margin: 0; padding: 0; display: none;");
    return () => {
      contentDiv && contentDiv.removeAttribute("style");
      contentWrapper && contentWrapper.removeAttribute("style");
    };
  }, []);
};
