import React, { ChangeEvent } from "react";
import { AqlState, FormProps } from "../constants";
import Checkbox from "../../../components/pure-css/form/Checkbox";
import RadioGroup from "../../../components/pure-css/form/RadioGroup";
import Textbox from "../../../components/pure-css/form/Textbox";
import Textarea from "../../../components/pure-css/form/Textarea";
import { Cell, Grid } from "../../../components/pure-css/grid";

const AqlForm = ({ state, dispatch, disabled }: FormProps) => {
  const updateBatchSize = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.batchSize',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateMemoryLimit = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.memoryLimit',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateQueryString = (event: ChangeEvent<HTMLTextAreaElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.queryString',
        value: event.target.value
      }
    });
  };

  const updateReturnType = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.returnType',
        value: event.target.value
      }
    });
  };

  const formState = state.formState as AqlState;

  const toggleCollapsePositions = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.collapsePositions',
        value: !formState.properties.collapsePositions
      }
    });
  };

  const toggleKeepNull = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.keepNull',
        value: !formState.properties.keepNull
      }
    });
  };

  return <Grid>
    <Cell size={'1-3'}>
      <Textarea label={'Query String'} value={formState.properties.queryString} onChange={updateQueryString}
                rows={4} required={true} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Grid>
        <Cell size={'1'}>
          <Textbox label={'Batch Size'} type={'number'} min={1} placeholder="10" required={true}
                   value={formState.properties.batchSize} onChange={updateBatchSize} disabled={disabled}/>
        </Cell>
        <Cell size={'1'}>
          <Textbox label={'Memory Limit'} type={'number'} min={1} max={33554432} placeholder="1048576"
                   required={true} value={formState.properties.memoryLimit} onChange={updateMemoryLimit}
                   disabled={disabled}/>
        </Cell>
      </Grid>
    </Cell>

    <Cell size={'1-3'}>
      <Grid>
        <Cell size={'1-2'}>
          <Checkbox checked={formState.properties.collapsePositions} onChange={toggleCollapsePositions}
                    label={'Collapse Positions'} disabled={disabled}/>
        </Cell>
        <Cell size={'1-2'}>
          <Checkbox checked={formState.properties.keepNull} onChange={toggleKeepNull} disabled={disabled}
                    label={'Keep Null'}/>
        </Cell>
        <Cell size={'1'}>
          <RadioGroup legend={'Return Type'} onChange={updateReturnType} name={'returnType'} items={[
            {
              label: 'String',
              value: 'string'
            },
            {
              label: 'Number',
              value: 'number'
            },
            {
              label: 'Boolean',
              value: 'bool'
            }
          ]} checked={formState.properties.returnType || 'string'} disabled={disabled}/>
        </Cell>
      </Grid>
    </Cell>
  </Grid>;
};

export default AqlForm;
