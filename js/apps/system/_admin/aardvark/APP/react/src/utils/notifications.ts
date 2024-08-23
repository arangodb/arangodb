import { createStandaloneToast, UseToastOptions } from "@chakra-ui/react";
import { theme } from "./../theme/theme";

const { toast } = createStandaloneToast({
  theme: theme
});

const notify = (message: string, options?: UseToastOptions) => {
  toast({
    isClosable: true,
    ...options,
    title: message
  });
};
export const notifyError = (message: string, options?: UseToastOptions) => {
  notify(message, { ...options, status: "error" });
};
export const notifySuccess = (message: string, options?: UseToastOptions) => {
  notify(message, { ...options, status: "success" });
};
export const notifyWarning = (message: string, options?: UseToastOptions) => {
  notify(message, { ...options, status: "warning" });
};
export const notifyInfo = (message: string, options?: UseToastOptions) => {
  notify(message, { ...options, status: "info" });
};
