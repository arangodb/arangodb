import { ChakraProvider } from "@chakra-ui/react";
import { Global } from "@emotion/react";
import React, { ReactNode } from "react";
import { theme } from "./theme";

export const ChakraCustomProvider = ({ children }: { children: ReactNode }) => {
  return (
    <ChakraProvider theme={theme}>
      {/* This is to override bootstrap styles */}
      <Global
        styles={{
          "input[type='number'], input[type='number']:focus": {
            height: '40px'
          }
        }}
      />
      {children}
    </ChakraProvider>
  );
};
