import { FormProps, GeoOptions } from "../../constants";
import React, { ChangeEvent } from "react";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { get } from "lodash";
import { getPath } from "../../helpers";

const GeoOptionsInput = ({ formState, dispatch, disabled, basePath }: FormProps) => {
  const updateMaxCells = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'options.maxCells',
        value: parseInt(event.target.value)
      },
      basePath
    });
  };

  const updateMinLevel = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'options.minLevel',
        value: parseInt(event.target.value)
      },
      basePath
    });
  };

  const updateMaxLevel = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'options.maxLevel',
        value: parseInt(event.target.value)
      },
      basePath
    });
  };

  const geoOptions = get(formState, getPath(basePath, 'options'), {}) as GeoOptions;

  return <Fieldset legend={'Options'}>
    <Grid>
      <Cell size={'1-3'}>
        <Textbox label={'Max S2 Cells'} type={'number'} placeholder="20" onChange={updateMaxCells}
                 value={geoOptions.maxCells} disabled={disabled}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Least Precise S2 Level'} type={'number'} placeholder="4" onChange={updateMinLevel}
                 value={geoOptions.minLevel} disabled={disabled}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Most Precise S2 Level'} type={'number'} placeholder="23" onChange={updateMaxLevel}
                 value={geoOptions.maxLevel} disabled={disabled}/>
      </Cell>
    </Grid>
  </Fieldset>;
};

export default GeoOptionsInput;
