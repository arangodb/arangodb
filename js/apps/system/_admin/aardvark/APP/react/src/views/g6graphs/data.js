export const data = {
  nodes: [
    {
      id: '1',
      label: 'node1',
      x: 100,
      y: 200,
      type: 'rect',
    },
    {
      id: '2',
      label: 'node2',
      x: 200,
      y: 100,
      type: 'rect',
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
  ]
};
