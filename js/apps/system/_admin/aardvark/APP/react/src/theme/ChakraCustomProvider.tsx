import { ChakraProvider } from "@chakra-ui/react";
import { Global } from "@emotion/react";
import React, { ReactNode } from "react";
import { theme } from "./theme";

export const ChakraCustomProvider = ({ children }: { children: ReactNode }) => {
  return (
    <ChakraProvider theme={theme}>
      <Global
        styles={{
          // {/* This is to override the non-react wrappers */}
          ".contentWrapper": {
            display: "none"
          },
          ".reactContainer": {
            height: "calc(100% - 120px)",
            display: "flex",
            width: "100%"
          },
          // {/* This is to override bootstrap styles */}
          "input[type='number'], input[type='number']:focus": {
            height: "40px"
          }
        }}
      />
      {children}
    </ChakraProvider>
  );
};
