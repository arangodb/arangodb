import React, { useState } from "react";

import Select, { components, OptionProps, Props } from "react-select";
export type OptionType = {
  value: string;
  label: string;
};

const Option = <IsMulti extends boolean>(
  props: OptionProps<OptionType, IsMulti>
) => {
  return (
    <div title={props.data.value}>
      <components.Option {...props} />
    </div>
  );
};

export const getSelectBase =
  <IsMulti extends boolean = false>(SelectComponent: Select) =>
  (props: Props<OptionType, IsMulti> & { normalize?: boolean }) => {
    const { normalize = true, ...rest } = props;
    const [inputValue, setInputValue] = useState("");
    const onInputChange = (inputValue: string) => {
      setInputValue(inputValue.normalize());
    };
    return (
      <SelectComponent
        {...rest}
        inputValue={normalize ? inputValue : undefined}
        onInputChange={normalize ? onInputChange : undefined}
        menuPortalTarget={document.body}
        components={{
          ...props.components,
          Option
        }}
        styles={{
          ...props.styles,
          option: baseStyles => ({
            ...baseStyles,
            overflow: "hidden",
            textOverflow: "ellipsis"
          }),
          menuPortal: base => ({ ...base, zIndex: 9999 }),
          input: baseStyles => ({
            ...baseStyles,
            "> input": {
              background: "transparent !important"
            }
          })
        }}
      />
    );
  };
