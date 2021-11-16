import React, { createRef, useEffect } from 'react';
import TextInput from 'react-autocomplete-input';
import { omit } from "lodash";

type AutoCompleteTextInputProps = {
  value: string | number;
  onSelect: (value: string | number) => void;
  maxOptions?: number;
  onRequestOptions?: () => void;
  matchAny?: boolean;
  options?: string[] | number[];
  regex?: string;
  requestOnlyIfNoOptions?: boolean;
  spaceRemovers?: string[];
  spacer?: string;
  minChars?: number;
  [key: string]: any;
};

const AutoCompleteTextInput = ({ onSelect, value, ...rest }: AutoCompleteTextInputProps) => {
  const inputRef = createRef();

  useEffect(() => {
    const els = document.getElementsByClassName('react-autocomplete-input');

    if (els.length) {
      const el = els[0] as HTMLElement;

      el.style.left = 'unset';
      el.style.top = 'unset';
    }
  }, [value]);

  rest = omit(rest, 'changeOnSelect', 'defaultValue', 'trigger', 'passThroughEnter',
    'Component', 'type', 'value', 'onSelect', 'offsetX', 'offsetY');

  return <TextInput ref={inputRef} Component={'input'} type={'text'} trigger={''} passThroughEnter={false}
                    value={value} onSelect={onSelect} {...rest}/>;
};

export default AutoCompleteTextInput;
