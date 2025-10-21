// 1. Import `extendTheme`
import {
  extendTheme,
  theme as defaultTheme,
  TooltipProps
} from "@chakra-ui/react";
import { cssVar, mode, StyleFunctionProps } from "@chakra-ui/theme-tools";

const $tooltipBg = cssVar("tooltip-bg");

// 2. Call `extendTheme` and pass your custom values
export const theme = extendTheme({
  fonts: {
    heading: "Inter, sans-serif",
    body: "Inter, sans-serif"
  },
  colors: {
    green: {
      "50": "#f4fef2",
      "100": "#e3f2e0",
      "200": "#98D78C",
      "300": "#55AB45",
      "400": "#217846",
      "500": "#007339",
      "600": "#006532",
      "700": "#005329",
      "800": "#004020",
      "900": "#002e17"
    },
    red: {
      "50": "#f8c5c6",
      "100": "#f5acae",
      "200": "#f18b8e",
      "300": "#ed6a6e",
      "400": "#e9494d",
      "500": "#e63035",
      "600": "#da1a20",
      "700": "#b3161a",
      "800": "#8b1114",
      "900": "#630c0e"
    },
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
    Tabs: {
      defaultProps: {
        colorScheme: "green"
      },
      sizes: {
        sm: {
          tablist: {
            height: "38px"
          }
        }
      },
      variants: {
        line: (props: StyleFunctionProps) => {
          if (props.colorScheme === "green") {
            return {
              tab: {
                _selected: {
                  color: "black",
                  borderColor: "green.600"
                }
              }
            };
          }
          return defaultTheme.components.Tabs.variants?.line;
        }
      }
    },
    Radio: {
      defaultProps: {
        colorScheme: "green"
      },
      baseStyle: {
        control: {
          _checked: {
            bg: "green.600",
            borderColor: "green.600"
          }
        }
      }
    },
    Checkbox: {
      defaultProps: {
        colorScheme: "green"
      },
      baseStyle: {
        control: {
          _checked: {
            bg: "green.600",
            borderColor: "green.600"
          }
        }
      }
    },
    Switch: {
      defaultProps: {
        colorScheme: "green"
      },
      baseStyle: {
        track: {
          _checked: {
            bg: "green.600"
          }
        }
      }
    },
    Button: {
      variants: {
        solid: (props: StyleFunctionProps) => {
          if (["green", "red"].includes(props.colorScheme)) {
            return {
              bg: `${props.colorScheme}.600`,
              color: "white",
              _hover: {
                bg: `${props.colorScheme}.700`
              },
              _active: {
                bg: `${props.colorScheme}.700`
              }
            };
          }
          if (props.colorScheme === "gray" || !props.colorScheme) {
            return {
              color: "gray.950"
            };
          }
          return defaultTheme.components.Button.variants?.solid(props);
        },
        outline: (props: StyleFunctionProps) => {
          if (props.colorScheme === "gray" || !props.colorScheme) {
            return {
              color: "gray.950"
            };
          }
        },
        ghost: (props: StyleFunctionProps) => {
          if (props.colorScheme === "gray" || !props.colorScheme) {
            return {
              color: "gray.950"
            };
          }
        }
      }
    },
    Table: {
      variants: {
        simple: {
          th: {
            color: "gray.700"
          }
        }
      }
    },
    Tooltip: {
      baseStyle: (props: TooltipProps) => {
        const bg = mode("gray.950", "gray.300")(props);
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
