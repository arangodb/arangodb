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
  name: string;
  value: string;
  label: ReactNode;
  id?: string;
  size: CellSize;
};

const RadioButton = ({ id, checked, onChange, name, value, label, size }: RadioButtonProps) => {
  if (!id) {
    id = uniqueId(`radio-${value}-`);
  }

  return <Cell size={size}>
    <label htmlFor={id} className="pure-radio">
      <StyledRadioButton id={id} name={name} value={value} onChange={onChange} checked={checked}/>
      &nbsp;{label}
    </label>
  </Cell>;
};

type RadioGroupProps = {
  legend: string;
  onChange: (event: ChangeEvent<HTMLInputElement>) => void;
  name: string;
  items: {
    label: ReactNode;
    value: string;
    id?: string;
  }[];
  checked?: string;
};

const RadioGroup = ({ legend, onChange, name, items, checked }: RadioGroupProps) => {
  const size = `${24 / items.length}-24` as CellSize;

  return <Fieldset legend={legend}>
    <Grid>
      {
        items.map(item =>
        <RadioButton onChange={onChange} name={name} value={item.value} label={item.label} id={item.id}
                     checked={checked === item.value} size={size}/>)
      }
    </Grid>
  </Fieldset>;
};

export default RadioGroup;
