import React from 'react';
import Graphin, { Utils, GraphinContext } from '@antv/graphin';
import { message, Card } from 'antd';
import { ContextMenu, FishEye } from '@antv/graphin-components';
import {
  TagFilled,
  DeleteFilled,
  ExpandAltOutlined
} from '@ant-design/icons';

const G6Graph = () => {
  const [state, setState] = React.useState({
    data2: null,
  });
  React.useEffect(() => {
    // eslint-disable-next-line no-undef
    fetch('https://gw.alipayobjects.com/os/antvdemo/assets/data/algorithm-category.json')
    .then(res => res.json())
    .then(res => {
      console.log('data', res);
      setState({
        data2: res,
      });
    });
  }, []);
  const { data2 } = state;

  return (
    <div>
          <Card
            title="G6Graph"
          >
              {data2 && (
                <Graphin
                  data={data2}
                >
                </Graphin>
              )}
          </Card>
    </div>
  );
}

export default G6Graph;
