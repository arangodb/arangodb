------------------------------ MODULE FuerteH1TCP ---------------------------
EXTENDS Integers, TLC, Sequences

(*
--algorithm H1Connection {

variables   \* VERIFIED
  state = "Created",       \* this is a member variable in the program
  active = FALSE,     \* this is a member variable in the program
  queueSize = 0,      \* this is the length of the input queue
  alarm = "off",      \* this encodes, if an alarm is set
  asyncRunning = "none",   \* this encodes, if an async operation is scheduled
  iocontext = << >>,  \* this is what is posted on the iocontext
                      \* the following are all member variables in the object
  reading = FALSE,    \* set from the moment the asyncRead is posted until
                      \* the completion handler has executed
  writing = FALSE,    \* set from the moment the asyncWrite is posted until
                      \* the completion handler has executed
                      \* both are read in the corresponding timeout
                      \* to decide if the completion handler has already
                      \* finished

procedure startConnection() {   \* VERIFIED
(* This is called whenever the connection was created and the first request is queued. 
   Therefore we know that the state is "Created" and we are active and no alarm is set. *)
 startConnectionBegin:
  assert /\ state = "Created"
         /\ active
         /\ asyncRunning = "none"
         /\ alarm = "off";
  state := "Connecting";
  alarm := "connectAlarm";
  asyncRunning := "connect";
  return;
};

procedure asyncWriteNextRequest() {     \* VERIFIED
(* This is called whenever we are active and potentially want to continue
   writing. *)
 asyncWriteNextRequestBegin:
  assert /\ active
         /\ state = "Connected"
         /\ asyncRunning = "none";
  (* We use the check-set-check pattern to make sure we are not going to
     sleep although somebody has posted something on the queue. *)
  check1: {
    if (queueSize = 0) {
      deactivate:
        \* This is the only place which is allowed to set active to FALSE:
        active := FALSE;
      check2: {
        if (queueSize = 0) {
          if (state = "Connected") {
            alarm := "idleAlarm";
          };
          return;
        };
        reactivate:
          if (active) {
            \* if somebody else has set active instead of us (after we
            \* have set it to false above), he is also responsible for
            \* starting the write.
            return;
          } else {
            active := TRUE;
          }
      }
    };
  };
  pop: {
    queueSize := queueSize - 1;
    alarm := "writeAlarm";
    asyncRunning := "write";
    writing := TRUE;
    return;
  }
};

procedure shutdownConnection() {    \* VERIFIED
(* This is called whenever we shut down the connection. *)
 shutdownConnectionBegin:
  if (state = "Closed") {
    return;
  };
 shutdownConnectionReally:
  state := "Closed";
  if (asyncRunning = "connect") {
    asyncRunning := "connectBAD";
  } else if (asyncRunning = "write") {
    asyncRunning := "writeBAD";
  } else if (asyncRunning = "read") {
    asyncRunning := "readBAD";
  };
 shutdownLoop:
  while (TRUE) {
    queueSize := 0;
    active := FALSE;
   shutdownReCheck:
    if (queueSize = 0) {
      return;
    };
   shutdownReActivate:
    active := TRUE;
  };
};
  
process(fuerte = "fuertethread") {
  loop:
    while (TRUE) {
      either activate:   \* VERIFIED
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "activate") {
          assert state /= "Connecting";
          iocontext := Tail(iocontext);
          if (state = "Created") {
            call startConnection();
          } else if (state = "Connected") {
            call asyncWriteNextRequest();
          } else if (state = "Closed") {
            active := FALSE;
            queueSize := 0;
          };
        };
      
      or connectDone:    \* VERIFIED
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) \in {"connect", "connectBAD"}) {
          assert state \in {"Connecting", "Created", "Closed"};
          alarm := "off";
          if (Head(iocontext) = "connect" /\ state = "Connecting") {
            assert active;
            iocontext := Tail(iocontext);
            state := "Connected";
            call asyncWriteNextRequest();
          } else {
            (* for the sake of simplicity we ignore the retries here and
               directly go to the Closed state *)
            iocontext := Tail(iocontext);
            call shutdownConnection();
          }; 
        };
      
      or writeDone:   \* VERIFIED
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) \in {"write", "writeBAD"}) {
          assert /\ state \in {"Connected", "Closed"}
                 /\ writing;
          writing := FALSE;
          if (Head(iocontext) = "write" /\ state = "Connected") {
            iocontext := Tail(iocontext);
            asyncRunning := "read";
            reading := TRUE;
            alarm := "readAlarm";
          } else {
            iocontext := Tail(iocontext);
            alarm := "off";
            call shutdownConnection();
          };
        };
      
      or readDone:   \* VERIFIED
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) \in {"read", "readBAD"}) {
          assert state \in {"Connected", "Closed"}; 
          alarm := "off";
          reading := FALSE;
          if (Head(iocontext) = "read" /\ state = "Connected") {
            iocontext := Tail(iocontext);
            call asyncWriteNextRequest();
          } else {
            iocontext := Tail(iocontext);
            call shutdownConnection();
          };
        };
      
      or cancellation:   \* VERIFIED
        if (/\ Len(iocontext) >= 1 /\ Head(iocontext) = "cancel") {
          iocontext := Tail(iocontext);
          alarm := "off";
          call shutdownConnection();
        };

      or alarmTriggered:   \* VERIFIED
        if (alarm /= "off") {
          iocontext := Append(iocontext, alarm);
          alarm := "off";
        };
      
      or asyncFinished:    \* VERIFIED
        if (asyncRunning /= "none") {
          iocontext := Append(iocontext, asyncRunning);
          asyncRunning := "none";
        };

      or handleConnectAlarm:   \* VERIFIED
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "connectAlarm") {
          iocontext := Tail(iocontext);
          if (asyncRunning = "connect") {
            asyncRunning := "connectBAD";    \* indicate error on socket
          }
        };
      
      or handleWriteAlarm:    \* VERIFIED
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "writeAlarm") {
          iocontext := Tail(iocontext);
          if (writing /\ asyncRunning = "write") {
            \* note that we might have been scheduled by an off-going
            \* alarm but the writeDone which wants to disarm the alarm
            \* was scheduled already earlier, in this case we must not
            \* interfere
            asyncRunning := "writeBAD";
          }
        };
      
      or handleReadAlarm:    \* VERIFIED
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "readAlarm") {
          iocontext := Tail(iocontext);
          if (reading /\ asyncRunning = "read") {
            \* note that we might have been scheduled by an off-going
            \* alarm but the readDone which wants to disarm the alarm
            \* was scheduled already earlier, in this case we must not
            \* interfere
            asyncRunning := "readBAD";
          }
        };
      
      or handleIdleAlarm:  \* VERIFIED
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "idleAlarm") {
          iocontext := Tail(iocontext);
          if (~ active /\ state = "Connected") {
            alarm := "off";
            call shutdownConnection();
            \* it is possible that the idle alarm was still set, since we have
            \* gone to sleep and we have been posted on the iocontext beforehand
          }
        };
        
      or skip;
    }
};

process(client = "clientthread") {
  sendMessage:   \* VERIFIED
    while (TRUE) {
        await queueSize < 2 /\ state /= "Closed";
      pushQueue:
        queueSize := queueSize + 1;
      postActivate:
        if (~ active) {
          active := TRUE;
          iocontext := Append(iocontext, "activate");
        };
    };
};

process(cancel = "cancelthread") {
  cancelBegin:    \* VERIFIED
    iocontext := Append(iocontext, "cancel");
};

}
*)
\* BEGIN TRANSLATION - the hash of the PCal code: PCal-925ea62a5ed7f2fd7841f2836c7d799c
VARIABLES state, active, queueSize, alarm, asyncRunning, iocontext, reading, 
          writing, pc, stack

vars == << state, active, queueSize, alarm, asyncRunning, iocontext, reading, 
           writing, pc, stack >>

ProcSet == {"fuertethread"} \cup {"clientthread"} \cup {"cancelthread"}

Init == (* Global variables *)
        /\ state = "Created"
        /\ active = FALSE
        /\ queueSize = 0
        /\ alarm = "off"
        /\ asyncRunning = "none"
        /\ iocontext = << >>
        /\ reading = FALSE
        /\ writing = FALSE
        /\ stack = [self \in ProcSet |-> << >>]
        /\ pc = [self \in ProcSet |-> CASE self = "fuertethread" -> "loop"
                                        [] self = "clientthread" -> "sendMessage"
                                        [] self = "cancelthread" -> "cancelBegin"]

startConnectionBegin(self) == /\ pc[self] = "startConnectionBegin"
                              /\ Assert(/\ state = "Created"
                                        /\ active
                                        /\ asyncRunning = "none"
                                        /\ alarm = "off", 
                                        "Failure of assertion at line 27, column 3.")
                              /\ state' = "Connecting"
                              /\ alarm' = "connectAlarm"
                              /\ asyncRunning' = "connect"
                              /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                              /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                              /\ UNCHANGED << active, queueSize, iocontext, 
                                              reading, writing >>

startConnection(self) == startConnectionBegin(self)

asyncWriteNextRequestBegin(self) == /\ pc[self] = "asyncWriteNextRequestBegin"
                                    /\ Assert(/\ active
                                              /\ state = "Connected"
                                              /\ asyncRunning = "none", 
                                              "Failure of assertion at line 41, column 3.")
                                    /\ pc' = [pc EXCEPT ![self] = "check1"]
                                    /\ UNCHANGED << state, active, queueSize, 
                                                    alarm, asyncRunning, 
                                                    iocontext, reading, 
                                                    writing, stack >>

check1(self) == /\ pc[self] = "check1"
                /\ IF queueSize = 0
                      THEN /\ pc' = [pc EXCEPT ![self] = "deactivate"]
                      ELSE /\ pc' = [pc EXCEPT ![self] = "pop"]
                /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                                iocontext, reading, writing, stack >>

deactivate(self) == /\ pc[self] = "deactivate"
                    /\ active' = FALSE
                    /\ pc' = [pc EXCEPT ![self] = "check2"]
                    /\ UNCHANGED << state, queueSize, alarm, asyncRunning, 
                                    iocontext, reading, writing, stack >>

check2(self) == /\ pc[self] = "check2"
                /\ IF queueSize = 0
                      THEN /\ IF state = "Connected"
                                 THEN /\ alarm' = "idleAlarm"
                                 ELSE /\ TRUE
                                      /\ alarm' = alarm
                           /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                           /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                      ELSE /\ pc' = [pc EXCEPT ![self] = "reactivate"]
                           /\ UNCHANGED << alarm, stack >>
                /\ UNCHANGED << state, active, queueSize, asyncRunning, 
                                iocontext, reading, writing >>

reactivate(self) == /\ pc[self] = "reactivate"
                    /\ IF active
                          THEN /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                               /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                               /\ UNCHANGED active
                          ELSE /\ active' = TRUE
                               /\ pc' = [pc EXCEPT ![self] = "pop"]
                               /\ stack' = stack
                    /\ UNCHANGED << state, queueSize, alarm, asyncRunning, 
                                    iocontext, reading, writing >>

pop(self) == /\ pc[self] = "pop"
             /\ queueSize' = queueSize - 1
             /\ alarm' = "writeAlarm"
             /\ asyncRunning' = "write"
             /\ writing' = TRUE
             /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
             /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
             /\ UNCHANGED << state, active, iocontext, reading >>

asyncWriteNextRequest(self) == asyncWriteNextRequestBegin(self)
                                  \/ check1(self) \/ deactivate(self)
                                  \/ check2(self) \/ reactivate(self)
                                  \/ pop(self)

shutdownConnectionBegin(self) == /\ pc[self] = "shutdownConnectionBegin"
                                 /\ IF state = "Closed"
                                       THEN /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                                            /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                                       ELSE /\ pc' = [pc EXCEPT ![self] = "shutdownConnectionReally"]
                                            /\ stack' = stack
                                 /\ UNCHANGED << state, active, queueSize, 
                                                 alarm, asyncRunning, 
                                                 iocontext, reading, writing >>

shutdownConnectionReally(self) == /\ pc[self] = "shutdownConnectionReally"
                                  /\ state' = "Closed"
                                  /\ IF asyncRunning = "connect"
                                        THEN /\ asyncRunning' = "connectBAD"
                                        ELSE /\ IF asyncRunning = "write"
                                                   THEN /\ asyncRunning' = "writeBAD"
                                                   ELSE /\ IF asyncRunning = "read"
                                                              THEN /\ asyncRunning' = "readBAD"
                                                              ELSE /\ TRUE
                                                                   /\ UNCHANGED asyncRunning
                                  /\ pc' = [pc EXCEPT ![self] = "shutdownLoop"]
                                  /\ UNCHANGED << active, queueSize, alarm, 
                                                  iocontext, reading, writing, 
                                                  stack >>

shutdownLoop(self) == /\ pc[self] = "shutdownLoop"
                      /\ queueSize' = 0
                      /\ active' = FALSE
                      /\ pc' = [pc EXCEPT ![self] = "shutdownReCheck"]
                      /\ UNCHANGED << state, alarm, asyncRunning, iocontext, 
                                      reading, writing, stack >>

shutdownReCheck(self) == /\ pc[self] = "shutdownReCheck"
                         /\ IF queueSize = 0
                               THEN /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                                    /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                               ELSE /\ pc' = [pc EXCEPT ![self] = "shutdownReActivate"]
                                    /\ stack' = stack
                         /\ UNCHANGED << state, active, queueSize, alarm, 
                                         asyncRunning, iocontext, reading, 
                                         writing >>

shutdownReActivate(self) == /\ pc[self] = "shutdownReActivate"
                            /\ active' = TRUE
                            /\ pc' = [pc EXCEPT ![self] = "shutdownLoop"]
                            /\ UNCHANGED << state, queueSize, alarm, 
                                            asyncRunning, iocontext, reading, 
                                            writing, stack >>

shutdownConnection(self) == shutdownConnectionBegin(self)
                               \/ shutdownConnectionReally(self)
                               \/ shutdownLoop(self)
                               \/ shutdownReCheck(self)
                               \/ shutdownReActivate(self)

loop == /\ pc["fuertethread"] = "loop"
        /\ \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "activate"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "connectDone"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "writeDone"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "readDone"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "cancellation"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "alarmTriggered"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncFinished"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "handleConnectAlarm"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "handleWriteAlarm"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "handleReadAlarm"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "handleIdleAlarm"]
           \/ /\ TRUE
              /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
        /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                        iocontext, reading, writing, stack >>

activate == /\ pc["fuertethread"] = "activate"
            /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "activate"
                  THEN /\ Assert(state /= "Connecting", 
                                 "Failure of assertion at line 112, column 11.")
                       /\ iocontext' = Tail(iocontext)
                       /\ IF state = "Created"
                             THEN /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "startConnection",
                                                                                     pc        |->  "loop" ] >>
                                                                                 \o stack["fuertethread"]]
                                  /\ pc' = [pc EXCEPT !["fuertethread"] = "startConnectionBegin"]
                                  /\ UNCHANGED << active, queueSize >>
                             ELSE /\ IF state = "Connected"
                                        THEN /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "asyncWriteNextRequest",
                                                                                                pc        |->  "loop" ] >>
                                                                                            \o stack["fuertethread"]]
                                             /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncWriteNextRequestBegin"]
                                             /\ UNCHANGED << active, queueSize >>
                                        ELSE /\ IF state = "Closed"
                                                   THEN /\ active' = FALSE
                                                        /\ queueSize' = 0
                                                   ELSE /\ TRUE
                                                        /\ UNCHANGED << active, 
                                                                        queueSize >>
                                             /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                                             /\ stack' = stack
                  ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                       /\ UNCHANGED << active, queueSize, iocontext, stack >>
            /\ UNCHANGED << state, alarm, asyncRunning, reading, writing >>

connectDone == /\ pc["fuertethread"] = "connectDone"
               /\ IF /\ Len(iocontext) >= 1
                     /\ Head(iocontext) \in {"connect", "connectBAD"}
                     THEN /\ Assert(state \in {"Connecting", "Created", "Closed"}, 
                                    "Failure of assertion at line 127, column 11.")
                          /\ alarm' = "off"
                          /\ IF Head(iocontext) = "connect" /\ state = "Connecting"
                                THEN /\ Assert(active, 
                                               "Failure of assertion at line 130, column 13.")
                                     /\ iocontext' = Tail(iocontext)
                                     /\ state' = "Connected"
                                     /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "asyncWriteNextRequest",
                                                                                        pc        |->  "loop" ] >>
                                                                                    \o stack["fuertethread"]]
                                     /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncWriteNextRequestBegin"]
                                ELSE /\ iocontext' = Tail(iocontext)
                                     /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "shutdownConnection",
                                                                                        pc        |->  "loop" ] >>
                                                                                    \o stack["fuertethread"]]
                                     /\ pc' = [pc EXCEPT !["fuertethread"] = "shutdownConnectionBegin"]
                                     /\ state' = state
                     ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                          /\ UNCHANGED << state, alarm, iocontext, stack >>
               /\ UNCHANGED << active, queueSize, asyncRunning, reading, 
                               writing >>

writeDone == /\ pc["fuertethread"] = "writeDone"
             /\ IF /\ Len(iocontext) >= 1
                   /\ Head(iocontext) \in {"write", "writeBAD"}
                   THEN /\ Assert(/\ state \in {"Connected", "Closed"}
                                  /\ writing, 
                                  "Failure of assertion at line 145, column 11.")
                        /\ writing' = FALSE
                        /\ IF Head(iocontext) = "write" /\ state = "Connected"
                              THEN /\ iocontext' = Tail(iocontext)
                                   /\ asyncRunning' = "read"
                                   /\ reading' = TRUE
                                   /\ alarm' = "readAlarm"
                                   /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                                   /\ stack' = stack
                              ELSE /\ iocontext' = Tail(iocontext)
                                   /\ alarm' = "off"
                                   /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "shutdownConnection",
                                                                                      pc        |->  "loop" ] >>
                                                                                  \o stack["fuertethread"]]
                                   /\ pc' = [pc EXCEPT !["fuertethread"] = "shutdownConnectionBegin"]
                                   /\ UNCHANGED << asyncRunning, reading >>
                   ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                        /\ UNCHANGED << alarm, asyncRunning, iocontext, 
                                        reading, writing, stack >>
             /\ UNCHANGED << state, active, queueSize >>

readDone == /\ pc["fuertethread"] = "readDone"
            /\ IF /\ Len(iocontext) >= 1
                  /\ Head(iocontext) \in {"read", "readBAD"}
                  THEN /\ Assert(state \in {"Connected", "Closed"}, 
                                 "Failure of assertion at line 163, column 11.")
                       /\ alarm' = "off"
                       /\ reading' = FALSE
                       /\ IF Head(iocontext) = "read" /\ state = "Connected"
                             THEN /\ iocontext' = Tail(iocontext)
                                  /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "asyncWriteNextRequest",
                                                                                     pc        |->  "loop" ] >>
                                                                                 \o stack["fuertethread"]]
                                  /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncWriteNextRequestBegin"]
                             ELSE /\ iocontext' = Tail(iocontext)
                                  /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "shutdownConnection",
                                                                                     pc        |->  "loop" ] >>
                                                                                 \o stack["fuertethread"]]
                                  /\ pc' = [pc EXCEPT !["fuertethread"] = "shutdownConnectionBegin"]
                  ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                       /\ UNCHANGED << alarm, iocontext, reading, stack >>
            /\ UNCHANGED << state, active, queueSize, asyncRunning, writing >>

cancellation == /\ pc["fuertethread"] = "cancellation"
                /\ IF /\ Len(iocontext) >= 1 /\ Head(iocontext) = "cancel"
                      THEN /\ iocontext' = Tail(iocontext)
                           /\ alarm' = "off"
                           /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "shutdownConnection",
                                                                              pc        |->  "loop" ] >>
                                                                          \o stack["fuertethread"]]
                           /\ pc' = [pc EXCEPT !["fuertethread"] = "shutdownConnectionBegin"]
                      ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                           /\ UNCHANGED << alarm, iocontext, stack >>
                /\ UNCHANGED << state, active, queueSize, asyncRunning, 
                                reading, writing >>

alarmTriggered == /\ pc["fuertethread"] = "alarmTriggered"
                  /\ IF alarm /= "off"
                        THEN /\ iocontext' = Append(iocontext, alarm)
                             /\ alarm' = "off"
                        ELSE /\ TRUE
                             /\ UNCHANGED << alarm, iocontext >>
                  /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                  /\ UNCHANGED << state, active, queueSize, asyncRunning, 
                                  reading, writing, stack >>

asyncFinished == /\ pc["fuertethread"] = "asyncFinished"
                 /\ IF asyncRunning /= "none"
                       THEN /\ iocontext' = Append(iocontext, asyncRunning)
                            /\ asyncRunning' = "none"
                       ELSE /\ TRUE
                            /\ UNCHANGED << asyncRunning, iocontext >>
                 /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                 /\ UNCHANGED << state, active, queueSize, alarm, reading, 
                                 writing, stack >>

handleConnectAlarm == /\ pc["fuertethread"] = "handleConnectAlarm"
                      /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "connectAlarm"
                            THEN /\ iocontext' = Tail(iocontext)
                                 /\ IF asyncRunning = "connect"
                                       THEN /\ asyncRunning' = "connectBAD"
                                       ELSE /\ TRUE
                                            /\ UNCHANGED asyncRunning
                            ELSE /\ TRUE
                                 /\ UNCHANGED << asyncRunning, iocontext >>
                      /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                      /\ UNCHANGED << state, active, queueSize, alarm, reading, 
                                      writing, stack >>

handleWriteAlarm == /\ pc["fuertethread"] = "handleWriteAlarm"
                    /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "writeAlarm"
                          THEN /\ iocontext' = Tail(iocontext)
                               /\ IF writing /\ asyncRunning = "write"
                                     THEN /\ asyncRunning' = "writeBAD"
                                     ELSE /\ TRUE
                                          /\ UNCHANGED asyncRunning
                          ELSE /\ TRUE
                               /\ UNCHANGED << asyncRunning, iocontext >>
                    /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                    /\ UNCHANGED << state, active, queueSize, alarm, reading, 
                                    writing, stack >>

handleReadAlarm == /\ pc["fuertethread"] = "handleReadAlarm"
                   /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "readAlarm"
                         THEN /\ iocontext' = Tail(iocontext)
                              /\ IF reading /\ asyncRunning = "read"
                                    THEN /\ asyncRunning' = "readBAD"
                                    ELSE /\ TRUE
                                         /\ UNCHANGED asyncRunning
                         ELSE /\ TRUE
                              /\ UNCHANGED << asyncRunning, iocontext >>
                   /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                   /\ UNCHANGED << state, active, queueSize, alarm, reading, 
                                   writing, stack >>

handleIdleAlarm == /\ pc["fuertethread"] = "handleIdleAlarm"
                   /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "idleAlarm"
                         THEN /\ iocontext' = Tail(iocontext)
                              /\ IF ~ active /\ state = "Connected"
                                    THEN /\ alarm' = "off"
                                         /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "shutdownConnection",
                                                                                            pc        |->  "loop" ] >>
                                                                                        \o stack["fuertethread"]]
                                         /\ pc' = [pc EXCEPT !["fuertethread"] = "shutdownConnectionBegin"]
                                    ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                                         /\ UNCHANGED << alarm, stack >>
                         ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                              /\ UNCHANGED << alarm, iocontext, stack >>
                   /\ UNCHANGED << state, active, queueSize, asyncRunning, 
                                   reading, writing >>

fuerte == loop \/ activate \/ connectDone \/ writeDone \/ readDone
             \/ cancellation \/ alarmTriggered \/ asyncFinished
             \/ handleConnectAlarm \/ handleWriteAlarm \/ handleReadAlarm
             \/ handleIdleAlarm

sendMessage == /\ pc["clientthread"] = "sendMessage"
               /\ queueSize < 2 /\ state /= "Closed"
               /\ pc' = [pc EXCEPT !["clientthread"] = "pushQueue"]
               /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                               iocontext, reading, writing, stack >>

pushQueue == /\ pc["clientthread"] = "pushQueue"
             /\ queueSize' = queueSize + 1
             /\ pc' = [pc EXCEPT !["clientthread"] = "postActivate"]
             /\ UNCHANGED << state, active, alarm, asyncRunning, iocontext, 
                             reading, writing, stack >>

postActivate == /\ pc["clientthread"] = "postActivate"
                /\ IF ~ active
                      THEN /\ active' = TRUE
                           /\ iocontext' = Append(iocontext, "activate")
                      ELSE /\ TRUE
                           /\ UNCHANGED << active, iocontext >>
                /\ pc' = [pc EXCEPT !["clientthread"] = "sendMessage"]
                /\ UNCHANGED << state, queueSize, alarm, asyncRunning, reading, 
                                writing, stack >>

client == sendMessage \/ pushQueue \/ postActivate

cancelBegin == /\ pc["cancelthread"] = "cancelBegin"
               /\ iocontext' = Append(iocontext, "cancel")
               /\ pc' = [pc EXCEPT !["cancelthread"] = "Done"]
               /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                               reading, writing, stack >>

cancel == cancelBegin

Next == fuerte \/ client \/ cancel
           \/ (\E self \in ProcSet:  \/ startConnection(self)
                                     \/ asyncWriteNextRequest(self)
                                     \/ shutdownConnection(self))

Spec == Init /\ [][Next]_vars

\* END TRANSLATION - the hash of the generated TLA code (remove to silence divergence warnings): TLA-b4f0892f475b000215f25d9b188fcb51

\* Invariants to prove:

TypeOK == /\ state \in {"Created", "Connecting", "Connected", "Closed"}
          /\ active \in {FALSE, TRUE}
          /\ queueSize \in 0..2
          /\ alarm \in {"off", "connectAlarm", "writeAlarm", "readAlarm",
                        "idleAlarm"}
          /\ asyncRunning \in {"none", "connect", "connectBAD", "write",
                               "writeBAD", "read", "readBAD"}
          /\ reading \in {FALSE, TRUE}
          /\ writing \in {FALSE, TRUE}
          
(* When we are active, something has to be ongoing, either an async call, or
   something scheduled on the iocontext. However, there are three tiny moments when
   we are just about to start the connection anew, or when we are about to start
   a write operation, or when we are already shutting down the connection, which
   we have to exclude. *)
WorkingIfActive == active => \/ asyncRunning /= "none"
                             \/ Len(iocontext) > 0
                             \/ state = "Closed"
                             \/ pc["fuertethread"] = "startConnectionBegin"
                             \/ pc["fuertethread"] = "asyncWriteNextRequestBegin"
                             \/ pc["fuertethread"] = "check1"
                             \/ pc["fuertethread"] = "deactivate"
                             \/ pc["fuertethread"] = "pop"
                             \/ pc["fuertethread"] = "shutdownConnectionBegin"
                             \/ pc["fuertethread"] = "shutdownConnectionReally"
                             
(* We basically want to either have an empty queue or want to be active.
   There are very tiny moments in time when both are violated, one is
   exactly when a new message is on the queue but the fuerte thread is not
   yet reactivated. The other is, if the check-set-check pattern is between
   the set and the recheck. These are the pc exceptions we have to have here: *)
NothingForgottenOnQueue == \/ queueSize = 0
                           \/ active
                           \/ pc["clientthread"] = "postActivate"
                           \/ pc["fuertethread"] = "check2"
                           \/ pc["fuertethread"] = "reactivate"
                           
NoSleepingBarber == /\ NothingForgottenOnQueue
                    /\ WorkingIfActive


=============================================================================
\* Modification History
\* Last modified Fri Aug 21 16:03:16 CEST 2020 by neunhoef
\* Last modified Wed Jul 22 12:06:32 CEST 2020 by simon
\* Created Mi 22. Apr 22:46:19 CEST 2020 by neunhoef
