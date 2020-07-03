------------------------------ MODULE Fuerte36FixedHttp ---------------------------
EXTENDS Integers, TLC, Sequences

(*
--algorithm HttpConnection {

variables
  state = "Disconnected",       \* this is a member variable in the program
  active = FALSE,     \* this is a member variable in the program
  queueSize = 0,      \* this is the length of the input queue
  alarm = "off",      \* this encodes, if an alarm is set
  asyncRunning = "none",   \* this encodes, if an async operation is scheduled
  iocontext = << >>;  \* this is what is posted on the iocontext
                      \* the following are all member variables in the object

procedure startConnection() {
 startConnectionBegin:
  if (state = "Disconnected") {
    state := "Connecting";
    alarm := "connectAlarm";
    asyncRunning := "connect";
  };
  return;
};

procedure shutdownConnection() {
 startShutdownConnection:
  state := "Failed";
  alarm := "off";
  \* Shut down connection:
  if (asyncRunning = "read") {
    asyncRunning := "readBAD";
  } else if (asyncRunning ="write") {
    asyncRunning := "writeBAD";
  } else if (asyncRunning = "connect") {
    asyncRunning := "connectBAD";
  };
  active := FALSE;
  return;
};

procedure restartConnection() {
 restartConnectionBegin:
  if (state = "Connected") {
    state := "Disconnected";
    call shutdownConnection();
  };
 doneRestartConnection:
  return;
};

procedure asyncWriteNextRequest() 
  variable popped = FALSE; {
  asyncWriteNextRequestBegin:
    skip;
    \* assert active;
  check1: { 
    if (queueSize > 0) {
      queueSize := queueSize - 1;
      popped := TRUE;
    };
    if (~ popped) {
      deactivate:
        active := FALSE;
      check2: {
        if (queueSize > 0) {
          queueSize := queueSize - 1;
          popped := TRUE;
        };
        if (~ popped) {
          alarm := "idleAlarm";
         doneDeactivate:
          return;
        };
      reactivate:
        active := TRUE;
      }
    };
  };
  pop: {
    assert popped;
    alarm := "writeAlarm";
    asyncRunning := "write";
    return;
  }
};

procedure startWriting() {
 startStartWriting:
  if (~ active) {
    active := TRUE;
    if (state /= "Connected") {
     needToConnect: 
      active := FALSE;
      if (state = "Disconnected") {
        call startConnection();
      };
    } else {
      call asyncWriteNextRequest();
    };
  };
 doneStartWriting:
  return;
};

process(fuerte = "fuertethread") {
  loop:
    while (TRUE) {
      either connectDone:
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) \in {"connect", "connectBAD"}) {
          alarm := "off";
          if (Head(iocontext) = "connect") {
            either reallyGood1: {
              iocontext := Tail(iocontext);
              state := "Connected";
              call startWriting();
              goto doneConnect;
            }
            or infactBad1:
              skip;
          };
          (* for the sake of simplicity we ignore the retries here and
             directly go to the Failed state *)
        failed:
          iocontext := Tail(iocontext);
          state := "Failed";
          queueSize := 0;
          call shutdownConnection();
         doneConnect: skip
        };
      
      or writeDone:
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) \in {"write", "writeBAD"}) {
          alarm := "off";
          if (Head(iocontext) = "write") {
            either reallyGood2: {
              iocontext := Tail(iocontext);
              asyncRunning := "read";
              alarm := "readAlarm";
              goto doneWrite;
            }
            or infactBad2:
              skip;
          };
         failedWrite:
          iocontext := Tail(iocontext);
          call restartConnection();
         doneWrite:
          skip;
        };
      
      or readDone:
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) \in {"read", "readBAD"}) {
          alarm := "off";
          if (Head(iocontext) = "read" /\ state = "Connected") {
            either reallyGood3: {
              iocontext := Tail(iocontext);
              call asyncWriteNextRequest();
              goto doneRead;
            }
            or infactBad3:
              skip;
          };
         failed3:
          iocontext := Tail(iocontext);
          call restartConnection();
         doneRead:
          skip;
        };
      
      or alarmTriggered:
        if (alarm /= "off") {
          iocontext := Append(iocontext, alarm);
          alarm := "off";
        };
      
      or asyncFinished:
        if (asyncRunning /= "none") {
          iocontext := Append(iocontext, asyncRunning);
          asyncRunning := "none";
        };

      or handleConnectAlarm:
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "connectAlarm") {
          iocontext := Tail(iocontext);
          if (asyncRunning = "connect") {
            asyncRunning := "connectBAD";    \* indicate error on socket
          };
        };
      
      or handleOtherAlarm:
        if (Len(iocontext) >= 1 /\ Head(iocontext) \in {"writeAlarm", "readAlarm", "idleAlarm"}) {
          iocontext := Tail(iocontext);
          if (active) {
            call restartConnection();
          } else {
            call shutdownConnection();
          };
        };
      
      or cancellation:
        if (/\ Len(iocontext) >= 1 /\ Head(iocontext) = "cancel") {
          iocontext := Tail(iocontext);
          if (active) {
            call restartConnection();
          } else {
            call shutdownConnection();
          };
         drainQueue:
          queueSize := 0;
        };

      or activation:
        if (/\ Len(iocontext) >= 1 /\ Head(iocontext) = "activate") {
          iocontext := Tail(iocontext);
          if (state = "Connected") {
            call asyncWriteNextRequest();
          } else if (state = "Disconnected") {
            call startConnection();
          } else if (state = "Failed") {
            queueSize := 0;
            active := FALSE;
          };
        };

      or skip;
    }
};

process(client = "clientthread") {
  sendMessage:
    while (TRUE) {
        await queueSize < 2;
        \* await queueSize = 0 /\ ~ active;
      pushQueue:
        if (state /= "Failed") {
          queueSize := queueSize + 1;
        };
      postActivate:
        if (~ active) {
          active := TRUE;
          iocontext := Append(iocontext, "activate");
        };
    };
};

process(cancel = "cancelthread") {
  cancelBegin:
    state := "Failed";
  pushCancel:
    iocontext := Append(iocontext, "cancel");
};

}
*)
\* BEGIN TRANSLATION
VARIABLES state, active, queueSize, alarm, asyncRunning, iocontext, pc, stack, 
          popped

vars == << state, active, queueSize, alarm, asyncRunning, iocontext, pc, 
           stack, popped >>

ProcSet == {"fuertethread"} \cup {"clientthread"} \cup {"cancelthread"}

Init == (* Global variables *)
        /\ state = "Disconnected"
        /\ active = FALSE
        /\ queueSize = 0
        /\ alarm = "off"
        /\ asyncRunning = "none"
        /\ iocontext = << >>
        (* Procedure asyncWriteNextRequest *)
        /\ popped = [ self \in ProcSet |-> FALSE]
        /\ stack = [self \in ProcSet |-> << >>]
        /\ pc = [self \in ProcSet |-> CASE self = "fuertethread" -> "loop"
                                        [] self = "clientthread" -> "sendMessage"
                                        [] self = "cancelthread" -> "cancelBegin"]

startConnectionBegin(self) == /\ pc[self] = "startConnectionBegin"
                              /\ IF state = "Disconnected"
                                    THEN /\ state' = "Connecting"
                                         /\ alarm' = "connectAlarm"
                                         /\ asyncRunning' = "connect"
                                    ELSE /\ TRUE
                                         /\ UNCHANGED << state, alarm, 
                                                         asyncRunning >>
                              /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                              /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                              /\ UNCHANGED << active, queueSize, iocontext, 
                                              popped >>

startConnection(self) == startConnectionBegin(self)

startShutdownConnection(self) == /\ pc[self] = "startShutdownConnection"
                                 /\ state' = "Failed"
                                 /\ alarm' = "off"
                                 /\ IF asyncRunning = "read"
                                       THEN /\ asyncRunning' = "readBAD"
                                       ELSE /\ IF asyncRunning ="write"
                                                  THEN /\ asyncRunning' = "writeBAD"
                                                  ELSE /\ IF asyncRunning = "connect"
                                                             THEN /\ asyncRunning' = "connectBAD"
                                                             ELSE /\ TRUE
                                                                  /\ UNCHANGED asyncRunning
                                 /\ active' = FALSE
                                 /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                                 /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                                 /\ UNCHANGED << queueSize, iocontext, popped >>

shutdownConnection(self) == startShutdownConnection(self)

restartConnectionBegin(self) == /\ pc[self] = "restartConnectionBegin"
                                /\ IF state = "Connected"
                                      THEN /\ state' = "Disconnected"
                                           /\ stack' = [stack EXCEPT ![self] = << [ procedure |->  "shutdownConnection",
                                                                                    pc        |->  "doneRestartConnection" ] >>
                                                                                \o stack[self]]
                                           /\ pc' = [pc EXCEPT ![self] = "startShutdownConnection"]
                                      ELSE /\ pc' = [pc EXCEPT ![self] = "doneRestartConnection"]
                                           /\ UNCHANGED << state, stack >>
                                /\ UNCHANGED << active, queueSize, alarm, 
                                                asyncRunning, iocontext, 
                                                popped >>

doneRestartConnection(self) == /\ pc[self] = "doneRestartConnection"
                               /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                               /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                               /\ UNCHANGED << state, active, queueSize, alarm, 
                                               asyncRunning, iocontext, popped >>

restartConnection(self) == restartConnectionBegin(self)
                              \/ doneRestartConnection(self)

asyncWriteNextRequestBegin(self) == /\ pc[self] = "asyncWriteNextRequestBegin"
                                    /\ TRUE
                                    /\ pc' = [pc EXCEPT ![self] = "check1"]
                                    /\ UNCHANGED << state, active, queueSize, 
                                                    alarm, asyncRunning, 
                                                    iocontext, stack, popped >>

check1(self) == /\ pc[self] = "check1"
                /\ IF queueSize > 0
                      THEN /\ queueSize' = queueSize - 1
                           /\ popped' = [popped EXCEPT ![self] = TRUE]
                      ELSE /\ TRUE
                           /\ UNCHANGED << queueSize, popped >>
                /\ IF ~ popped'[self]
                      THEN /\ pc' = [pc EXCEPT ![self] = "deactivate"]
                      ELSE /\ pc' = [pc EXCEPT ![self] = "pop"]
                /\ UNCHANGED << state, active, alarm, asyncRunning, iocontext, 
                                stack >>

deactivate(self) == /\ pc[self] = "deactivate"
                    /\ active' = FALSE
                    /\ pc' = [pc EXCEPT ![self] = "check2"]
                    /\ UNCHANGED << state, queueSize, alarm, asyncRunning, 
                                    iocontext, stack, popped >>

check2(self) == /\ pc[self] = "check2"
                /\ IF queueSize > 0
                      THEN /\ queueSize' = queueSize - 1
                           /\ popped' = [popped EXCEPT ![self] = TRUE]
                      ELSE /\ TRUE
                           /\ UNCHANGED << queueSize, popped >>
                /\ IF ~ popped'[self]
                      THEN /\ alarm' = "idleAlarm"
                           /\ pc' = [pc EXCEPT ![self] = "doneDeactivate"]
                      ELSE /\ pc' = [pc EXCEPT ![self] = "reactivate"]
                           /\ alarm' = alarm
                /\ UNCHANGED << state, active, asyncRunning, iocontext, stack >>

doneDeactivate(self) == /\ pc[self] = "doneDeactivate"
                        /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                        /\ popped' = [popped EXCEPT ![self] = Head(stack[self]).popped]
                        /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                        /\ UNCHANGED << state, active, queueSize, alarm, 
                                        asyncRunning, iocontext >>

reactivate(self) == /\ pc[self] = "reactivate"
                    /\ active' = TRUE
                    /\ pc' = [pc EXCEPT ![self] = "pop"]
                    /\ UNCHANGED << state, queueSize, alarm, asyncRunning, 
                                    iocontext, stack, popped >>

pop(self) == /\ pc[self] = "pop"
             /\ Assert(popped[self], 
                       "Failure of assertion at line 81, column 5.")
             /\ alarm' = "writeAlarm"
             /\ asyncRunning' = "write"
             /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
             /\ popped' = [popped EXCEPT ![self] = Head(stack[self]).popped]
             /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
             /\ UNCHANGED << state, active, queueSize, iocontext >>

asyncWriteNextRequest(self) == asyncWriteNextRequestBegin(self)
                                  \/ check1(self) \/ deactivate(self)
                                  \/ check2(self) \/ doneDeactivate(self)
                                  \/ reactivate(self) \/ pop(self)

startStartWriting(self) == /\ pc[self] = "startStartWriting"
                           /\ IF ~ active
                                 THEN /\ active' = TRUE
                                      /\ IF state /= "Connected"
                                            THEN /\ pc' = [pc EXCEPT ![self] = "needToConnect"]
                                                 /\ UNCHANGED << stack, popped >>
                                            ELSE /\ stack' = [stack EXCEPT ![self] = << [ procedure |->  "asyncWriteNextRequest",
                                                                                          pc        |->  "doneStartWriting",
                                                                                          popped    |->  popped[self] ] >>
                                                                                      \o stack[self]]
                                                 /\ popped' = [popped EXCEPT ![self] = FALSE]
                                                 /\ pc' = [pc EXCEPT ![self] = "asyncWriteNextRequestBegin"]
                                 ELSE /\ pc' = [pc EXCEPT ![self] = "doneStartWriting"]
                                      /\ UNCHANGED << active, stack, popped >>
                           /\ UNCHANGED << state, queueSize, alarm, 
                                           asyncRunning, iocontext >>

needToConnect(self) == /\ pc[self] = "needToConnect"
                       /\ active' = FALSE
                       /\ IF state = "Disconnected"
                             THEN /\ stack' = [stack EXCEPT ![self] = << [ procedure |->  "startConnection",
                                                                           pc        |->  "doneStartWriting" ] >>
                                                                       \o stack[self]]
                                  /\ pc' = [pc EXCEPT ![self] = "startConnectionBegin"]
                             ELSE /\ pc' = [pc EXCEPT ![self] = "doneStartWriting"]
                                  /\ stack' = stack
                       /\ UNCHANGED << state, queueSize, alarm, asyncRunning, 
                                       iocontext, popped >>

doneStartWriting(self) == /\ pc[self] = "doneStartWriting"
                          /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                          /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                          /\ UNCHANGED << state, active, queueSize, alarm, 
                                          asyncRunning, iocontext, popped >>

startWriting(self) == startStartWriting(self) \/ needToConnect(self)
                         \/ doneStartWriting(self)

loop == /\ pc["fuertethread"] = "loop"
        /\ \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "connectDone"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "writeDone"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "readDone"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "alarmTriggered"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncFinished"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "handleConnectAlarm"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "handleOtherAlarm"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "cancellation"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "activation"]
           \/ /\ TRUE
              /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
        /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                        iocontext, stack, popped >>

connectDone == /\ pc["fuertethread"] = "connectDone"
               /\ IF /\ Len(iocontext) >= 1
                     /\ Head(iocontext) \in {"connect", "connectBAD"}
                     THEN /\ alarm' = "off"
                          /\ IF Head(iocontext) = "connect"
                                THEN /\ \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "reallyGood1"]
                                        \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "infactBad1"]
                                ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "failed"]
                     ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                          /\ alarm' = alarm
               /\ UNCHANGED << state, active, queueSize, asyncRunning, 
                               iocontext, stack, popped >>

failed == /\ pc["fuertethread"] = "failed"
          /\ iocontext' = Tail(iocontext)
          /\ state' = "Failed"
          /\ queueSize' = 0
          /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "shutdownConnection",
                                                             pc        |->  "doneConnect" ] >>
                                                         \o stack["fuertethread"]]
          /\ pc' = [pc EXCEPT !["fuertethread"] = "startShutdownConnection"]
          /\ UNCHANGED << active, alarm, asyncRunning, popped >>

doneConnect == /\ pc["fuertethread"] = "doneConnect"
               /\ TRUE
               /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
               /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                               iocontext, stack, popped >>

reallyGood1 == /\ pc["fuertethread"] = "reallyGood1"
               /\ iocontext' = Tail(iocontext)
               /\ state' = "Connected"
               /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "startWriting",
                                                                  pc        |->  "doneConnect" ] >>
                                                              \o stack["fuertethread"]]
               /\ pc' = [pc EXCEPT !["fuertethread"] = "startStartWriting"]
               /\ UNCHANGED << active, queueSize, alarm, asyncRunning, popped >>

infactBad1 == /\ pc["fuertethread"] = "infactBad1"
              /\ TRUE
              /\ pc' = [pc EXCEPT !["fuertethread"] = "failed"]
              /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                              iocontext, stack, popped >>

writeDone == /\ pc["fuertethread"] = "writeDone"
             /\ IF /\ Len(iocontext) >= 1
                   /\ Head(iocontext) \in {"write", "writeBAD"}
                   THEN /\ alarm' = "off"
                        /\ IF Head(iocontext) = "write"
                              THEN /\ \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "reallyGood2"]
                                      \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "infactBad2"]
                              ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "failedWrite"]
                   ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                        /\ alarm' = alarm
             /\ UNCHANGED << state, active, queueSize, asyncRunning, iocontext, 
                             stack, popped >>

failedWrite == /\ pc["fuertethread"] = "failedWrite"
               /\ iocontext' = Tail(iocontext)
               /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "restartConnection",
                                                                  pc        |->  "doneWrite" ] >>
                                                              \o stack["fuertethread"]]
               /\ pc' = [pc EXCEPT !["fuertethread"] = "restartConnectionBegin"]
               /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                               popped >>

doneWrite == /\ pc["fuertethread"] = "doneWrite"
             /\ TRUE
             /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
             /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                             iocontext, stack, popped >>

reallyGood2 == /\ pc["fuertethread"] = "reallyGood2"
               /\ iocontext' = Tail(iocontext)
               /\ asyncRunning' = "read"
               /\ alarm' = "readAlarm"
               /\ pc' = [pc EXCEPT !["fuertethread"] = "doneWrite"]
               /\ UNCHANGED << state, active, queueSize, stack, popped >>

infactBad2 == /\ pc["fuertethread"] = "infactBad2"
              /\ TRUE
              /\ pc' = [pc EXCEPT !["fuertethread"] = "failedWrite"]
              /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                              iocontext, stack, popped >>

readDone == /\ pc["fuertethread"] = "readDone"
            /\ IF /\ Len(iocontext) >= 1
                  /\ Head(iocontext) \in {"read", "readBAD"}
                  THEN /\ alarm' = "off"
                       /\ IF Head(iocontext) = "read" /\ state = "Connected"
                             THEN /\ \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "reallyGood3"]
                                     \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "infactBad3"]
                             ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "failed3"]
                  ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                       /\ alarm' = alarm
            /\ UNCHANGED << state, active, queueSize, asyncRunning, iocontext, 
                            stack, popped >>

failed3 == /\ pc["fuertethread"] = "failed3"
           /\ iocontext' = Tail(iocontext)
           /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "restartConnection",
                                                              pc        |->  "doneRead" ] >>
                                                          \o stack["fuertethread"]]
           /\ pc' = [pc EXCEPT !["fuertethread"] = "restartConnectionBegin"]
           /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                           popped >>

doneRead == /\ pc["fuertethread"] = "doneRead"
            /\ TRUE
            /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
            /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                            iocontext, stack, popped >>

reallyGood3 == /\ pc["fuertethread"] = "reallyGood3"
               /\ iocontext' = Tail(iocontext)
               /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "asyncWriteNextRequest",
                                                                  pc        |->  "doneRead",
                                                                  popped    |->  popped["fuertethread"] ] >>
                                                              \o stack["fuertethread"]]
               /\ popped' = [popped EXCEPT !["fuertethread"] = FALSE]
               /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncWriteNextRequestBegin"]
               /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning >>

infactBad3 == /\ pc["fuertethread"] = "infactBad3"
              /\ TRUE
              /\ pc' = [pc EXCEPT !["fuertethread"] = "failed3"]
              /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                              iocontext, stack, popped >>

alarmTriggered == /\ pc["fuertethread"] = "alarmTriggered"
                  /\ IF alarm /= "off"
                        THEN /\ iocontext' = Append(iocontext, alarm)
                             /\ alarm' = "off"
                        ELSE /\ TRUE
                             /\ UNCHANGED << alarm, iocontext >>
                  /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                  /\ UNCHANGED << state, active, queueSize, asyncRunning, 
                                  stack, popped >>

asyncFinished == /\ pc["fuertethread"] = "asyncFinished"
                 /\ IF asyncRunning /= "none"
                       THEN /\ iocontext' = Append(iocontext, asyncRunning)
                            /\ asyncRunning' = "none"
                       ELSE /\ TRUE
                            /\ UNCHANGED << asyncRunning, iocontext >>
                 /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                 /\ UNCHANGED << state, active, queueSize, alarm, stack, 
                                 popped >>

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
                      /\ UNCHANGED << state, active, queueSize, alarm, stack, 
                                      popped >>

handleOtherAlarm == /\ pc["fuertethread"] = "handleOtherAlarm"
                    /\ IF Len(iocontext) >= 1 /\ Head(iocontext) \in {"writeAlarm", "readAlarm", "idleAlarm"}
                          THEN /\ iocontext' = Tail(iocontext)
                               /\ IF active
                                     THEN /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "restartConnection",
                                                                                             pc        |->  "loop" ] >>
                                                                                         \o stack["fuertethread"]]
                                          /\ pc' = [pc EXCEPT !["fuertethread"] = "restartConnectionBegin"]
                                     ELSE /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "shutdownConnection",
                                                                                             pc        |->  "loop" ] >>
                                                                                         \o stack["fuertethread"]]
                                          /\ pc' = [pc EXCEPT !["fuertethread"] = "startShutdownConnection"]
                          ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                               /\ UNCHANGED << iocontext, stack >>
                    /\ UNCHANGED << state, active, queueSize, alarm, 
                                    asyncRunning, popped >>

cancellation == /\ pc["fuertethread"] = "cancellation"
                /\ IF /\ Len(iocontext) >= 1 /\ Head(iocontext) = "cancel"
                      THEN /\ iocontext' = Tail(iocontext)
                           /\ IF active
                                 THEN /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "restartConnection",
                                                                                         pc        |->  "drainQueue" ] >>
                                                                                     \o stack["fuertethread"]]
                                      /\ pc' = [pc EXCEPT !["fuertethread"] = "restartConnectionBegin"]
                                 ELSE /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "shutdownConnection",
                                                                                         pc        |->  "drainQueue" ] >>
                                                                                     \o stack["fuertethread"]]
                                      /\ pc' = [pc EXCEPT !["fuertethread"] = "startShutdownConnection"]
                      ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                           /\ UNCHANGED << iocontext, stack >>
                /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                                popped >>

drainQueue == /\ pc["fuertethread"] = "drainQueue"
              /\ queueSize' = 0
              /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
              /\ UNCHANGED << state, active, alarm, asyncRunning, iocontext, 
                              stack, popped >>

activation == /\ pc["fuertethread"] = "activation"
              /\ IF /\ Len(iocontext) >= 1 /\ Head(iocontext) = "activate"
                    THEN /\ iocontext' = Tail(iocontext)
                         /\ IF state = "Connected"
                               THEN /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "asyncWriteNextRequest",
                                                                                       pc        |->  "loop",
                                                                                       popped    |->  popped["fuertethread"] ] >>
                                                                                   \o stack["fuertethread"]]
                                    /\ popped' = [popped EXCEPT !["fuertethread"] = FALSE]
                                    /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncWriteNextRequestBegin"]
                                    /\ UNCHANGED << active, queueSize >>
                               ELSE /\ IF state = "Disconnected"
                                          THEN /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "startConnection",
                                                                                                  pc        |->  "loop" ] >>
                                                                                              \o stack["fuertethread"]]
                                               /\ pc' = [pc EXCEPT !["fuertethread"] = "startConnectionBegin"]
                                               /\ UNCHANGED << active, 
                                                               queueSize >>
                                          ELSE /\ IF state = "Failed"
                                                     THEN /\ queueSize' = 0
                                                          /\ active' = FALSE
                                                     ELSE /\ TRUE
                                                          /\ UNCHANGED << active, 
                                                                          queueSize >>
                                               /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                                               /\ stack' = stack
                                    /\ UNCHANGED popped
                    ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                         /\ UNCHANGED << active, queueSize, iocontext, stack, 
                                         popped >>
              /\ UNCHANGED << state, alarm, asyncRunning >>

fuerte == loop \/ connectDone \/ failed \/ doneConnect \/ reallyGood1
             \/ infactBad1 \/ writeDone \/ failedWrite \/ doneWrite
             \/ reallyGood2 \/ infactBad2 \/ readDone \/ failed3
             \/ doneRead \/ reallyGood3 \/ infactBad3 \/ alarmTriggered
             \/ asyncFinished \/ handleConnectAlarm \/ handleOtherAlarm
             \/ cancellation \/ drainQueue \/ activation

sendMessage == /\ pc["clientthread"] = "sendMessage"
               /\ queueSize < 2
               /\ pc' = [pc EXCEPT !["clientthread"] = "pushQueue"]
               /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                               iocontext, stack, popped >>

pushQueue == /\ pc["clientthread"] = "pushQueue"
             /\ IF state /= "Failed"
                   THEN /\ queueSize' = queueSize + 1
                   ELSE /\ TRUE
                        /\ UNCHANGED queueSize
             /\ pc' = [pc EXCEPT !["clientthread"] = "postActivate"]
             /\ UNCHANGED << state, active, alarm, asyncRunning, iocontext, 
                             stack, popped >>

postActivate == /\ pc["clientthread"] = "postActivate"
                /\ IF ~ active
                      THEN /\ active' = TRUE
                           /\ iocontext' = Append(iocontext, "activate")
                      ELSE /\ TRUE
                           /\ UNCHANGED << active, iocontext >>
                /\ pc' = [pc EXCEPT !["clientthread"] = "sendMessage"]
                /\ UNCHANGED << state, queueSize, alarm, asyncRunning, stack, 
                                popped >>

client == sendMessage \/ pushQueue \/ postActivate

cancelBegin == /\ pc["cancelthread"] = "cancelBegin"
               /\ state' = "Failed"
               /\ pc' = [pc EXCEPT !["cancelthread"] = "pushCancel"]
               /\ UNCHANGED << active, queueSize, alarm, asyncRunning, 
                               iocontext, stack, popped >>

pushCancel == /\ pc["cancelthread"] = "pushCancel"
              /\ iocontext' = Append(iocontext, "cancel")
              /\ pc' = [pc EXCEPT !["cancelthread"] = "Done"]
              /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                              stack, popped >>

cancel == cancelBegin \/ pushCancel

Next == fuerte \/ client \/ cancel
           \/ (\E self \in ProcSet:  \/ startConnection(self)
                                     \/ shutdownConnection(self)
                                     \/ restartConnection(self)
                                     \/ asyncWriteNextRequest(self)
                                     \/ startWriting(self))

Spec == Init /\ [][Next]_vars

\* END TRANSLATION

\* Invariants to prove:

TypeOK == /\ state \in {"Disconnected", "Connecting", "Connected", "Failed"}
          /\ active \in {FALSE, TRUE}
          /\ queueSize \in 0..2
          /\ alarm \in {"off", "connectAlarm", "writeAlarm", "readAlarm", "idleAlarm"}
          /\ asyncRunning \in {"none", "connect", "connectBAD", "write",
                               "writeBAD", "read", "readBAD"}
          /\ \/ Len(iocontext) = 0
             \/ Head(iocontext) \in {"connect", "connectBAD", "write", "writeBAD", "read",
                                     "readBAD", "connectAlarm", "writeAlarm", "readAlarm",
                                     "idleAlarm", "cancel", "activate"}
          /\ Len(iocontext) < 8
          
(* When we are active, something has to be ongoing, either an async call, or
   something scheduled on the iocontext. However, there is the tiny moment when
   we are just about to start the connection anew, or when we are about to start
   a write operation. *)
WorkingIfActive == active => \/ asyncRunning /= "none"
                             \/ Len(iocontext) > 0
                             \/ pc["fuertethread"] = "startConnectionBegin"
                             \/ pc["fuertethread"] = "asyncWriteNextRequestBegin"
                             \/ pc["fuertethread"] = "check1"
                             \/ pc["fuertethread"] = "deactivate"
                             \/ pc["fuertethread"] = "pop"
 
(* We basically want to either have an empty queue or want to be active.
   There are very tiny moments in time when both are violated, one is
   exactly when a new message is on the queue but the fuerte thread is not
   yet reactivated. The other is, if the check-set-check pattern is between
   the set and the recheck. These are the pc exceptions we have to have here: *)
NothingForgottenOnQueue == \/ queueSize = 0
                           \/ active
                           \/ pc["clientthread"] = "postActivate"
                           \/ pc["clientthread"] = "startConnectionBegin"
                           \/ pc["fuertethread"] = "check2"
                           \/ pc["fuertethread"] = "reactivate"
                           
\* NoSleepingBarber == /\ NothingForgottenOnQueue
\*                     /\ WorkingIfActive

SleepingBarberType1 == /\ queueSize > 0
                       /\ active = FALSE
                       /\ asyncRunning = "none"
                       /\ alarm = "off"
                       /\ iocontext = << >>
                       /\ pc["fuertethread"] = "loop"
                       /\ pc["clientthread"] = "sendMessage"
                       
SleepingBarberType2 == FALSE

NoSleepingBarber == /\ ~ SleepingBarberType1
                    /\ ~ SleepingBarberType2
=============================================================================
\* Modification History
\* Last modified Fri Jul 03 15:55:27 CEST 2020 by neunhoef
\* Created Mi 22. Apr 22:46:19 CEST 2020 by neunhoef
