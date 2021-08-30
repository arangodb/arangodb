import React, { ChangeEvent, ReactNode } from 'react';
import styled from 'styled-components';
import { uniqueId } from 'lodash';

const StyledCheckbox = styled.input.attrs(() => ({
  type: 'checkbox'
}))`
  &&& {
    width: auto;
  }
`;

type CheckboxProps = {
  id?: string;
  checked?: boolean;
  onChange: (event: ChangeEvent<HTMLInputElement>) => void;
  label: ReactNode;
  inline?: boolean;
  disabled?: boolean;
};

const Checkbox = ({ id, checked, onChange, label, inline, disabled }: CheckboxProps) => {
  if (!id) {
    id = uniqueId('checkbox-');
  }

  if (inline) {
    return <label htmlFor={id} className="pure-checkbox">
      <StyledCheckbox id={id} checked={checked} onChange={onChange} disabled={disabled}/>
      &nbsp;{label}
    </label>;
  }

  return <>
    <label htmlFor={id} className="pure-checkbox">{label}</label>
    <StyledCheckbox id={id} checked={checked} onChange={onChange} disabled={disabled}/>
  </>;
};

export default Checkbox;
