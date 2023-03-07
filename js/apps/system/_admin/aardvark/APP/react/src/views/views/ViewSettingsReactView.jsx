/* global frontendConfig */
import React from "react";
import { ViewSettingsWrap } from "./ViewSettingsWrap";

const ViewSettingsReactView = ({ name }) => {
  return (
      <ViewSettingsWrap isCluster={frontendConfig.isCluster} name={name} />
  );
};
window.ViewSettingsReactView = ViewSettingsReactView;
