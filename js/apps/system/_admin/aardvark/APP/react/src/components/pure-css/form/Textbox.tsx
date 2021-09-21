import React, { ReactNode } from 'react';
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
  const { type } = rest;

  if (!id) {
    id = uniqueId(`textbox-${type}-`);
  }

  return <>
    <PlainLabel htmlFor={id}>{label}</PlainLabel>
    <StyledTextbox id={id} disabled={disabled} {...rest}/>
  </>;
};

export default Textbox;
