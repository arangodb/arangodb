import React, { ReactNode, useEffect, useState } from 'react';
import TextInput from 'react-autocomplete-input';
import { defaultsDeep, omit, uniqueId } from "lodash";
import PlainLabel from "./PlainLabel";

type AutoCompleteTextInputProps = {
  id?: string;
  label?: ReactNode;
  disabled?: boolean;
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

const AutoCompleteTextInput = ({
                                 id,
                                 label,
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

  const style = defaultsDeep(rest.style, { width: '90%' });

  rest = omit(rest, 'style', 'changeOnSelect', 'defaultValue', 'trigger', 'passThroughEnter',
    'Component', 'type');

  return <>
    {label ? <PlainLabel htmlFor={thisId}>{label}</PlainLabel> : null}
    <TextInput Component={'input'} type={'text'} id={thisId} trigger={''} value={value} onSelect={onSelect}
               passThroughEnter={false} style={style} {...rest}/>
  </>;
};

export default AutoCompleteTextInput;
