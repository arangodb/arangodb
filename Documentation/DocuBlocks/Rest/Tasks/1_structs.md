@RESTSTRUCT{name,api_task_struct,string,required,}
A user-friendly name for the task.

@RESTSTRUCT{id,api_task_struct,string,required,}
A string identifying the task.

@RESTSTRUCT{created,api_task_struct,number,required,float}
The timestamp when this task was created.

@RESTSTRUCT{type,api_task_struct,string,required,}
What type of task is this [ `periodic`, `timed`]
  - periodic are tasks that repeat periodically
  - timed are tasks that execute once at a specific time

@RESTSTRUCT{period,api_task_struct,number,required,}
This task should run each `period` seconds.

@RESTSTRUCT{offset,api_task_struct,number,required,float}
Time offset in seconds from the `created` timestamp.

@RESTSTRUCT{command,api_task_struct,string,required,}
The JavaScript function for this task.

@RESTSTRUCT{database,api_task_struct,string,required,}
The database this task belongs to.
