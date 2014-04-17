
; import plugin
!addplugin ".\SharedMem"



; all functions writes 2 values into stack
; $0 contains a numerical value. in general 0 means "ok"
: $1 contains an explanation of the result
function .onInit
; tests if the memory segment was already created
; 0: it was not yet created
; 1:  was already created
sharedmem::ExistsSharedMem
pop $0
MessageBox MB_OK "sharedmem::ExistsSharedMem  $0"

; Creates the shared memory segment
; return values:
; 0: all was ok in another case the error code of the windows system

sharedmem::CreateSharedMemory
pop $0
MessageBox MB_OK "sharedmem::CreateSharedMemory  $0"

;writes the given parameter into the segment as an string
; return values: 
                  0: the string was written in another case the 
                     error code of the windows system
; posible errors: 2: mem segment does not exits
;                 5: you have not rights to writte into the mem segment
sharedmem::WriteIntoSharedMem "This is written into shared memory segment"
pop $0
MessageBox MB_OK "sharedmem::WriteIntoSharedMem  $0"

; reads the containt of the mem segment. it is saved into $1
; $0 similar to functions above
sharedmem::ReadIntoSharedMem
pop $0
pop $1
MessageBox MB_OK "sharedmem::ReadIntoSharedMem  '$0'  '$1'"

functionEnd


