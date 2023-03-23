@RESTSTRUCT{error,admin_server_jwt,boolean,required,}
boolean flag to indicate whether an error occurred (*false* in this case)

@RESTSTRUCT{code,admin_server_jwt,integer,required,int64}
the HTTP status code - 200 in this case

@RESTSTRUCT{result,admin_server_jwt,object,required,jwt_secret_struct}
The result object.

@RESTSTRUCT{active,jwt_secret_struct,object,required,}
An object with the SHA-256 hash of the active secret.

@RESTSTRUCT{passive,jwt_secret_struct,array,required,object}
An array of objects with the SHA-256 hashes of the passive secrets.
Can be empty.
