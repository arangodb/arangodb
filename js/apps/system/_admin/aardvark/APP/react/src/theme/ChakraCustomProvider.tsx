import { ChakraProvider } from "@chakra-ui/react";
import { Global } from "@emotion/react";
import React, { ReactNode } from "react";
import { theme } from "./theme";

export const ChakraCustomProvider = ({
  children,
  overrideNonReact
}: {
  children: ReactNode;
  // used by components which are rendered in content-react div
  overrideNonReact?: boolean;
}) => {
  let overrideNonReactStyles = {};
  if (overrideNonReact) {
    overrideNonReactStyles = {
      ".contentWrapper": {
        display: "none"
      },
      ".reactContainer": {
        height: "auto",
        display: "flex",
        width: "100%"
      },
      ".centralRow": {
        backgroundColor: "white !important"
      }
    };
  }
  return (
    <ChakraProvider theme={theme}>
      <Global
        styles={{
          // {/* This is to override the non-react wrappers */}
          ...overrideNonReactStyles,
          // {/* This is to override bootstrap styles */}
          "input[type='number'], input[type='password']": {
            borderColor: "var(--chakra-colors-gray-200)"
          },
          "input[type='password']": {
            fontSize: "var(--chakra-fontSizes-md)",
            padding: 0,
            paddingLeft: "var(--chakra-space-4)",
            paddingRight: "var(--chakra-space-4)"
          },
          "input[type='number'], input[type='password'], input[type='number']:focus, input[type='password']:focus":
            {
              height: "40px"
            }
        }}
      />
      {children}
    </ChakraProvider>
  );
};
