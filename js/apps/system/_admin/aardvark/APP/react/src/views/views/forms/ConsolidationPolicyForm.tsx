import { FormProps } from "../../../utils/constants";
import { BytesAccumConsolidationPolicy, TierConsolidationPolicy, ViewProperties } from "../constants";
import React, { ChangeEvent } from "react";
import { get } from "lodash";
import Select from "../../../components/pure-css/form/Select";
import Textbox from "../../../components/pure-css/form/Textbox";
import { getNumericFieldSetter } from "../../../utils/helpers";
import ToolTip from "../../../components/arango/tootip";

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
      <Textbox type={'number'} value={threshold} disabled={disabled} min={0.0} max={1.0} step={0.0001}
               onChange={getNumericFieldSetter('consolidationPolicy.threshold', dispatch)}/>
    </th>
    <th className="collectionTh">
      <ToolTip
        title="Consolidation is performed on segments which accumulated size in bytes is less than all segments’ byte size multiplied by the threshold."
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info"></span>
      </ToolTip>
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
        <Textbox type={'number'} value={segmentsMin} disabled={disabled} min={0} step={1}
                 onChange={getNumericFieldSetter('consolidationPolicy.segmentsMin', dispatch)}/>
      </th>
      <th className="collectionTh">
        <ToolTip
          title="The minimum number of segments that will be evaluated as candidates for consolidation."
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info"></span>
        </ToolTip>
      </th>
    </tr>

    <tr className="tableRow" id="row_change-view-segmentsMax">
      <th className="collectionTh">
        Segments Max:
      </th>
      <th className="collectionTh">
        <Textbox type={'number'} value={segmentsMax} disabled={disabled} min={0} step={1}
                 onChange={getNumericFieldSetter('consolidationPolicy.segmentsMax', dispatch)}/>
      </th>
      <th className="collectionTh">
        <ToolTip
          title="The maximum number of segments that will be evaluated as candidates for consolidation."
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info"></span>
        </ToolTip>
      </th>
    </tr>
    <tr className="tableRow" id="row_change-view-segmentsBytesMax">
      <th className="collectionTh">
        Segments Bytes Max:
      </th>
      <th className="collectionTh">
        <Textbox type={'number'} value={segmentsBytesMax} disabled={disabled} min={0} step={1}
                 onChange={getNumericFieldSetter('consolidationPolicy.segmentsBytesMax', dispatch)}/>
      </th>
      <th className="collectionTh">
        <ToolTip
          title="Maximum allowed size of all consolidated segments in bytes."
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info"></span>
        </ToolTip>
      </th>
    </tr>

    <tr className="tableRow" id="row_change-view-segmentsBytesFloor">
      <th className="collectionTh">
        Segments Bytes Floor:
      </th>
      <th className="collectionTh">
        <Textbox type={'number'} value={segmentsBytesFloor} disabled={disabled} min={0} step={1}
                 onChange={getNumericFieldSetter('consolidationPolicy.segmentsBytesFloor', dispatch)}/>
      </th>
      <th className="collectionTh">
        <ToolTip
          title="Defines the value (in bytes) to treat all smaller segments as equal for consolidation selection."
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info"></span>
        </ToolTip>
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
      <th className="collectionTh">
        <ToolTip
          title="Represents the type of policy."
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info"></span>
        </ToolTip>
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
