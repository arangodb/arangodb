import {
  FormControl,
  FormControlProps,
  FormErrorMessage,
  FormErrorMessageProps,
  FormLabel,
  FormLabelProps
} from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";

export type BaseFormControlProps = FormControlProps & {
  name: string;
  labelProps?: FormLabelProps;
  errorMessageProps?: FormErrorMessageProps;
  labelDirection?: "row" | "column";
};
export const FormikFormControl = ({
  errorMessageProps,
  name,
  children,
  label,
  labelProps,
  labelDirection,
  ...rest
}: BaseFormControlProps) => {
  const [, { error, touched }] = useField(name);
  return (
    <FormControl
      display={labelDirection === "row" ? "flex" : ""}
      alignItems={labelDirection === "row" ? "center" : ""}
      isInvalid={!!error && touched}
      {...rest}
    >
      <FormLabel htmlFor={name} {...labelProps}>
        {label}
      </FormLabel>
      {children}
      <FormErrorMessage {...errorMessageProps}>{error}</FormErrorMessage>
    </FormControl>
  );
};
