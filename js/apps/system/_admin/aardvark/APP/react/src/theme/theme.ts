// 1. Import `extendTheme`
import { extendTheme, TooltipProps } from "@chakra-ui/react";
import { mode, cssVar } from "@chakra-ui/theme-tools";

const $tooltipBg = cssVar("tooltip-bg");

// 2. Call `extendTheme` and pass your custom values
export const theme = extendTheme({
  colors: {
    gray: {
      50: "#fafafa",
      100: "#f5f5f5",
      200: "#eeeeee",
      300: "#e0e0e0",
      400: "#bdbdbd",
      500: "#9e9e9e",
      600: "#757575",
      700: "#616161",
      800: "#424242",
      900: "#212121"
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
  }
});
