export const data = {
  nodes: [
    {
      id: '1',
      x: 100,
      y: 100,
      type: 'circle',
      label: 'node1',
      population: '1,800,000',
      labelCfg: {
        position: 'bottom',
        offset: 10,
        style: {
          // ... The style of the label
        },
      },
    },
    {
      id: '2',
      label: 'node2',
      x: 200,
      y: 100,
      type: 'rect',
    },
    {
      id: '3',
      label: 'node3',
      x: 330,
      y: 100,
      type: 'ellipse',
    },
    {
      id: '4',
      label: 'node4',
      x: 460,
      y: 100,
      type: 'diamond',
    },
    {
      id: '5',
      label: 'node5',
      x: 560,
      y: 100,
      //size: 80,
      type: 'triangle',
    },
    {
      id: '6',
      label: 'node6',
      x: 660,
      y: 100,
      //size: [60, 30],
      type: 'star',
    },
    {
      id: '7',
      label: 'node7',
      x: 760,
      y: 100,
      size: 50,
      type: 'image',
      img: 'https://gw.alipayobjects.com/zos/rmsportal/XuVpGqBFxXplzvLjJBZB.svg',
    },
    {
      id: '8',
      label: 'node8',
      x: 900,
      y: 100,
      size: [300, 50],
      description: 'Description of modelRect',
      type: 'modelRect',
    },
  ],
  edges: [
    {
      source: '1',
      target: '2',
      data: {
        type: 'name1',
        distance: '100,000',
        date: '2021-08-03'
      }
    },
    {
      source: '1',
      target: '3',
      data: {
        type: 'name2',
        distance: '200',
        date: '2021-10-07'
      }
    },
    {
      source: '2',
      target: '5',
      data: {
        type: 'name1',
        distance: '8,000',
        date: '2021-11-15'
      }
    },
    {
      source: '5',
      target: '6',
      data: {
        type: 'name2',
        distance: '950,900',
        date: '2022-01-03'
      }
    },
    {
      source: '3',
      target: '4',
      data: {
        type: 'name3',
        distance: '333,670',
        date: '2022-01-12'
      }
    },
    {
      source: '4',
      target: '7',
      data: {
        type: 'name2',
        distance: '346,890',
        date: '2021-10-06'
      }
    },
    {
      source: '1',
      target: '8',
      data: {
        type: 'name2',
        distance: '800,600',
        date: '2020-06-20'
      }
    },
    {
      source: '1',
      target: '9',
      data: {
        type: 'name3',
        distance: '250,000,80',
        date: '2020-04-24'
      }
    }
  ]
};

/*
export const data = {
  nodes: [
    {
      id: '1',
      label: 'node1'
    },
    {
      id: '2',
      label: 'node2'
    },
    {
      id: '3',
      label: 'node3'
    },
    {
      id: '4',
      label: 'node4'
    },
    {
      id: '5',
      label: 'node5'
    },
    {
      id: '6',
      label: 'node6'
    },
    {
      id: '7',
      label: 'node7'
    },
    {
      id: '8',
      label: 'node8'
    },
    {
      id: '9',
      label: 'node9'
    }
  ],
  edges: [
    {
      source: '1',
      target: '2',
      data: {
        type: 'name1',
        distance: '100,000,000,00 ',
        date: '2019-08-03'
      }
    },
    {
      source: '1',
      target: '3',
      data: {
        type: 'name2',
        distance: '100,000,000,00 ',
        date: '2019-08-03'
      }
    },
    {
      source: '2',
      target: '5',
      data: {
        type: 'name1',
        distance: '100,000,000,00 ',
        date: '2019-08-03'
      }
    },
    {
      source: '5',
      target: '6',
      data: {
        type: 'name2',
        distance: '100,000,000,00 ',
        date: '2019-08-03'
      }
    },
    {
      source: '3',
      target: '4',
      data: {
        type: 'name3',
        distance: '100,000,000,00 ',
        date: '2019-08-03'
      }
    },
    {
      source: '4',
      target: '7',
      data: {
        type: 'name2',
        distance: '100,000,000,00 ',
        date: '2019-08-03'
      }
    },
    {
      source: '1',
      target: '8',
      data: {
        type: 'name2',
        distance: '100,000,000,00 ',
        date: '2019-08-03'
      }
    },
    {
      source: '1',
      target: '9',
      data: {
        type: 'name3',
        distance: '100,000,000,00 ',
        date: '2019-08-03'
      }
    }
  ]
};
*/
