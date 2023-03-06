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
};
export const FormikFormControl = ({
  errorMessageProps,
  name,
  children,
  label,
  labelProps,
  ...rest
}: BaseFormControlProps) => {
  const [, { error, touched }] = useField(name);
  return (
    <FormControl isInvalid={!!error && touched} {...rest}>
      {label ? (
        <FormLabel htmlFor={name} {...labelProps}>
          {label}
        </FormLabel>
      ) : null}
      {children}
      <FormErrorMessage {...errorMessageProps}>{error}</FormErrorMessage>
    </FormControl>
  );
};
