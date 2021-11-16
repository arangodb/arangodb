/* eslint-disable jsx-a11y/anchor-is-valid,jsx-a11y/anchor-has-content */
import React, { ReactNode, useEffect, useState } from "react";
import AutoCompleteTextInput from "./AutoCompleteTextInput";
import { defaultsDeep, uniqueId } from "lodash";
import PlainLabel from "./PlainLabel";

type AutoCompleteMultiSelectProps = {
  id?: string;
  label?: ReactNode;
  disabled?: boolean;
  values: string[] | number[];
  onRemove: (value: string | number) => void;
  onSelect: (value: string | number) => void;
  maxOptions?: number;
  onRequestOptions?: () => void;
  matchAny?: boolean;
  options?: string[] | number[];
  regex?: string;
  requestOnlyIfNoOptions?: boolean;
  minChars?: number;
  [key: string]: any;
};

const AutoCompleteMultiSelect = ({
                                   id,
                                   label,
                                   disabled,
                                   onSelect,
                                   values,
                                   onRemove,
                                   options,
                                   ...rest
                                 }: AutoCompleteMultiSelectProps) => {
  const [thisId, setThisId] = useState(id || uniqueId('textbox-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);

  const style = defaultsDeep(rest.style, {
    width: '90%',
    display: 'inline-flex'
  });
  delete rest.style;

  return <>
    {label ? <PlainLabel htmlFor={thisId}>{label}</PlainLabel> : null}
    <div className={'select2-container select2-container-multi'} id={thisId} style={style}>
      <ul className={'select2-choices'}>
        {
          values.map(value => <li className={'select2-search-choice'}>
            <div>{value}</div>
            <a href={'#'} className={'select2-search-choice-close'} tabIndex={-1}
               onClick={() => !disabled && onRemove(value)}/>
          </li>)
        }
        <li className={'select2-search-field'}>
          <AutoCompleteTextInput minChars={1} spacer={''} onSelect={onSelect} matchAny={true}
                                 options={options} style={{ width: 10 }} disabled={disabled} {...rest}/>
        </li>
      </ul>
    </div>
  </>;
};

export default AutoCompleteMultiSelect;
