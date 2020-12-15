export interface Task {
  execute() : void
}

const sleep = async (ms: number) : Promise<void> => new Promise(resolve => setTimeout(resolve, ms));

class TaskRepeater {
  private todoList : Map<string, Task>

  constructor() {
    this.todoList = new Map();
    // Start execution;
    this.repeatAll();
  }

  public registerTask(name : string, task : Task) {
    this.todoList.set(name, task);
  }

  public stopTask(name: string) {
    this.todoList.delete(name);
  }

  private async repeatAll() : Promise<never> {
    while (true) {
      if (this.todoList.size > 0) {
        for (const [, task] of this.todoList) {
          task.execute();
        }
      }
      await sleep(10000);
    }
  }
}

export const taskRepeater = new TaskRepeater();
