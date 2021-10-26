import { FormState } from "../../constants";
import { FormProps } from "../../../../utils/constants";
import React, { ChangeEvent, useEffect, useState } from "react";
import { find, sortBy } from "lodash";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { validateAndFix } from "../../helpers";
import { IconButton } from "../../../../components/arango/buttons";

type CopyFromInputProps = {
  views: FormState[];
} & Pick<FormProps<FormState>, 'dispatch'>;

const CopyFromInput = ({ views, dispatch }: CopyFromInputProps) => {
  const [sortedViews, setSortedViews] = useState(sortBy(views, 'name'));
  const [selectedView, setSelectedView] = useState(sortedViews[0]);
  const { data } = useSWR(`/view/${selectedView.name}/properties`, (path) => getApiRouteForCurrentDB().get(path));

  const fullView = data ? data.body : selectedView;

  useEffect(() => {
    setSortedViews(sortBy(views, 'name'));
  }, [views]);

  useEffect(() => {
    let tempSelectedView = find(sortedViews, { name: selectedView.name });
    if (!tempSelectedView) {
      setSelectedView(sortedViews[0]);
    }
  }, [selectedView.name, sortedViews]);

  const copyFormState = () => {
    fullView.name = '';
    validateAndFix(fullView);

    dispatch({
      type: 'setFormState',
      formState: fullView as FormState
    });
    dispatch({ type: 'regenRenderKey' });
  };

  const updateSelectedView = (event: ChangeEvent<HTMLSelectElement>) => {
    let tempSelectedView = find(sortedViews, { name: event.target.value });
    if (!tempSelectedView) {
      tempSelectedView = sortedViews[0];
    }

    setSelectedView(tempSelectedView);
  };

  return <Grid>
    <Cell size={'2-3'}>
      <select value={selectedView.name} style={{
        width: 'auto',
        float: 'right'
      }} onChange={updateSelectedView}>
        {
          sortedViews.map((view, idx) =>
            <option key={idx} value={view.name}>{view.name}</option>)
        }
      </select>
    </Cell>
    <Cell size={'1-3'}>
      <IconButton icon={'hand-o-left'} type={'warning'} onClick={copyFormState}>Copy from</IconButton>
    </Cell>
  </Grid>;
};

export default CopyFromInput;
