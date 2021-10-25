import { FormProps } from "../../../utils/constants";
import { BytesAccumConsolidationPolicy, TierConsolidationPolicy, ViewProperties } from "../constants";
import React from "react";
import { get } from "lodash";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";
import { getNumericFieldSetter, getNumericFieldValue } from "../../../utils/helpers";

const BytesAccumConsolidationPolicyForm = ({
                                             formState,
                                             dispatch,
                                             disabled
                                           }: FormProps<BytesAccumConsolidationPolicy>) => {
  const threshold = get(formState, ['consolidationPolicy', 'threshold'], '');

  return <Grid>
    <Cell size={'1-2'}>
      <Textbox type={'number'} label={'Threshold'} value={threshold} disabled={disabled}
               onChange={getNumericFieldSetter('consolidationPolicy.threshold', dispatch)}/>
    </Cell>
  </Grid>;
};

const TierConsolidationPolicyForm = ({
                                       formState,
                                       dispatch,
                                       disabled
                                     }: FormProps<TierConsolidationPolicy>) => {
  const segmentsMin = get(formState, ['consolidationPolicy', 'segmentsMin'], '');
  const segmentsMax = get(formState, ['consolidationPolicy', 'segmentsMax'], '');
  const segmentsBytesMax = get(formState, ['consolidationPolicy', 'segmentsBytesMax'], '');
  const segmentsBytesFloor = get(formState, ['consolidationPolicy', 'segmentsBytesFloor'], '');

  return <Grid>
    <Cell size={'1-4'}>
      <Textbox type={'number'} label={'Segments Min'} value={segmentsMin} disabled={disabled}
               onChange={getNumericFieldSetter('consolidationPolicy.segmentsMin', dispatch)}/>
    </Cell>
    <Cell size={'1-4'}>
      <Textbox type={'number'} label={'Segments Max'} value={segmentsMax} disabled={disabled}
               onChange={getNumericFieldSetter('consolidationPolicy.segmentsMax', dispatch)}/>
    </Cell>
    <Cell size={'1-4'}>
      <Textbox type={'number'} label={'Segments Bytes Max'} value={segmentsBytesMax} disabled={disabled}
               onChange={getNumericFieldSetter('consolidationPolicy.segmentsBytesMax', dispatch)}/>
    </Cell>
    <Cell size={'1-4'}>
      <Textbox type={'number'} label={'Segments Bytes Floor'} value={segmentsBytesFloor} disabled={disabled}
               onChange={getNumericFieldSetter('consolidationPolicy.segmentsBytesFloor', dispatch)}/>
    </Cell>
  </Grid>;
};

const ConsolidationPolicyForm = ({ formState, dispatch, disabled }: FormProps<ViewProperties>) => {
  const policyType = get(formState, ['consolidationPolicy', 'type'], 'tier');

  return <Grid>
    <Cell size={'1-4'}>
      <Textbox type={'number'} label={'Consolidation Interval (msec)'} disabled={disabled}
               value={getNumericFieldValue(formState.consolidationIntervalMsec)}
               onChange={getNumericFieldSetter('consolidationIntervalMsec', dispatch)}/>
    </Cell>
    <Cell size={'1-4'}>
      <Textbox label={'Consolidation Policy Type'} type={'text'} value={policyType} disabled={true}/>
    </Cell>
    <Cell size={'1-2'}/>
    <Cell size={'1'}>
      {
        policyType === 'tier'
          ? <TierConsolidationPolicyForm formState={formState as TierConsolidationPolicy} dispatch={dispatch}
                                         disabled={disabled}/>
          : <BytesAccumConsolidationPolicyForm formState={formState as BytesAccumConsolidationPolicy}
                                               dispatch={dispatch} disabled={disabled}/>
      }
    </Cell>
  </Grid>;
};

export default ConsolidationPolicyForm;
