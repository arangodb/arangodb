import React, { ReactNode, useEffect, useState } from 'react';
import styled from 'styled-components';
import { uniqueId } from 'lodash';
import PlainLabel from "./PlainLabel";

type StyledTextinputProps = {
  type: 'text' | 'number';
  [key: string]: any;
};

const StyledTextinput = styled.input.attrs(({ type, ...rest }: StyledTextinputProps) => ({
  type,
  ...rest
}))`
  &&& {
    width: 90%;
    height: auto;
  }
`;

type TextinputProps = {
  id?: string;
  label?: ReactNode;
  disabled?: boolean;
} & StyledTextinputProps;

const Textinput = ({ id, label, disabled, ...rest }: TextinputProps) => {
  const [thisId, setThisId] = useState(id || uniqueId('textinput-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);


  return <>
    {label ? <PlainLabel htmlFor={thisId}>{label}</PlainLabel> : null}
    <StyledTextinput id={thisId} disabled={disabled} {...rest}/>
  </>;
};

export default Textinput;
