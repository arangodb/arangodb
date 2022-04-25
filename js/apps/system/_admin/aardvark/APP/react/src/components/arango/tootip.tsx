import React from "react";
import Tippy, { TippyProps } from "@tippy.js/react";

type ToolTipProps = {
  title: string;
  setArrow: boolean;
  children: TippyProps["children"];
};

const ToolTip = ({ title, setArrow, children }: ToolTipProps) => {
  return (
    <Tippy content={title} arrow={setArrow} placement="right">
      {children}
    </Tippy>
  );
};

export default ToolTip;
