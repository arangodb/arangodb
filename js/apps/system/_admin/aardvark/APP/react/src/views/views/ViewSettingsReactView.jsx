import React from "react";
import { ViewSettingsWrap } from "./ViewSettingsWrap";

const ViewSettingsReactView = ({ name }) => {
  return <ViewSettingsWrap name={name} />;
};
window.ViewSettingsReactView = ViewSettingsReactView;
