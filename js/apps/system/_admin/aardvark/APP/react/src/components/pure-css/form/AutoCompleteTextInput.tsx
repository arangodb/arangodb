import React, { ReactNode, useEffect, useState } from 'react';
import TextInput from 'react-autocomplete-input';
import { defaultsDeep, omit, uniqueId } from "lodash";
import PlainLabel from "./PlainLabel";

type AutoCompleteTextInputProps = {
  id?: string;
  label?: ReactNode;
  disabled?: boolean;
  value?: string | number;
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

const AutoCompleteTextInput = ({
                                 id,
                                 label,
                                 disabled,
                                 onSelect,
                                 value,
                                 ...rest
                               }: AutoCompleteTextInputProps) => {
  const [thisId, setThisId] = useState(id || uniqueId('textbox-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);

  useEffect(() => {
    const els = document.getElementsByClassName('react-autocomplete-input');

    if (els.length) {
      const el = els[0] as HTMLElement;

      el.style.left = 'unset';
      el.style.top = 'unset';
    }
  }, [value]);

  const style = defaultsDeep(rest.style, { width: '90%' });

  rest = omit(rest, 'style', 'changeOnSelect', 'defaultValue', 'trigger', 'passThroughEnter',
    'Component', 'type', 'value', 'onSelect', 'offsetX', 'offsetY');

  return <>
    {label ? <PlainLabel htmlFor={thisId}>{label}</PlainLabel> : null}
    <TextInput Component={'input'} type={'text'} id={thisId} disabled={disabled} trigger={''}
               passThroughEnter={false} value={value} onSelect={onSelect} style={style} {...rest}/>
  </>;
};

export default AutoCompleteTextInput;
