import { FormProps } from "../../../utils/constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import PrimarySortInput from "./inputs/PrimarySortInput";
import React from "react";
import Select from "../../../components/pure-css/form/Select";
import { ViewProperties } from "../constants";

const ViewPropertiesForm = ({ formState, dispatch, disabled }: FormProps<ViewProperties>) => {
  return <Grid>
    <Cell size={'1'} style={{
      marginBottom: 20,
      borderBottom: '1px solid #e5e5e5'
    }}>
      <PrimarySortInput formState={formState} dispatch={dispatch} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Select label={'Primary Sort Compression'} disabled={disabled}
              value={formState.primarySortCompression || 'lz4'}>
        <option key={'lz4'} value={'lz4'}>LZ4</option>
        <option key={'none'} value={'none'}>None</option>
      </Select>
    </Cell>

  </Grid>;
};

export default ViewPropertiesForm;
