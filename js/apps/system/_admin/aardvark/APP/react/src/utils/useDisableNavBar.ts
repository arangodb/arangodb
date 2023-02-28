import { useEffect } from "react";

const disableSubNav = () => {
  // Setup observer to watch for container divs creation, then disable subnav.
  // This is used during direct page loads or a page refresh.
  const target = $("#subNavigationBar")[0];
  const observer = new MutationObserver(function(mutations) {
    mutations.forEach(function(mutation) {
      const newNodes = mutation.addedNodes; // DOM NodeList
      if (newNodes !== null) {
        // If there are new nodes added
        const $nodes = $(newNodes); // jQuery set
        $nodes.each(function(_idx: number, node: NodeList) {
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

export const useDisableNavBar = () => {
  useEffect(() => {
    const bottomSubNav = $("#subNavigationBar .subMenuEntries.bottom")[0];
    if (bottomSubNav) {
      $(bottomSubNav).hide();
    }
    const observer = disableSubNav();

    return () => {
      observer.disconnect();
      $(bottomSubNav).show();
    };
  });
};
