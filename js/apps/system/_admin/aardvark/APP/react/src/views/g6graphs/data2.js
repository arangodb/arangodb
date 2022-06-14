export const data2 = {
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
    }
  ],
  edges: [
    {
      source: '1',
      target: '2',
      data: {
        type: 'name1',
        amount: '100,000,000,00 元',
        date: '2019-08-03'
      }
    },
    {
      source: '1',
      target: '3',
      data: {
        type: 'name2',
        amount: '100,000,000,00 元',
        date: '2019-08-03'
      }
    },
    {
      source: '2',
      target: '4',
      data: {
        type: 'name1',
        amount: '100,000,000,00 元',
        date: '2019-08-03'
      }
    },
    {
      source: '1',
      target: '4',
      data: {
        type: 'name2',
        amount: '100,000,000,00 元',
        date: '2019-08-03'
      }
    },
    {
      source: '3',
      target: '4',
      data: {
        type: 'name3',
        amount: '100,000,000,00 元',
        date: '2019-08-03'
      }
    }
  ]
};
