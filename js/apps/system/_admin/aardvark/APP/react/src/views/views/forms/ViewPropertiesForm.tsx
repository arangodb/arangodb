import { FormProps } from "../../../utils/constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import PrimarySortInput from "./inputs/PrimarySortInput";
import React, { ChangeEvent } from "react";
import Select from "../../../components/pure-css/form/Select";
import { ViewProperties } from "../constants";
import StoredValuesInput from "./inputs/StoredValuesInput";
import Textbox from "../../../components/pure-css/form/Textbox";
import { getNumericFieldSetter } from "../../../utils/helpers";
import ConsolidationPolicyForm from "./ConsolidationPolicyForm";

type ViewPropertiesFormProps = FormProps<ViewProperties> & {
  isEdit?: boolean
};

const ViewPropertiesForm = ({ formState, dispatch, disabled, isEdit = false }: ViewPropertiesFormProps) => {
  const updatePrimarySortCompression = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'primarySortCompression',
        value: event.target.value
      }
    });
  };

  return <Grid>
    <Cell size={'1'} style={{
      marginBottom: 20,
      borderBottom: '1px solid #e5e5e5'
    }}>
      <PrimarySortInput formState={formState} dispatch={dispatch} disabled={disabled || isEdit}/>
    </Cell>

    <Cell size={'1'}>
      <hr/>
    </Cell>

    <Cell size={'1'} style={{
      marginBottom: 20,
      borderBottom: '1px solid #e5e5e5'
    }}>
      <StoredValuesInput formState={formState} dispatch={dispatch} disabled={disabled || isEdit}/>
    </Cell>

    <Cell size={'1'}>
      <hr/>
    </Cell>

    <Cell size={'1-3'}>
      <Select label={'Primary Sort Compression'} disabled={disabled || isEdit}
              onChange={updatePrimarySortCompression} value={formState.primarySortCompression || 'lz4'}>
        <option key={'lz4'} value={'lz4'}>LZ4</option>
        <option key={'none'} value={'none'}>None</option>
      </Select>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Cleanup Interval Step'} value={formState.cleanupIntervalStep || ''}
               onChange={getNumericFieldSetter('cleanupIntervalStep', dispatch)} disabled={disabled}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Commit Interval (msec)'} value={formState.commitIntervalMsec || ''}
               onChange={getNumericFieldSetter('commitIntervalMsec', dispatch)} disabled={disabled}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Write Buffer Idle'} value={formState.writebufferIdle || ''}
               onChange={getNumericFieldSetter('writebufferIdle', dispatch)} disabled={disabled || isEdit}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Write Buffer Active'} value={formState.writebufferActive || ''}
               onChange={getNumericFieldSetter('writebufferActive', dispatch)} disabled={disabled || isEdit}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Write Buffer Size Max'} value={formState.writebufferSizeMax || ''}
               onChange={getNumericFieldSetter('writebufferSizeMax', dispatch)}
               disabled={disabled || isEdit}/>
    </Cell>

    <Cell size={'1'}>
      <hr/>
    </Cell>

    <Cell size={'1'}>
      <ConsolidationPolicyForm formState={formState} dispatch={dispatch} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default ViewPropertiesForm;
