import React, { ReactNode } from 'react';
import styled from 'styled-components';
import { uniqueId } from 'lodash';
import PlainLabel from "./PlainLabel";

const StyledSelect = styled.select`
  &&& {
    width: auto;
  }
`;

type SelectProps = {
  id?: string;
  label: ReactNode;
  children: ReactNode;
  disabled?: boolean;
  inline?: boolean;
  [key: string]: any;
};

const Select = ({ id, label, children, disabled, inline, ...rest }: SelectProps) => {
  if (!id) {
    id = uniqueId('textbox-');
  }

  if (inline) {
    return <PlainLabel htmlFor={id} style={{
      display: 'inline-block'
    }}>
      {label}:&nbsp;
      <StyledSelect id={id} disabled={disabled} {...rest}>{children}</StyledSelect>
    </PlainLabel>;
  }

  return <>
    <PlainLabel htmlFor={id}>{label}</PlainLabel>
    <StyledSelect id={id} disabled={disabled} {...rest}>{children}</StyledSelect>
  </>;
};

export default Select;
