import React, { ChangeEvent, ReactNode, useEffect, useState } from 'react';
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
  const [thisId, setThisId] = useState(id || uniqueId('checkbox-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);

  if (inline) {
    return <label htmlFor={thisId} className="pure-checkbox">
      <StyledCheckbox id={thisId} checked={checked} onChange={onChange} disabled={disabled}/>
      &nbsp;{label}
    </label>;
  }

  return <>
    <label htmlFor={thisId} className="pure-checkbox">{label}</label>
    <StyledCheckbox id={thisId} checked={checked} onChange={onChange} disabled={disabled}/>
  </>;
};

export default Checkbox;
