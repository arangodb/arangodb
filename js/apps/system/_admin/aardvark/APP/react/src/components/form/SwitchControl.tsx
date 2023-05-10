import { Switch, SwitchProps } from "@chakra-ui/react";
import { useField, useFormikContext } from "formik";
import React from "react";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

export type InputControlProps = BaseFormControlProps & {
  switchProps?: SwitchProps;
};

export const SwitchControl = (props: InputControlProps) => {
  const { name, label, switchProps, ...rest } = props;
  const [field] = useField(name);
  const { isSubmitting } = useFormikContext();
  return (
    <FormikFormControl
      name={name}
      label={label}
      isDisabled={isSubmitting}
      {...rest}
    >
      <Switch {...field} isChecked={!!field.value} id={name} {...switchProps} />
    </FormikFormControl>
  );
};
