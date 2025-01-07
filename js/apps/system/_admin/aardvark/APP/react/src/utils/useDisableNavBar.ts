import { useEffect } from "react";

/**
 * This function disables the bottom nav bar (subNav) using CSS
 * on React component mount, this will hide the nav bar,
 * and show it again when the component unmounts
 */
export const useDisableNavBar = () => {
  useEffect(() => {
    const bottomSubNameSelector = "#subNavigationBar .subMenuEntries.bottom";
    const bottomSubNav = window.$(bottomSubNameSelector);
    if (bottomSubNav.length) {
      window.$(bottomSubNav).hide();
    }
    const observer = disableSubNav();

    return () => {
      const bottomSubNavEl = window.$(bottomSubNameSelector);
      bottomSubNavEl.show();
      observer.disconnect();
    };
  }, []);
};

/**
 * Setup observer to watch for container divs creation,
 * then disable subnav.
 * This is used during direct page loads or a page refresh.
 * */
const disableSubNav = () => {
  const target = window.$("#subNavigationBar")[0];
  const observer = new MutationObserver(function (mutations) {
    mutations.forEach(function (mutation) {
      const newNodes = mutation.addedNodes; // DOM NodeList
      if (newNodes !== null) {
        // If there are new nodes added
        const jqNodes = window.$(newNodes); // jQuery set
        jqNodes.each(function (_idx: number, node: NodeList) {
          const jqNode = window.$(node);
          if (jqNode.hasClass("bottom")) {
            jqNode.hide();
          }
        });
      }
    });
  });

  const config = {
    attributes: true,
    childList: true,
    characterData: true
  };

  observer.observe(target, config);

  return observer;
};
