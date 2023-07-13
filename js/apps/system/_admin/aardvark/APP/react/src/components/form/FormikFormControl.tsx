import {
  Grid,
  FormControl,
  FormControlProps,
  FormErrorMessage,
  FormErrorMessageProps,
  FormLabel,
  FormLabelProps
} from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";
import { InfoTooltip } from "../tooltip/InfoTooltip";

export type BaseFormControlProps = FormControlProps & {
  name: string;
  tooltip?: string;
  labelProps?: FormLabelProps;
  errorMessageProps?: FormErrorMessageProps;
};
export const FormikFormControl = ({
  errorMessageProps,
  name,
  tooltip,
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
      {tooltip ? (
        <Grid
          gridTemplateColumns={"1fr 40px"}
          columnGap="1"
          maxWidth="full"
          alignItems="center"
        >
          {children}
          {<InfoTooltip label={tooltip} />}
        </Grid>
      ) : (
        children
      )}
      <FormErrorMessage {...errorMessageProps}>{error}</FormErrorMessage>
    </FormControl>
  );
};
