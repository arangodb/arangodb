

@RESTSTRUCT{name,api_task_struct,string,required,}
The fully qualified name of the user function

@RESTSTRUCT{id,api_task_struct,string,required,}
A string identifying the task

@RESTSTRUCT{created,api_task_struct,number,required,float}
The timestamp when this task was created

@RESTSTRUCT{type,api_task_struct,string,required,}
What type of task is this [ `periodic`, `timed`]
  - periodic are tasks that repeat periodically
  - timed are tasks that execute once at a specific time

@RESTSTRUCT{period,api_task_struct,number,required,}
this task should run each `period` seconds

@RESTSTRUCT{offset,api_task_struct,number,required,float}
time offset in seconds from the created timestamp

@RESTSTRUCT{command,api_task_struct,string,required,}
the javascript function for this task

@RESTSTRUCT{database,api_task_struct,string,required,}
the database this task belongs to
