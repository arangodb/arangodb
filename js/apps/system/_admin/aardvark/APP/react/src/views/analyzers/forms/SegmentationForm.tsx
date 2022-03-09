import React, { ChangeEvent, Dispatch } from "react";
import { CaseProperty, SegmentationState } from "../constants";
import { DispatchArgs, FormProps } from '../../../utils/constants';
import CaseInput from "./inputs/CaseInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Select from "../../../components/pure-css/form/Select";

const SegmentationForm = ({ formState, dispatch, disabled }: FormProps<SegmentationState>) => {
  const updateBreak = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.break',
        value: event.target.value
      }
    });
  };

  return <Grid>
    <Cell size={'1-8'}>
      <Select label={'Break'} value={formState.properties.break || 'alpha'} onChange={updateBreak}
              disabled={disabled}>
        <option value={'all'}>All</option>
        <option value={'alpha'}>Alpha</option>
        <option value={'graphic'}>Graphic</option>
      </Select>
    </Cell>

    <Cell size={'1-8'}>
      <CaseInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<CaseProperty>>}
                 disabled={disabled} defaultValue={'lower'}/>
    </Cell>
  </Grid>;
};

export default SegmentationForm;
