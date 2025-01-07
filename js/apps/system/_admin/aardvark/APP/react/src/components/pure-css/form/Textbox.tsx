import React, { ReactNode, useEffect, useState } from 'react';
import styled from '@emotion/styled';
import { uniqueId } from 'lodash';
import PlainLabel from "./PlainLabel";

type StyledTextboxProps = {
  type: 'text' | 'number';
  [key: string]: any;
};

const StyledTextbox = styled.input`
  &&& {
    width: 90%;
    height: auto;
  }
`;

type TextboxProps = {
  id?: string;
  label?: ReactNode;
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
    {label ? <PlainLabel htmlFor={thisId}>{label}</PlainLabel> : null}
    <StyledTextbox id={thisId} disabled={disabled} {...rest}/>
  </>;
};

export default Textbox;
