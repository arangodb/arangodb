import React, { useState } from "react";

import Select, {
  SelectInstance,
  components,
  OptionProps,
  Props,
  GroupBase
} from "react-select";
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

export const getSelectBase = <IsMulti extends boolean = false>(
  SelectComponent: Select
) =>
  React.forwardRef(
    (
      props: Props<OptionType, IsMulti> & { normalize?: boolean },
      ref: React.Ref<SelectInstance<OptionType, IsMulti, GroupBase<OptionType>>>
    ) => {
      const { normalize = true, ...rest } = props;
      const [inputValue, setInputValue] = useState("");
      const onInputChange = (inputValue: string) => {
        setInputValue(inputValue.normalize());
      };
      return (
        <SelectComponent
          ref={ref}
          inputValue={
            normalize && !props.onInputChange ? inputValue : props.inputValue
          }
          onInputChange={
            normalize && !props.onInputChange ? onInputChange : undefined
          }
          {...rest}
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
    }
  );
