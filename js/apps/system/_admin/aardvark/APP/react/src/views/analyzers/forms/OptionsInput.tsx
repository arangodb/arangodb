import { FormProps, GeoOptionsState } from "../constants";
import React, { ChangeEvent } from "react";
import { get } from "lodash";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Fieldset from "../../../components/pure-css/form/Fieldset";
import Textbox from "../../../components/pure-css/form/Textbox";

const OptionsInput = ({ state, dispatch }: FormProps) => {
  const updateMaxCells = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.options.maxCells',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateMinLevel = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.options.minLevel',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateMaxLevel = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.options.maxLevel',
        value: parseInt(event.target.value)
      }
    });
  };

  const options = (state.formState as GeoOptionsState).properties.options;

  return <Fieldset legend={'Options'}>
    <Grid>
      <Cell size={'1-3'}>
        <Textbox label={'Max S2 Cells'} type={'number'} placeholder="20" onChange={updateMaxCells}
                 value={get(options, 'maxCells', '')}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Least Precise S2 Level'} type={'number'} placeholder="4" onChange={updateMinLevel}
                 value={get(options, 'minLevel', '')}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Most Precise S2 Level'} type={'number'} placeholder="23" onChange={updateMaxLevel}
                 value={get(options, 'maxLevel', '')}/>
      </Cell>
    </Grid>
  </Fieldset>;
};

export default OptionsInput;
