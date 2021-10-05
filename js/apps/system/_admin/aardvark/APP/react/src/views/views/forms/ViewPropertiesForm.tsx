import { FormProps } from "../../../utils/constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import PrimarySortInput from "./inputs/PrimarySortInput";
import React from "react";
import Select from "../../../components/pure-css/form/Select";
import { ViewProperties } from "../constants";
import StoredValuesInput from "./inputs/StoredValuesInput";
import Textbox from "../../../components/pure-css/form/Textbox";

const ViewPropertiesForm = ({ formState, dispatch, disabled }: FormProps<ViewProperties>) => {
  return <Grid>
    <Cell size={'1'} style={{
      marginBottom: 20,
      borderBottom: '1px solid #e5e5e5'
    }}>
      <PrimarySortInput formState={formState} dispatch={dispatch} disabled={disabled}/>
    </Cell>
    <Cell size={'1'} style={{
      marginBottom: 20,
      borderBottom: '1px solid #e5e5e5'
    }}>
      <StoredValuesInput formState={formState} dispatch={dispatch} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Select label={'Primary Sort Compression'} disabled={disabled}
              value={formState.primarySortCompression || 'lz4'}>
        <option key={'lz4'} value={'lz4'}>LZ4</option>
        <option key={'none'} value={'none'}>None</option>
      </Select>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Cleanup Interval Step'} disabled={disabled}
               value={formState.cleanupIntervalStep}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Commit Interval (msec)'} disabled={disabled}
               value={formState.commitIntervalMsec}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Write Buffer Idle'} disabled={disabled}
               value={formState.writebufferIdle}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Write Buffer Active'} disabled={disabled}
               value={formState.writebufferActive}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Write Buffer Size Max'} disabled={disabled}
               value={formState.writebufferSizeMax}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox type={'number'} label={'Consolidation Interval (msec)'} disabled={disabled}
               value={formState.consolidationIntervalMsec}/>
    </Cell>

  </Grid>;
};

export default ViewPropertiesForm;
