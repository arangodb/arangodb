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
      50: "#f5faeb",
      100: "#e9f4d3",
      200: "#d5e9ad",
      300: "#b8da7c",
      400: "#9dc853",
      500: "#7ead35",
      600: "#608726",
      700: "#4b6922",
      800: "#3e5420",
      900: "#35481f",
      950: "#1a270c"
    },
    red: {
      50: "#fff2f1",
      100: "#ffe1df",
      200: "#ffc7c4",
      300: "#ffa19b",
      400: "#ff6a61",
      500: "#ff3b30",
      600: "#e6190d",
      700: "#cc1409",
      800: "#a8150c",
      900: "#8b1811",
      950: "#4c0703"
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
