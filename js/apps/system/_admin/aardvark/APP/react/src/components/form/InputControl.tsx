import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";
import { useField, useFormikContext } from "formik";
import React from "react";
import { Input, InputProps } from "@chakra-ui/react";

export type InputControlProps = BaseFormControlProps & {
  inputProps?: InputProps;
};

export const InputControl = React.forwardRef(
  (props: InputControlProps, ref: React.Ref<HTMLInputElement>) => {
    const { name, label, inputProps, ...rest } = props;
    const [field] = useField(name);
    const { isSubmitting } = useFormikContext();
    return (
      <FormikFormControl
        name={name}
        label={label}
        isDisabled={isSubmitting}
        {...rest}
      >
        <Input
          ref={ref}
          {...field}
          id={name}
          backgroundColor="white"
          {...inputProps}
        />
      </FormikFormControl>
    );
  }
);
