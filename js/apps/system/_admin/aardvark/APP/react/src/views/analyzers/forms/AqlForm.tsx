import React, { ChangeEvent } from "react";
import { AqlState } from "../constants";
import { FormProps } from '../../../utils/constants';
import Checkbox from "../../../components/pure-css/form/Checkbox";
import Textbox from "../../../components/pure-css/form/Textbox";
import Textarea from "../../../components/pure-css/form/Textarea";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Select from "../../../components/pure-css/form/Select";
import { getNumericFieldSetter, getNumericFieldValue } from "../../../utils/helpers";

const AqlForm = ({ formState, dispatch, disabled }: FormProps<AqlState>) => {
  const updateQueryString = (event: ChangeEvent<HTMLTextAreaElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.queryString',
        value: event.target.value
      }
    });
  };

  const updateReturnType = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.returnType',
        value: event.target.value
      }
    });
  };

  const updateCollapsePositions = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.collapsePositions',
        value: event.target.checked
      }
    });
  };

  const updateKeepNull = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.keepNull',
        value: event.target.checked
      }
    });
  };

  return <Grid>
    <Cell size={'1-2'}>
      <Textarea label={'Query String'} value={formState.properties.queryString || ''} disabled={disabled}
                onChange={updateQueryString} rows={4} required={true}/>
    </Cell>

    <Cell size={'1-2'}>
      <Grid>
        <Cell size={'1'}>
          <Textbox label={'Batch Size'} type={'number'} min={1} required={true}
                   value={getNumericFieldValue(formState.properties.batchSize)} disabled={disabled}
                   onChange={getNumericFieldSetter('properties.batchSize', dispatch)}/>
        </Cell>
        <Cell size={'1'}>
          <Textbox label={'Memory Limit'} type={'number'} min={1} max={33554432} required={true}
                   value={getNumericFieldValue(formState.properties.memoryLimit)}
                   disabled={disabled} onChange={getNumericFieldSetter('properties.memoryLimit', dispatch)}/>
        </Cell>
      </Grid>
    </Cell>

    <Cell size={'1-2'}>
      <Grid>
        <Cell size={'1-3'}>
          <Checkbox checked={formState.properties.collapsePositions || false} disabled={disabled}
                    onChange={updateCollapsePositions} label={'Collapse Positions'}/>
        </Cell>
        <Cell size={'1-3'}>
          <Checkbox checked={formState.properties.keepNull || false} onChange={updateKeepNull}
                    disabled={disabled} label={'Keep Null'}/>
        </Cell>
        <Cell size={'1-3'}>
          <Select label={'Return Type'} value={formState.properties.returnType || 'string'}
                  onChange={updateReturnType} required={true} disabled={disabled}>
            <option value={'string'}>String</option>
            <option value={'number'}>Number</option>
            <option value={'bool'}>Boolean</option>
          </Select>
        </Cell>
      </Grid>
    </Cell>
  </Grid>;
};

export default AqlForm;
