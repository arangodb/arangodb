import React from "react";
import { createRoot } from "react-dom/client";

const renderReactComponent = (element: React.ReactNode) => {
  const container = document.getElementById("content-react");
  window.reactRootElement = createRoot(container!);
  window.reactRootElement.render(element);
};

const unmountReactComponents = () => {
  window.reactRootElement && window.reactRootElement.unmount();
};
window.renderReactComponent = renderReactComponent;
window.unmountReactComponents = unmountReactComponents;
declare global {
  interface Window {
    renderReactComponent: any;
    unmountReactComponents: any;
    reactRootElement: any;
  }
}

export {};
