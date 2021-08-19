import React, { ChangeEvent } from "react";
import { map } from "lodash";
import { FormProps, typeNameMap } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";
import Select from "../../../components/pure-css/form/Select";

const BaseForm = ({ formState, dispatch, disabled }: FormProps) => {
  const updateName = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'name',
        value: event.target.value
      }
    });
  };

  const updateType = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'type',
        value: event.target.value
      }
    });
  };

  return <Grid>
    <Cell size={'1-2'}>
      <Textbox label={'Analyzer Name'} type={'text'} placeholder="Analyzer Name" value={formState.name}
               onChange={updateName} required={true} disabled={disabled}/>
    </Cell>

    <Cell size={'1-2'}>
      <Select label={'Analyzer Type'} value={formState.type} onChange={updateType} required={true}
              disabled={disabled}>
        {
          map(typeNameMap, (value, key) => <option key={key} value={key}>{value}</option>)
        }
      </Select>
    </Cell>
  </Grid>;
};

export default BaseForm;
