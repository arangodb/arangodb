import { useField, useFormikContext } from "formik";
import React from "react";
import Select, { Props as ReactSelectProps, PropsValue } from "react-select";
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
  const value = selectProps?.options?.find(option => {
    return (option as OptionType).value === field.value;
  }) as PropsValue<OptionType>;
  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <Select
        {...field}
        value={value}
        inputId={name}
        isDisabled={isSubmitting}
        menuPortalTarget={document.body}
        {...selectProps}
        styles={{
          ...selectProps?.styles,
          menuPortal: (base) => ({ ...base, zIndex: 9999 }),
          container: base => {
            return { ...base, width: "100%" };
          }
        }}
        onChange={value => {
          helper.setValue((value as OptionType)?.value);
        }}
      />
    </FormikFormControl>
  );
};
