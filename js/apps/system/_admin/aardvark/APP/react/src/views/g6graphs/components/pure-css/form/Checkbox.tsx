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
  onChange?: (event: ChangeEvent<HTMLInputElement>) => void;
  label?: ReactNode;
  inline?: boolean;
  disabled?: boolean;
  template?: string;
};

const Checkbox = ({ id, checked, onChange, label, inline, disabled, template }: CheckboxProps) => {
  const [thisId, setThisId] = useState(id || uniqueId('checkbox-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);

  if (inline && label) {
    if(template === "graphviewer") {
        return <>
            {label ? <div style={{ 'color': '#ffffff', 'width': '150px' }}>{label}</div> : null}
            <StyledCheckbox id={thisId} checked={checked || false} onChange={onChange} disabled={disabled} style={{ 'marginRight': '8px' }}/>
        </>;
    }
    return <label htmlFor={thisId} className="pure-checkbox" style={{ width: "fit-content" }}>
      <StyledCheckbox id={thisId} checked={checked || false} onChange={onChange} disabled={disabled}/>
      &nbsp;{label}
    </label>;
  }

  return <>
    {label ? <label htmlFor={thisId} className="pure-checkbox">{label}</label> : null}
    <StyledCheckbox id={thisId} checked={checked} onChange={onChange} disabled={disabled}/>
  </>;
};

export default Checkbox;
