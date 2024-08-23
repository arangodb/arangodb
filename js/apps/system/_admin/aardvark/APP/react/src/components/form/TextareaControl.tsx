import { Textarea, TextareaProps } from "@chakra-ui/react";
import { useField, useFormikContext } from "formik";
import React from "react";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

export type TextareaControlProps = BaseFormControlProps & {
  textareaProps?: TextareaProps;
};

export const TextareaControl = (props: TextareaControlProps) => {
  const { name, label, textareaProps, ...rest } = props;
  const [field] = useField(name);
  const { isSubmitting } = useFormikContext();
  return (
    <FormikFormControl
      name={name}
      label={label}
      isDisabled={isSubmitting}
      {...rest}
    >
      <Textarea
        {...field}
        id={name}
        backgroundColor="white"
        {...textareaProps}
      />
    </FormikFormControl>
  );
};
