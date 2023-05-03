import React, { ChangeEvent, Dispatch } from "react";
import { GeoS2State, GeoOptionsProperty } from "../constants";
import { DispatchArgs, FormProps } from "../../../utils/constants";
import GeoOptionsInput from "./inputs/GeoOptionsInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Select from "../../../components/pure-css/form/Select";

const GeoS2Form = ({
  formState,
  dispatch,
  disabled
}: FormProps<GeoS2State>) => {
  const updateType = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: "setField",
      field: {
        path: "properties.type",
        value: event.target.value
      }
    });
  };
  const updateFormat = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: "setField",
      field: {
        path: "properties.format",
        value: event.target.value
      }
    });
  };

  return (
    <Grid>
      <Cell size={"1-1"}>
        <Select
          label={"Type"}
          value={formState.properties.type || "shape"}
          onChange={updateType}
          disabled={disabled}
        >
          <option value={"shape"}>Shape</option>
          <option value={"centroid"}>Centroid</option>
          <option value={"point"}>Point</option>
        </Select>
      </Cell>
      <Cell size={"1-1"}>
        <Select
          label={"Format"}
          value={formState.properties.format || "latLngDouble"}
          onChange={updateFormat}
          disabled={disabled}
        >
          <option value={"latLngDouble"}>latLngDouble</option>
          <option value={"latLngInt"}>latLngInt</option>
          <option value={"s2Point"}>s2Point</option>
        </Select>
      </Cell>
      <Cell size={"1"}>
        <GeoOptionsInput
          formState={formState}
          dispatch={dispatch as Dispatch<DispatchArgs<GeoOptionsProperty>>}
          disabled={disabled}
        />
      </Cell>
    </Grid>
  );
};

export default GeoS2Form;
