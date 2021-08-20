import { FormProps, GeoOptions } from "../../constants";
import React, { ChangeEvent } from "react";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { get } from "lodash";

const GeoOptionsInput = ({ formState, dispatch, disabled }: FormProps) => {
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

  const geoOptions = get(formState, 'properties.options', {}) as GeoOptions;

  return <Fieldset legend={'Options'}>
    <Grid>
      <Cell size={'1-3'}>
        <Textbox label={'Max S2 Cells'} type={'number'} placeholder={disabled ? '' : '20'}
                 onChange={updateMaxCells} value={geoOptions.maxCells || ''} disabled={disabled}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Least Precise S2 Level'} type={'number'} placeholder={disabled ? '' : '4'}
                 onChange={updateMinLevel} value={geoOptions.minLevel || ''} disabled={disabled}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Most Precise S2 Level'} type={'number'} placeholder={disabled ? '' : '23'}
                 onChange={updateMaxLevel} value={geoOptions.maxLevel || ''} disabled={disabled}/>
      </Cell>
    </Grid>
  </Fieldset>;
};

export default GeoOptionsInput;
