import { isEqual } from 'lodash';
import React, { useState } from 'react';
import useSWR from 'swr';
import { ArangoTable, ArangoTD, ArangoTH } from '../../components/arango/table';
import { Cell, Grid } from '../../components/pure-css/grid';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { usePermissions } from '../../utils/helpers';

const ViewsReactView = () => {
  const { data } = useSWR('/view', (path) => getApiRouteForCurrentDB().get(path));
  const { data: permData } = usePermissions();

  const [views, setViews] = useState([]);

  if (data && permData) {
    // const permission = permData.body.result;

    if (!isEqual(data.body.result, views)) {
      setViews(data.body.result);
    }

    return <div className={'innerContent'} id={'viewsContent'} style={{ paddingTop: 0 }}>
      <Grid>
        <Cell size={'1'}>
          <div className={'sectionHeader'}>
            <div className={'title'}>[TODO: Add View]</div>
          </div>
          <ArangoTable>
            <thead>
            <tr>
              <ArangoTH seq={0}>Name</ArangoTH>
              <ArangoTH seq={1}>Type</ArangoTH>
            </tr>
            </thead>
            <tbody>
            {
              views.length
                ? views.map(view => (
                  <tr key={view.name}>
                    <ArangoTD seq={0}>{view.name}</ArangoTD>
                    <ArangoTD seq={1}>{view.type}</ArangoTD>
                  </tr>
                ))
                : <tr>
                  <ArangoTD seq={0} colSpan={3}>No views found.</ArangoTD>
                </tr>
            }
            </tbody>
          </ArangoTable>
        </Cell>
      </Grid>
    </div>;
  }

  return <h1>Views</h1>;
};

window.ViewsReactView = ViewsReactView;
