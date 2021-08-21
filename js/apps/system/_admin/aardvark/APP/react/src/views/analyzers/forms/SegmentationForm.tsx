import React, { ChangeEvent } from "react";
import { FormProps, SegmentationState } from "../constants";
import CaseInput from "./inputs/CaseInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import RadioGroup from "../../../components/pure-css/form/RadioGroup";

const SegmentationForm = ({ formState, dispatch, disabled }: FormProps) => {
  const updateBreak = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.break',
        value: event.target.value
      }
    });
  };

  const breakProperty = (formState as SegmentationState).properties.break;

  return <Grid>
    <Cell size={'1-2'}>
      <RadioGroup legend={'Break'} onChange={updateBreak} name={'break'} items={[
        {
          label: 'All',
          value: 'all'
        },
        {
          label: 'Alpha',
          value: 'alpha'
        },
        {
          label: 'Graphic',
          value: 'graphic'
        }
      ]} checked={breakProperty || 'alpha'} disabled={disabled}/>
    </Cell>

    <Cell size={'1-2'}>
      <CaseInput formState={formState} dispatch={dispatch} disabled={disabled} defaultValue={'lower'}/>
    </Cell>
  </Grid>;
};

export default SegmentationForm;
