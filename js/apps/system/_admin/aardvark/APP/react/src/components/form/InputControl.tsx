import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";
import { useField, useFormikContext } from "formik";
import React from "react";
import { Input, InputProps } from "@chakra-ui/react";

export type InputControlProps = BaseFormControlProps & {
  inputProps?: InputProps;
};

export const InputControl = (props: InputControlProps) => {
  const { name, label, inputProps, ...rest } = props;
  const [field] = useField(name);
  const { isSubmitting } = useFormikContext();
  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <Input {...field} id={name} isDisabled={isSubmitting} {...inputProps} />
    </FormikFormControl>
  );
};
