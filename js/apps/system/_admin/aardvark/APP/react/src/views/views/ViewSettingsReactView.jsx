/* global frontendConfig */
import React from "react";
import { ViewSettings } from "./ViewSettings";

const ViewSettingsReactView = ({ name }) => {
  return (
      <ViewSettings isCluster={frontendConfig.isCluster} name={name} />
  );
};
window.ViewSettingsReactView = ViewSettingsReactView;
