import { FormProps } from "../../../utils/constants";
import { BytesAccumConsolidationPolicy, TierConsolidationPolicy, ViewProperties } from "../constants";
import React, { ChangeEvent } from "react";
import { get } from "lodash";
import Select from "../../../components/pure-css/form/Select";
import Textbox from "../../../components/pure-css/form/Textbox";
import { getNumericFieldSetter } from "../../../utils/helpers";

const BytesAccumConsolidationPolicyForm = ({
                                             formState,
                                             dispatch,
                                             disabled
                                           }: FormProps<BytesAccumConsolidationPolicy>) => {
  const threshold = get(formState, ['consolidationPolicy', 'threshold'], '');

  return <tr className="tableRow" id="row_change-view-threshold">
    <th className="collectionTh">
      Threshold:
    </th>
    <th className="collectionTh">
      <Textbox type={'number'} value={threshold} disabled={disabled}
               onChange={getNumericFieldSetter('consolidationPolicy.threshold', dispatch)}/>
    </th>
  </tr>;
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

  return <>
    <tr className="tableRow" id="row_change-view-segmentsMin">
      <th className="collectionTh">
        Segments Min:
      </th>
      <th className="collectionTh">
        <Textbox type={'number'} value={segmentsMin} disabled={disabled}
                 onChange={getNumericFieldSetter('consolidationPolicy.segmentsMin', dispatch)}/>
      </th>
    </tr>

    <tr className="tableRow" id="row_change-view-segmentsMax">
      <th className="collectionTh">
        Segments Max:
      </th>
      <th className="collectionTh">
        <Textbox type={'number'} value={segmentsMax} disabled={disabled}
                 onChange={getNumericFieldSetter('consolidationPolicy.segmentsMax', dispatch)}/>
      </th>
    </tr>
    <tr className="tableRow" id="row_change-view-segmentsBytesMax">
      <th className="collectionTh">
        Segments Bytes Max:
      </th>
      <th className="collectionTh">
        <Textbox type={'number'} value={segmentsBytesMax} disabled={disabled}
                 onChange={getNumericFieldSetter('consolidationPolicy.segmentsBytesMax', dispatch)}/>
      </th>
    </tr>

    <tr className="tableRow" id="row_change-view-segmentsBytesFloor">
      <th className="collectionTh">
        Segments Bytes Floor:
      </th>
      <th className="collectionTh">
        <Textbox type={'number'} value={segmentsBytesFloor} disabled={disabled}
                 onChange={getNumericFieldSetter('consolidationPolicy.segmentsBytesFloor', dispatch)}/>
      </th>
    </tr>
  </>;
};

const ConsolidationPolicyForm = ({ formState, dispatch, disabled }: FormProps<ViewProperties>) => {
  const updateConsolidationPolicyType = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'consolidationPolicy.type',
        value: event.target.value
      }
    });
  };

  const policyType = get(formState, ['consolidationPolicy', 'type'], 'tier');

  return <table>
    <tbody>
    <tr className="tableRow" id="row_change-view-policyType">
      <th className="collectionTh">
        Policy Type:
      </th>
      <th className="collectionTh">
        <Select disabled={disabled} value={policyType} onChange={updateConsolidationPolicyType}>
          <option key={'tier'} value={'tier'}>Tier</option>
          <option key={'bytes_accum'} value={'bytes_accum'}>Bytes Accum [DEPRECATED]</option>
        </Select>
      </th>
    </tr>

    {
      policyType === 'tier'
        ? <TierConsolidationPolicyForm formState={formState as TierConsolidationPolicy} dispatch={dispatch}
                                       disabled={disabled}/>
        : <BytesAccumConsolidationPolicyForm formState={formState as BytesAccumConsolidationPolicy}
                                             dispatch={dispatch} disabled={disabled}/>
    }
    </tbody>
  </table>;
};

export default ConsolidationPolicyForm;
