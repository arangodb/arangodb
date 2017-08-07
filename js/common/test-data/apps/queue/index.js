'use strict';
const queues = require('@arangodb/foxx/queues');
const router = require('@arangodb/foxx/router')();
module.context.use(router);

router.post((req, res) => {
  console.error(require('@arangodb/users').currentUser());
  const queue = queues.create('test_queue');
  queue.push({
    name: 'test_job',
    mount: '/test'
  }, {});
});

router.delete((req, res) => {
  const queue = queues.get('test_queue');
  for (const job of queue.all()) {
    queue.delete(job);
  }
  queues.delete('test_queue');
});
