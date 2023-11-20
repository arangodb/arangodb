// 1. Import `extendTheme`
import { extendTheme, TooltipProps } from "@chakra-ui/react";
import { mode, cssVar } from "@chakra-ui/theme-tools";

const $tooltipBg = cssVar("tooltip-bg");

// 2. Call `extendTheme` and pass your custom values
export const theme = extendTheme({
  fonts: {
    heading: "Inter, sans-serif",
    body: "Inter, sans-serif"
  },
  colors: {
    gray: {
      50: "#f8f8f8",
      100: "#f0f0f0",
      200: "#e5e5e5",
      300: "#d1d1d1",
      400: "#b4b4b4",
      500: "#9a9a9a",
      600: "#818181",
      700: "#6a6a6a",
      800: "#5a5a5a",
      900: "#4e4e4e",
      950: "#282828"
    }
  },
  components: {
    Tooltip: {
      baseStyle: (props: TooltipProps) => {
        const bg = mode("gray.900", "gray.300")(props);
        return {
          [$tooltipBg.variable]: `colors.${bg}`
        };
      }
    },
    Popover: {
      baseStyle: {
        popper: {
          zIndex: 500
        }
      }
    },
    Modal: {
      sizes: {
        max: {
          dialog: {
            maxW: "calc(100vw - 200px)",
            minH: "100vh",
            my: 0
          }
        }
      }
    }
  },
  styles: {
    global: {
      body: {
        color: "gray.950"
      }
    }
  }
});
