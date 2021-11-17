/* eslint-disable jsx-a11y/anchor-is-valid,jsx-a11y/anchor-has-content */
import React, { MouseEvent, ReactNode, useEffect, useState } from "react";
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
  const [value, setValue] = useState<string | number>('');

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);

  const handleSelect = (val: string | number) => {
    onSelect(val);
    setValue('');
  };

  const getRemoveHandler = (val: string | number) => (event: MouseEvent<HTMLAnchorElement>) => {
    event.preventDefault();

    if (!disabled) {
      onRemove(val);
    }
  };

  const style = defaultsDeep(rest.style, {
    width: '90%',
    display: 'inline-flex'
  });
  delete rest.style;

  return <>
    {label ? <PlainLabel htmlFor={thisId}>{label}</PlainLabel> : null}
    <div className={'select2-container select2-container-multi'} id={thisId} style={style}>
      <ul className={'select2-choices'} style={{
        display: 'flex',
        flexWrap: 'wrap'
      }}>
        {
          values.map(value => <li className={'select2-search-choice'} key={value}>
            <div>{value}</div>
            <a href={'#'} className={'select2-search-choice-close'} tabIndex={-1}
               onClick={getRemoveHandler(value)}/>
          </li>)
        }
        <li className={'select2-search-field'} key={'_search'} style={{
          flexGrow: 1,
          float: 'none'
        }}>
          <AutoCompleteTextInput key={1} minChars={1} spacer={''} onSelect={handleSelect} matchAny={true}
                                 value={value} onChange={setValue} options={options}
                                 disabled={disabled} {...rest}/>
        </li>
      </ul>
    </div>
  </>;
};

export default AutoCompleteMultiSelect;
