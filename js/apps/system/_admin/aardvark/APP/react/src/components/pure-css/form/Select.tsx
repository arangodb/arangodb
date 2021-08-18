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
  [key: string]: any;
};

const Select = ({ id, label, children, ...rest }: SelectProps) => {
  if (!id) {
    id = uniqueId('textbox-');
  }

  return <>
    <PlainLabel htmlFor={id}>{label}</PlainLabel>
    <StyledSelect id={id} {...rest}>{children}</StyledSelect>
  </>;
};

export default Select;
