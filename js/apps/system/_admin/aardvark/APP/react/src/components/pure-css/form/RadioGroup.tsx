import React, { ChangeEvent, ReactNode } from 'react';
import styled from 'styled-components';
import Fieldset from "./Fieldset";
import { uniqueId } from 'lodash';
import { Cell, CellSize, Grid } from "../grid";

const StyledRadioButton = styled.input.attrs(() => ({
  type: 'radio'
}))`
  &&& {
    width: auto;
    margin-bottom: 10px;
  }
`;

type RadioButtonProps = {
  checked?: boolean;
  onChange: (event: ChangeEvent<HTMLInputElement>) => void;
  value: string;
  label: ReactNode;
  id?: string;
  size: CellSize;
  disabled?: boolean;
};

const RadioButton = ({ id, checked, onChange, value, label, size, disabled }: RadioButtonProps) => {
  if (!id) {
    id = uniqueId(`radio-${value}-`);
  }

  return <Cell size={size}>
    <label htmlFor={id} className="pure-radio">
      <StyledRadioButton id={id} value={value} onChange={onChange} checked={checked}
                         disabled={disabled}/>
      &nbsp;{label}
    </label>
  </Cell>;
};

type RadioGroupProps = {
  legend: string;
  onChange: (event: ChangeEvent<HTMLInputElement>) => void;
  items: {
    label: ReactNode;
    value: string;
    id?: string;
  }[];
  disabled?: boolean;
  checked?: string;
};

const RadioGroup = ({ legend, onChange, items, checked, disabled }: RadioGroupProps) => {
  const size = `${24 / items.length}-24` as CellSize;

  return <Fieldset legend={legend}>
    <Grid>
      {
        items.map(item =>
          <RadioButton onChange={onChange} value={item.value} label={item.label} id={item.id}
                       checked={checked === item.value} size={size} disabled={disabled}
                       key={`${item.value}`}/>)
      }
    </Grid>
  </Fieldset>;
};

export default RadioGroup;
