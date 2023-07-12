import React from "react";
import { EditViewWrap } from "./editView/EditViewWrap";

const ViewSettingsReactView = ({ name }) => {
  return <EditViewWrap name={name} />;
};
window.ViewSettingsReactView = ViewSettingsReactView;
