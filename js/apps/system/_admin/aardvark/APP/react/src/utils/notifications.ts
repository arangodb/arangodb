import { createStandaloneToast, UseToastOptions } from "@chakra-ui/react";
import { theme } from "./../theme/theme";

const toast = createStandaloneToast({
  theme: theme
});

export const notifyError = (message: string, options?: UseToastOptions) => {
  toast({
    isClosable: true,
    ...options,
    title: message,
    status: "error"
  });
};
export const notifySuccess = (message: string, options?: UseToastOptions) => {
  toast({
    isClosable: true,
    ...options,
    title: message,
    status: "success"
  });
};
export const notifyWarning = (message: string, options?: UseToastOptions) => {
  toast({
    isClosable: true,
    ...options,
    title: message,
    status: "warning"
  });
};
export const notifyInfo = (message: string, options?: UseToastOptions) => {
  toast({
    isClosable: true,
    ...options,
    title: message,
    status: "info"
  });
};
