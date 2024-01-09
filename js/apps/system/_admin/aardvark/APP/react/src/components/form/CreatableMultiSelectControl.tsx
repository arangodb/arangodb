import { useField, useFormikContext } from "formik";
import React from "react";
import { Props as ReactSelectProps, PropsValue } from "react-select";
import CreatableMultiSelect from "../select/CreatableMultiSelect";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

type OptionType = {
  label: string;
  value: string;
};
export type InputControlProps = BaseFormControlProps & {
  selectProps?: ReactSelectProps<OptionType, true>;
};

export const CreatableMultiSelectControl = (props: InputControlProps) => {
  const { name, label, selectProps, ...rest } = props;
  const [field, , helper] = useField<string | string[] | undefined>(name);
  const { isSubmitting } = useFormikContext();
  const value: PropsValue<OptionType> = getValue({
    field,
    options: selectProps?.options as OptionType[]
  });

  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <CreatableMultiSelect
        {...field}
        value={value}
        inputId={name}
        isDisabled={rest.isDisabled || isSubmitting}
        {...selectProps}
        onChange={selectedOptions => {
          const valueStringArray = selectedOptions.map(value => {
            return value.value;
          });
          helper.setValue(valueStringArray);
        }}
      />
    </FormikFormControl>
  );
};

const createValue = (value: string | string[]) => {
  if (Array.isArray(value)) {
    return value.map(value => {
      return {
        value,
        label: value
      };
    });
  }
  return [
    {
      value,
      label: value
    }
  ];
};

const getValue = ({
  field,
  options
}: {
  field: { value: string | string[] | undefined };
  options?: readonly OptionType[];
}) => {
  if (!field.value) {
    return [];
  }

  // if no options provided, it's a newly created value
  if (!options || !options.length) {
    return createValue(field.value);
  }

  if (typeof field.value === "string") {
    // if it's a string, find matching options
    const foundValues = options.filter(option => {
      return option.value === field.value;
    });
    if (foundValues && foundValues.length > 0) {
      return foundValues;
    } else {
      // if not found, it's a newly created value
      return createValue(field.value);
    }
  }

  // if not a string, simply convert to array of objects
  return createValue(field.value);
};
