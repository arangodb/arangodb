import { useField, useFormikContext } from "formik";
import React from "react";
import Select, { Props as ReactSelectProps } from "react-select";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

type OptionType = {
  label: string;
  value: string;
};
export type InputControlProps = BaseFormControlProps & {
  selectProps?: ReactSelectProps<OptionType>;
};

export const SelectControl = (props: InputControlProps) => {
  const { name, label, selectProps, ...rest } = props;
  const [field, , helper] = useField(name);
  const { isSubmitting } = useFormikContext();
  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <Select
        {...field}
        id={name}
        isDisabled={isSubmitting}
        {...selectProps}
        onChange={(value, action) => {
          console.log({value, action});
          helper.setValue(value)
        }}
      />
    </FormikFormControl>
  );
};
