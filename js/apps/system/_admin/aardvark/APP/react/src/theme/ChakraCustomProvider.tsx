import { ChakraProvider } from "@chakra-ui/react";
import { Global } from "@emotion/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import React, { ReactNode } from "react";
import { theme } from "./theme";

const queryClient = new QueryClient();

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
      },
      "a:focus": {
        outline: "none"
      }
    };
  }
  return (
    <QueryClientProvider client={queryClient}>
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
              },
            svg: {
              overflow: "visible !important"
            },
            ".chakra-toast__inner button[aria-label='Close']": {
              top: "12px !important",
              right: "12px !important"
            },
            "blockquote, dl, dd, h1, h2, h3, h4, h5, h6, hr, figure, p, pre": {
              margin: 0
            },
            "label, input, button, select, textarea": {
              lineHeight: "inherit"
            },
            "pre, code, kbd,samp": {
              fontFamily: "SFMono-Regular,Menlo,Monaco,Consolas,monospace",
              fontSize: "1em"
            }
          }}
        />
        {children}
      </ChakraProvider>
    </QueryClientProvider>
  );
};
