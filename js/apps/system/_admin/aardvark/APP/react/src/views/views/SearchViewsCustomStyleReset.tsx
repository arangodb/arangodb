import { Global } from "@emotion/react";
import React, { ReactNode } from "react";

export const SearchViewsCustomStyleReset = ({
  children
}: {
  children: ReactNode;
}) => {
  return (
    <>
      <Global
        styles={{
          // {/* This is to override bootstrap styles */}
          "input[type='number'], input[type='number']:focus": {
            height: "32px",
            border: "gray.200",
            marginBottom: "unset"
          }
        }}
      />
      {children}
    </>
  );
};
