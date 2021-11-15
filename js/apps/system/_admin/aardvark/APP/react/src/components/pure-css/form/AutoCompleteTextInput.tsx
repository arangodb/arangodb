import React from 'react';
import TextInput from 'react-autocomplete-input';
import { omit } from "lodash";

type AutoCompleteTextInputProps = {
  value: string | number;
  onSelect: (value: string | number) => void;
  maxOptions?: number;
  onRequestOptions?: () => {};
  matchAny?: boolean;
  offsetX?: number;
  offsetY?: number;
  options?: string[] | number[];
  regex?: string;
  requestOnlyIfNoOptions?: boolean;
  spaceRemovers?: string[];
  spacer?: string;
  minChars?: number;
  [key: string]: any;
};

const AutoCompleteTextInput = ({ onSelect, value, ...rest }: AutoCompleteTextInputProps) => {
  rest = omit(rest, 'changeOnSelect', 'defaultValue', 'trigger', 'passThroughEnter', 'Component', 'type', 'value', 'onSelect');

  return <TextInput Component={'input'} type={'text'} trigger={''} passThroughEnter={false} value={value}
                    onSelect={onSelect} {...rest}/>;
};

export default AutoCompleteTextInput;
