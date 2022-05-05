import React, { ReactNode, useEffect, useState } from 'react';
import styled from 'styled-components';
import { uniqueId } from 'lodash';
import PlainLabel from "./PlainLabel";

type StyledTextinputProps = {
  type: 'text' | 'number';
  [key: string]: any;
};

/*
const StyledTextinput = styled.input.attrs(({ type, ...rest }: StyledTextinputProps) => ({
  type,
  ...rest
}))`
  &&& {
    width: 50%;
    height: auto;
  }
`;
*/
const StyledTextinput = styled.input.attrs(({ type, width, ...rest }: StyledTextinputProps) => ({
  type,
  ...rest
}))`
  &&& {
    width: ${props => props.width || '50%'};
    height: auto;
    margin-right: 8px;
    color: #555555;
    border: 2px solid rgba(140, 138, 137, 0.25);
    border-radius: 4px;
    background-color: #fff !important;
    box-shadow: none;
    outline: none;
    outline-color: transparent;
    outline-style: none;
  }
`;

type TextinputProps = {
  id?: string;
  label?: ReactNode;
  disabled?: boolean;
  width?: string;
} & StyledTextinputProps;

const Textinput = ({ id, label, disabled, width, ...rest }: TextinputProps) => {
  const [thisId, setThisId] = useState(id || uniqueId('textinput-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);


  return <>
    {label ? <PlainLabel htmlFor={thisId}>{label}</PlainLabel> : null}
    <StyledTextinput id={thisId} disabled={disabled} width={width} {...rest}/>
  </>;
};

export default Textinput;
