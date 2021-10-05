import React, { ReactNode, useEffect, useState } from 'react';
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
  const [thisId, setThisId] = useState(id || uniqueId('textbox-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);

  if (inline) {
    return <PlainLabel htmlFor={thisId} style={{
      display: 'inline-block'
    }}>
      {label}:&nbsp;
      <StyledSelect id={thisId} disabled={disabled} {...rest}>{children}</StyledSelect>
    </PlainLabel>;
  }

  return <>
    <PlainLabel htmlFor={thisId}>{label}</PlainLabel>
    <StyledSelect id={thisId} disabled={disabled} {...rest}>{children}</StyledSelect>
  </>;
};

export default Select;
