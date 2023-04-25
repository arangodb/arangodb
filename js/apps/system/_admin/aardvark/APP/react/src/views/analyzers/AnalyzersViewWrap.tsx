import React from "react";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { AnalyzersView } from "./AnalyzersView";

export const AnalyzersViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return <AnalyzersView />;
};
