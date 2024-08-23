import { useEffect } from "react";

/**
 * This function disables the bottom nav bar (subNav) using CSS
 * on React component mount, this will hide the nav bar,
 * and show it again when the component unmounts
 */
export const useDisableNavBar = () => {
  useEffect(() => {
    const bottomSubNameSelector = "#subNavigationBar .subMenuEntries.bottom";
    const bottomSubNav = $(bottomSubNameSelector);
    if (bottomSubNav.length) {
      $(bottomSubNav).hide();
    }
    const observer = disableSubNav();

    return () => {
      const bottomSubNavEl = $(bottomSubNameSelector);
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
  const target = $("#subNavigationBar")[0];
  const observer = new MutationObserver(function (mutations) {
    mutations.forEach(function (mutation) {
      const newNodes = mutation.addedNodes; // DOM NodeList
      if (newNodes !== null) {
        // If there are new nodes added
        const $nodes = $(newNodes); // jQuery set
        $nodes.each(function (_idx: number, node: NodeList) {
          const $node = $(node);
          if ($node.hasClass("bottom")) {
            $node.hide();
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
