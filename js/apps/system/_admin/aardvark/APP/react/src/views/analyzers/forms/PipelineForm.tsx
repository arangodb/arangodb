import React, { Dispatch } from "react";
import { AnalyzerTypeState, PipelineState, PipelineStates, typeNameMap } from "../constants";
import { DispatchArgs, FormProps } from "../../../utils/constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { getForm } from "../helpers";
import { omit } from "lodash";
import TypeInput from "./inputs/TypeInput";
import { getPath } from "../../../utils/helpers";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import { IconButton } from "../../../components/arango/buttons";

const restrictedTypeNameMap = omit(typeNameMap, 'geojson', 'geopoint', 'pipeline', 'identity');

const PipelineForm = ({ formState, dispatch, disabled }: FormProps<PipelineStates>) => {
  const items = formState.properties.pipeline;

  const removeItem = (index: number) => {
    const tempItems = items.slice();

    tempItems.splice(index, 1);
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.pipeline',
        value: tempItems
      }
    });
  };

  const getRemover = (index: number) => () => {
    removeItem(index);
  };

  const moveUp = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const prevItem = tempItems[index - 1];

    tempItems.splice(index - 1, 2, item, prevItem);
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.pipeline',
        value: tempItems
      }
    });
  };

  const moveDown = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const nextItem = tempItems[index + 1];

    tempItems.splice(index, 2, nextItem, item);
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.pipeline',
        value: tempItems
      }
    });
  };

  const getShifter = (direction: 'up' | 'down', index: number) => {
    switch (direction) {
      case 'up':
        return () => {
          moveUp(index);
        };
      case 'down':
        return () => {
          moveDown(index);
        };
    }
  };

  const addAnalyzer = () => {
    const analyzer = {
      type: 'delimiter',
      properties: {
        delimiter: ''
      }
    };

    dispatch({
      type: 'setField',
      field: {
        path: `properties.pipeline[${items.length}]`,
        value: analyzer
      }
    });
  };

  const getWrappedDispatch = (index: number): Dispatch<DispatchArgs<PipelineState>> => (action: DispatchArgs<PipelineState>) => {
    action.basePath = getPath(`properties.pipeline[${index}]`, action.basePath);
    (dispatch as unknown as Dispatch<DispatchArgs<PipelineState>>)(action);
  };

  return <Grid>
    {
      disabled
        ? null
        : <Cell size={'1'}>
          <IconButton icon={'plus'} type={'warning'} onClick={addAnalyzer}>Add Analyzer</IconButton>
        </Cell>
    }
    <Cell size={'1'}>
      <ArangoTable style={{
        marginTop: 5,
        marginBottom: 5
      }}>
        <tbody>
        {
          items.map((item, idx) => {
            const isFirst = idx === 0;
            const isLast = idx === items.length - 1;
            const itemDispatch = getWrappedDispatch(idx);

            return <tr key={idx} style={{ borderBottom: '1px  solid #DDD' }}>
              <ArangoTD seq={0}>{idx + 1}.</ArangoTD>
              <ArangoTD seq={1}>
                <Grid>
                  <Cell size={'3-4'}>
                    <TypeInput formState={item}
                               dispatch={itemDispatch as Dispatch<DispatchArgs<AnalyzerTypeState>>}
                               inline={true} key={idx}
                               typeNameMap={restrictedTypeNameMap} disabled={disabled}/>
                  </Cell>
                  {
                    disabled
                      ? null
                      : <Cell size={'1-4'}>
                        <IconButton icon={'trash-o'} type={'danger'} style={{ marginTop: 10 }}
                                    onClick={getRemover(idx)}/>&nbsp;
                        <IconButton icon={'arrow-up'} type={'warning'} style={{ marginTop: 10 }}
                                    onClick={getShifter('up', idx)} disabled={isFirst}/>&nbsp;
                        <IconButton icon={'arrow-down'} type={'warning'} style={{ marginTop: 10 }}
                                    onClick={getShifter('down', idx)} disabled={isLast}/>
                      </Cell>
                  }
                  <Cell size={'1'}>
                    {
                      getForm({
                        formState: item,
                        dispatch: itemDispatch as Dispatch<DispatchArgs<AnalyzerTypeState>>,
                        disabled
                      })
                    }
                  </Cell>
                </Grid>
              </ArangoTD>
            </tr>;
          })
        }
        </tbody>
      </ArangoTable>
    </Cell>
    {
      disabled || items.length === 0
        ? null
        : <Cell size={'1'}>
          <IconButton icon={'plus'} type={'warning'} onClick={addAnalyzer}>Add Analyzer</IconButton>
        </Cell>
    }
  </Grid>;
};

export default PipelineForm;
