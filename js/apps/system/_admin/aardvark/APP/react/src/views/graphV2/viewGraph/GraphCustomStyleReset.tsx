import { Global } from "@emotion/react";
import React, { ReactNode } from "react";

export const GraphCustomStyleReset = ({
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
            border: "gray.200"
          }
        }}
      />
      {children}
    </>
  );
};
