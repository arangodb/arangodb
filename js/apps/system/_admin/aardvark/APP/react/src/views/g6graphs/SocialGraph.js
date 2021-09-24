/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState } from 'react';
import Graphin from '@antv/graphin';
import { Card } from 'antd';
import { ContextMenu } from '@antv/graphin-components';

const SocialGraph = () => {
  const [data, setData] = useState(null);
  const { Menu} = ContextMenu;
  useEffect(() => {
      arangoFetch(arangoHelper.databaseUrl('/_admin/aardvark/g6graph/social'))
          .then(res => res.json())
          .then(setData)
          .catch(console.error)
          .catch(console.error);
  }, []);

  if (data) {
    return (
      <div className="SocialGraph">
                  <Card
                      title="Social Graph"
                      bodyStyle={{ height: '554px', overflow: 'scroll' }}
                  >
                      <Graphin data={data} layout={{ name: "dagre" }}>
                  <ContextMenu>
                      <Menu.Item>menu item</Menu.Item>
                  </ContextMenu>
              </Graphin>
                  </Card>
      </div>
    );
  }
  return null;
}

export default SocialGraph;
