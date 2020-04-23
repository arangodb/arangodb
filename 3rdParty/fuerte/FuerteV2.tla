------------------------------ MODULE FuerteV2 ---------------------------
EXTENDS Integers, TLC, Sequences

(*
--algorithm H1Connection {

variables
  state = "Disconnected",
  active = FALSE,
  queueSize = 0,
  alarm = "off",
  asyncRunning = "none",
  iocontext = << >>;

procedure startConnection() {
 startConnectionBegin:
  assert /\ state = "Disconnected"
         /\ active
         /\ asyncRunning = "none"
         /\ alarm = "off";
  state := "Connecting";
  alarm := "connectAlarm";
  asyncRunning := "connect";
  return;
};

procedure asyncWriteNextRequest() {
 asyncWriteNextRequestBegin:
  assert /\ state = "Connected"
         /\ active
         /\ asyncRunning = "none";
  check1: { 
    if (queueSize = 0) {
      deactivate:
        active := FALSE;
      check2: {
        if (queueSize = 0) {
          alarm := "idleAlarm";
          return;
        };
        reactivate:
          if (active) {
            \* if somebody else has set active instead of us (after we have set it
            \* to false above), he is also responsible for starting the write.
            return;
          } else {
            active := TRUE;
          }
      }
    };
  };
  pop: {
    queueSize := queueSize - 1;
    alarm := "requestAlarm";
    asyncRunning := "write";
    return;
  }
};

  
process(fuerte = "fuertethread") {

  loop:
    while (TRUE) {
      either activate:
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "activate") {
          assert state /= "Connecting";
          iocontext := Tail(iocontext);
          if (state = "Disconnected") {
            call startConnection();
          } else if (state = "Connected") {
            call asyncWriteNextRequest();
          } else if (state = "Failed") {
            active := FALSE;
            queueSize := 0;
          };
        };
      
      or connectOK:
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) = "connect"
            /\ state = "Connecting") {
          \* if connect timeout happened, we have to continue with connectBAD
          iocontext := Tail(iocontext);
          assert state = "Connecting" /\ active;
          alarm := "off";
          state := "Connected";
          call asyncWriteNextRequest();
        };
      
      or connectBAD:
        (* for the sake of simplicity we ignore the retries here and directly
           go to Failed state *)
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "connect") {
          assert active;
          iocontext := Tail(iocontext);
          alarm := "off";
          state := "Failed";
          active := FALSE;
          queueSize := 0;
        };
      
      or writeOK:
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) = "write"
            /\ state = "Connected") {
          \* if connect timeout happened, we have to continue with writeBAD
          assert active;
          iocontext := Tail(iocontext);
          asyncRunning := "read";
        };
      
      or writeBAD:
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) = "write") {
          assert /\ state \in {"Connected", "Disconnected"}
                 /\ active
                 /\ alarm \in {"off", "requestAlarm"};
          iocontext := Tail(iocontext);
          alarm := "off";
          state := "Disconnected";
          call startConnection();
          \* this for now ignores the retry if nothing was written yet,
          \* this models that an error is returned to the client and we
          \* try to reconnect and execute the next request
        }; 
      
      or readOK:
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) = "read"
            /\ state = "Connected") {
          \* if connect timeout happened, we have to continue with readBAD
          assert active; 
          iocontext := Tail(iocontext);
          alarm := "off";
          call asyncWriteNextRequest();
        };
      
      or readBAD:
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) = "read") {
          assert /\ state \in {"Connected", "Disconnected"}
                 /\ active
                 /\ alarm \in {"off", "requestAlarm"};
          iocontext := Tail(iocontext);
          alarm := "off";
          state := "Disconnected";
          call startConnection();
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
          if (state = "Connecting") {
            assert alarm = "off";
            state := "Disconnected";    \* indicate error on socket
          }
        };
      
      or handleRequestAlarm:
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "requestAlarm") {
          iocontext := Tail(iocontext);
          if (state = "Connected") {
            state := "Disconnected";      \* indicate error on socket
            if (alarm /= "off") {
              \* it is possible that the idle alarm was still set, since we have
              \* gone to sleep and we have been posted on the iocontext beforehand
              alarm := "off";
            };
          }
          \* note that we might have been scheduled by an off-going alarm but
          \* the writeOK or writeBAD which wants to disarm the alarm was scheduled
          \* already earlier, in this case we must not interfere
        };
      
      or handleIdleAlarm:
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "idleAlarm") {
          iocontext := Tail(iocontext);
          if (state = "Connected") {
            state := "Disconnected";
            if (alarm /= "off") {
              \* it is possible that the idle alarm was still set, since we have
              \* gone to sleep and we have been posted on the iocontext beforehand
              alarm := "off";
            };
          }
        };
      
      or skip;
    }
};

process(client = "clientthread") {
  sendMessage: 
    while (TRUE) {
        await queueSize < 16;
      pushQueue:
        queueSize := queueSize + 1;
      postActivate:
        if (~ active) {
          active := TRUE;
          iocontext := Append(iocontext, "activate");
        }
    }
};

}

*)
\* BEGIN TRANSLATION
VARIABLES state, active, queueSize, alarm, asyncRunning, iocontext, pc, stack

vars == << state, active, queueSize, alarm, asyncRunning, iocontext, pc, 
           stack >>

ProcSet == {"fuertethread"} \cup {"clientthread"}

Init == (* Global variables *)
        /\ state = "Disconnected"
        /\ active = FALSE
        /\ queueSize = 0
        /\ alarm = "off"
        /\ asyncRunning = "none"
        /\ iocontext = << >>
        /\ stack = [self \in ProcSet |-> << >>]
        /\ pc = [self \in ProcSet |-> CASE self = "fuertethread" -> "loop"
                                        [] self = "clientthread" -> "sendMessage"]

startConnectionBegin(self) == /\ pc[self] = "startConnectionBegin"
                              /\ Assert(/\ state = "Disconnected"
                                        /\ active
                                        /\ asyncRunning = "none"
                                        /\ alarm = "off", 
                                        "Failure of assertion at line 17, column 3.")
                              /\ state' = "Connecting"
                              /\ alarm' = "connectAlarm"
                              /\ asyncRunning' = "connect"
                              /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                              /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                              /\ UNCHANGED << active, queueSize, iocontext >>

startConnection(self) == startConnectionBegin(self)

asyncWriteNextRequestBegin(self) == /\ pc[self] = "asyncWriteNextRequestBegin"
                                    /\ Assert(/\ state = "Connected"
                                              /\ active
                                              /\ asyncRunning = "none", 
                                              "Failure of assertion at line 29, column 3.")
                                    /\ pc' = [pc EXCEPT ![self] = "check1"]
                                    /\ UNCHANGED << state, active, queueSize, 
                                                    alarm, asyncRunning, 
                                                    iocontext, stack >>

check1(self) == /\ pc[self] = "check1"
                /\ IF queueSize = 0
                      THEN /\ pc' = [pc EXCEPT ![self] = "deactivate"]
                      ELSE /\ pc' = [pc EXCEPT ![self] = "pop"]
                /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                                iocontext, stack >>

deactivate(self) == /\ pc[self] = "deactivate"
                    /\ active' = FALSE
                    /\ pc' = [pc EXCEPT ![self] = "check2"]
                    /\ UNCHANGED << state, queueSize, alarm, asyncRunning, 
                                    iocontext, stack >>

check2(self) == /\ pc[self] = "check2"
                /\ IF queueSize = 0
                      THEN /\ alarm' = "idleAlarm"
                           /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                           /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                      ELSE /\ pc' = [pc EXCEPT ![self] = "reactivate"]
                           /\ UNCHANGED << alarm, stack >>
                /\ UNCHANGED << state, active, queueSize, asyncRunning, 
                                iocontext >>

reactivate(self) == /\ pc[self] = "reactivate"
                    /\ IF active
                          THEN /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
                               /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
                               /\ UNCHANGED active
                          ELSE /\ active' = TRUE
                               /\ pc' = [pc EXCEPT ![self] = "pop"]
                               /\ stack' = stack
                    /\ UNCHANGED << state, queueSize, alarm, asyncRunning, 
                                    iocontext >>

pop(self) == /\ pc[self] = "pop"
             /\ queueSize' = queueSize - 1
             /\ alarm' = "requestAlarm"
             /\ asyncRunning' = "write"
             /\ pc' = [pc EXCEPT ![self] = Head(stack[self]).pc]
             /\ stack' = [stack EXCEPT ![self] = Tail(stack[self])]
             /\ UNCHANGED << state, active, iocontext >>

asyncWriteNextRequest(self) == asyncWriteNextRequestBegin(self)
                                  \/ check1(self) \/ deactivate(self)
                                  \/ check2(self) \/ reactivate(self)
                                  \/ pop(self)

loop == /\ pc["fuertethread"] = "loop"
        /\ \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "activate"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "connectOK"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "connectBAD"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "writeOK"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "writeBAD"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "readOK"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "readBAD"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "alarmTriggered"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncFinished"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "handleConnectAlarm"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "handleRequestAlarm"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "handleIdleAlarm"]
           \/ /\ TRUE
              /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
        /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                        iocontext, stack >>

activate == /\ pc["fuertethread"] = "activate"
            /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "activate"
                  THEN /\ Assert(state /= "Connecting", 
                                 "Failure of assertion at line 67, column 11.")
                       /\ iocontext' = Tail(iocontext)
                       /\ IF state = "Disconnected"
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
                                        ELSE /\ IF state = "Failed"
                                                   THEN /\ active' = FALSE
                                                        /\ queueSize' = 0
                                                   ELSE /\ TRUE
                                                        /\ UNCHANGED << active, 
                                                                        queueSize >>
                                             /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                                             /\ stack' = stack
                  ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                       /\ UNCHANGED << active, queueSize, iocontext, stack >>
            /\ UNCHANGED << state, alarm, asyncRunning >>

connectOK == /\ pc["fuertethread"] = "connectOK"
             /\ IF /\ Len(iocontext) >= 1
                   /\ Head(iocontext) = "connect"
                   /\ state = "Connecting"
                   THEN /\ iocontext' = Tail(iocontext)
                        /\ Assert(state = "Connecting" /\ active, 
                                  "Failure of assertion at line 85, column 11.")
                        /\ alarm' = "off"
                        /\ state' = "Connected"
                        /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "asyncWriteNextRequest",
                                                                           pc        |->  "loop" ] >>
                                                                       \o stack["fuertethread"]]
                        /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncWriteNextRequestBegin"]
                   ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                        /\ UNCHANGED << state, alarm, iocontext, stack >>
             /\ UNCHANGED << active, queueSize, asyncRunning >>

connectBAD == /\ pc["fuertethread"] = "connectBAD"
              /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "connect"
                    THEN /\ Assert(active, 
                                   "Failure of assertion at line 95, column 11.")
                         /\ iocontext' = Tail(iocontext)
                         /\ alarm' = "off"
                         /\ state' = "Failed"
                         /\ active' = FALSE
                         /\ queueSize' = 0
                    ELSE /\ TRUE
                         /\ UNCHANGED << state, active, queueSize, alarm, 
                                         iocontext >>
              /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
              /\ UNCHANGED << asyncRunning, stack >>

writeOK == /\ pc["fuertethread"] = "writeOK"
           /\ IF /\ Len(iocontext) >= 1
                 /\ Head(iocontext) = "write"
                 /\ state = "Connected"
                 THEN /\ Assert(active, 
                                "Failure of assertion at line 108, column 11.")
                      /\ iocontext' = Tail(iocontext)
                      /\ asyncRunning' = "read"
                 ELSE /\ TRUE
                      /\ UNCHANGED << asyncRunning, iocontext >>
           /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
           /\ UNCHANGED << state, active, queueSize, alarm, stack >>

writeBAD == /\ pc["fuertethread"] = "writeBAD"
            /\ IF /\ Len(iocontext) >= 1
                  /\ Head(iocontext) = "write"
                  THEN /\ Assert(/\ state \in {"Connected", "Disconnected"}
                                 /\ active
                                 /\ alarm \in {"off", "requestAlarm"}, 
                                 "Failure of assertion at line 116, column 11.")
                       /\ iocontext' = Tail(iocontext)
                       /\ alarm' = "off"
                       /\ state' = "Disconnected"
                       /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "startConnection",
                                                                          pc        |->  "loop" ] >>
                                                                      \o stack["fuertethread"]]
                       /\ pc' = [pc EXCEPT !["fuertethread"] = "startConnectionBegin"]
                  ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                       /\ UNCHANGED << state, alarm, iocontext, stack >>
            /\ UNCHANGED << active, queueSize, asyncRunning >>

readOK == /\ pc["fuertethread"] = "readOK"
          /\ IF /\ Len(iocontext) >= 1
                /\ Head(iocontext) = "read"
                /\ state = "Connected"
                THEN /\ Assert(active, 
                               "Failure of assertion at line 133, column 11.")
                     /\ iocontext' = Tail(iocontext)
                     /\ alarm' = "off"
                     /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "asyncWriteNextRequest",
                                                                        pc        |->  "loop" ] >>
                                                                    \o stack["fuertethread"]]
                     /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncWriteNextRequestBegin"]
                ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                     /\ UNCHANGED << alarm, iocontext, stack >>
          /\ UNCHANGED << state, active, queueSize, asyncRunning >>

readBAD == /\ pc["fuertethread"] = "readBAD"
           /\ IF /\ Len(iocontext) >= 1
                 /\ Head(iocontext) = "read"
                 THEN /\ Assert(/\ state \in {"Connected", "Disconnected"}
                                /\ active
                                /\ alarm \in {"off", "requestAlarm"}, 
                                "Failure of assertion at line 142, column 11.")
                      /\ iocontext' = Tail(iocontext)
                      /\ alarm' = "off"
                      /\ state' = "Disconnected"
                      /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "startConnection",
                                                                         pc        |->  "loop" ] >>
                                                                     \o stack["fuertethread"]]
                      /\ pc' = [pc EXCEPT !["fuertethread"] = "startConnectionBegin"]
                 ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                      /\ UNCHANGED << state, alarm, iocontext, stack >>
           /\ UNCHANGED << active, queueSize, asyncRunning >>

alarmTriggered == /\ pc["fuertethread"] = "alarmTriggered"
                  /\ IF alarm /= "off"
                        THEN /\ iocontext' = Append(iocontext, alarm)
                             /\ alarm' = "off"
                        ELSE /\ TRUE
                             /\ UNCHANGED << alarm, iocontext >>
                  /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                  /\ UNCHANGED << state, active, queueSize, asyncRunning, 
                                  stack >>

asyncFinished == /\ pc["fuertethread"] = "asyncFinished"
                 /\ IF asyncRunning /= "none"
                       THEN /\ iocontext' = Append(iocontext, asyncRunning)
                            /\ asyncRunning' = "none"
                       ELSE /\ TRUE
                            /\ UNCHANGED << asyncRunning, iocontext >>
                 /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                 /\ UNCHANGED << state, active, queueSize, alarm, stack >>

handleConnectAlarm == /\ pc["fuertethread"] = "handleConnectAlarm"
                      /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "connectAlarm"
                            THEN /\ iocontext' = Tail(iocontext)
                                 /\ IF state = "Connecting"
                                       THEN /\ Assert(alarm = "off", 
                                                      "Failure of assertion at line 167, column 13.")
                                            /\ state' = "Disconnected"
                                       ELSE /\ TRUE
                                            /\ state' = state
                            ELSE /\ TRUE
                                 /\ UNCHANGED << state, iocontext >>
                      /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                      /\ UNCHANGED << active, queueSize, alarm, asyncRunning, 
                                      stack >>

handleRequestAlarm == /\ pc["fuertethread"] = "handleRequestAlarm"
                      /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "requestAlarm"
                            THEN /\ iocontext' = Tail(iocontext)
                                 /\ IF state = "Connected"
                                       THEN /\ state' = "Disconnected"
                                            /\ IF alarm /= "off"
                                                  THEN /\ alarm' = "off"
                                                  ELSE /\ TRUE
                                                       /\ alarm' = alarm
                                       ELSE /\ TRUE
                                            /\ UNCHANGED << state, alarm >>
                            ELSE /\ TRUE
                                 /\ UNCHANGED << state, alarm, iocontext >>
                      /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                      /\ UNCHANGED << active, queueSize, asyncRunning, stack >>

handleIdleAlarm == /\ pc["fuertethread"] = "handleIdleAlarm"
                   /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "idleAlarm"
                         THEN /\ iocontext' = Tail(iocontext)
                              /\ IF state = "Connected"
                                    THEN /\ state' = "Disconnected"
                                         /\ IF alarm /= "off"
                                               THEN /\ alarm' = "off"
                                               ELSE /\ TRUE
                                                    /\ alarm' = alarm
                                    ELSE /\ TRUE
                                         /\ UNCHANGED << state, alarm >>
                         ELSE /\ TRUE
                              /\ UNCHANGED << state, alarm, iocontext >>
                   /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                   /\ UNCHANGED << active, queueSize, asyncRunning, stack >>

fuerte == loop \/ activate \/ connectOK \/ connectBAD \/ writeOK
             \/ writeBAD \/ readOK \/ readBAD \/ alarmTriggered
             \/ asyncFinished \/ handleConnectAlarm \/ handleRequestAlarm
             \/ handleIdleAlarm

sendMessage == /\ pc["clientthread"] = "sendMessage"
               /\ queueSize < 16
               /\ pc' = [pc EXCEPT !["clientthread"] = "pushQueue"]
               /\ UNCHANGED << state, active, queueSize, alarm, asyncRunning, 
                               iocontext, stack >>

pushQueue == /\ pc["clientthread"] = "pushQueue"
             /\ queueSize' = queueSize + 1
             /\ pc' = [pc EXCEPT !["clientthread"] = "postActivate"]
             /\ UNCHANGED << state, active, alarm, asyncRunning, iocontext, 
                             stack >>

postActivate == /\ pc["clientthread"] = "postActivate"
                /\ IF ~ active
                      THEN /\ active' = TRUE
                           /\ iocontext' = Append(iocontext, "activate")
                      ELSE /\ TRUE
                           /\ UNCHANGED << active, iocontext >>
                /\ pc' = [pc EXCEPT !["clientthread"] = "sendMessage"]
                /\ UNCHANGED << state, queueSize, alarm, asyncRunning, stack >>

client == sendMessage \/ pushQueue \/ postActivate

Next == fuerte \/ client
           \/ (\E self \in ProcSet:  \/ startConnection(self)
                                     \/ asyncWriteNextRequest(self))

Spec == Init /\ [][Next]_vars

\* END TRANSLATION

\* Invariants to prove:

TypeOK == /\ state \in {"Disconnected", "Connecting", "Connected", "Failed"}
          /\ active \in {FALSE, TRUE}
          /\ queueSize \in 0..16
          /\ alarm \in {"off", "connectAlarm", "requestAlarm", "idleAlarm"}
          /\ asyncRunning \in {"none", "connect", "write", "read"}

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
                             \*   \/ alarm /= "none"       \* needed? wanted?
    
(* We basically want to either have an empty queue or want to be active. There
   are very tiny moments in time when both are violated, one is exactly when
   a new message is on the queue but the fuerte thread is not yet reactivated.
   The other is, if the check-set-check pattern is between the set and the recheck.
   These are the pc exceptions we have to have here: *)                         
NothingForgottenOnQueue == \/ queueSize = 0
                           \/ active
                           \/ pc["clientthread"] = "postActivate"
                           \/ pc["fuertethread"] = "check2"
                           \/ pc["fuertethread"] = "reactivate"
                           
NoSleepingBarber == /\ NothingForgottenOnQueue
                    /\ WorkingIfActive


=============================================================================
\* Modification History
\* Last modified Thu Apr 23 16:03:24 CEST 2020 by neunhoef
\* Created Mi 22. Apr 22:46:19 CEST 2020 by neunhoef
