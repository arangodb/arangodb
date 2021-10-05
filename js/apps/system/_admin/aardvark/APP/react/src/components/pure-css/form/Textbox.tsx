import React, { ReactNode, useEffect, useState } from 'react';
import styled from 'styled-components';
import { uniqueId } from 'lodash';
import PlainLabel from "./PlainLabel";

type StyledTextboxProps = {
  type: 'text' | 'number';
  [key: string]: any;
};

const StyledTextbox = styled.input.attrs(({ type, ...rest }: StyledTextboxProps) => ({
  type,
  ...rest
}))`
  &&& {
    width: 90%;
    height: auto;
  }
`;

type TextboxProps = {
  id?: string;
  label: ReactNode;
  disabled?: boolean;
} & StyledTextboxProps;

const Textbox = ({ id, label, disabled, ...rest }: TextboxProps) => {
  const [thisId, setThisId] = useState(id || uniqueId('textbox-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);


  return <>
    <PlainLabel htmlFor={thisId}>{label}</PlainLabel>
    <StyledTextbox id={thisId} disabled={disabled} {...rest}/>
  </>;
};

export default Textbox;
