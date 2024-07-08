import React from "react";
import { createRoot } from "react-dom/client";

const renderReactComponent = (element: React.ReactNode) => {
  const container = document.getElementById("content-react");
  const root = createRoot(container!);
  root.render(element);
};

const unmountReactComponents = () => {
  const container = document.getElementById("content-react");
  const root = createRoot(container!);
  root && root.unmount();
};
window.renderReactComponent = renderReactComponent;
window.unmountReactComponents = unmountReactComponents;
declare global {
  interface Window {
    renderReactComponent: any;
    unmountReactComponents: any;
  }
}

export {};
