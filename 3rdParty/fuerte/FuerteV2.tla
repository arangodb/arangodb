------------------------------ MODULE FuerteV2 ---------------------------
EXTENDS Integers, TLC, Sequences

(*
--algorithm H1Connection {

variables                       \* initial states VERIFIED
  state = "Disconnected",
  active = FALSE,
  queueSize = 0,
  alarm = "off",
  asyncRunning = "none",
  iocontext = << >>;

procedure startConnection() {   \* VERIFIED
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

procedure asyncWriteNextRequest() {    \* VERIFIED
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
      either activate:      \* VERIFIED
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "activate") {
          assert active /\ state /= "Connecting";
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
      
      or connectDone:          \* VERIFIED
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) \in {"connect", "connectBAD"}) {
          assert active /\ state \in {"Connecting", "Disconnecting"};
          alarm := "off";
          if (Head(iocontext) = "connect" /\ state = "Disconnecting") {
            iocontext := Tail(iocontext);
            state := "Connected";
            call asyncWriteNextRequest();
          } else {
            (* for the sake of simplicity we ignore the retries here and directly
               go to Failed state *)
            iocontext := Tail(iocontext);
            state := "Failed";
            active := FALSE;
            queueSize := 0;
          }; 
        };
      
      or writeDone:            \* VERIFIED
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) \in {"write", "writeBAD"}) {
          assert active /\ state \in {"Connected", "Disconnected"};
          if (Head(iocontext) = "write" /\ state = "Connected") {
            iocontext := Tail(iocontext);
            asyncRunning := "read";
          } else {
            iocontext := Tail(iocontext);
            alarm := "off";
            state := "Disconnected";
            call startConnection();
            \* this for now ignores the retry if nothing was written yet,
            \* this models that an error is returned to the client and we
            \* try to reconnect and execute the next request
          };
        };
      
      or readDone:              \* VERIFIED
        if (/\ Len(iocontext) >= 1
            /\ Head(iocontext) \in {"read", "readBAD"}) {
          assert active /\ state \in {"Connected", "Disconnected"}; 
          alarm := "off";
          if (Head(iocontext) = "read" /\ state = "Connected") {
            iocontext := Tail(iocontext);
            call asyncWriteNextRequest();
          } else {
            iocontext := Tail(iocontext);
            state := "Disconnected";
            call startConnection();
          };
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
            assert alarm = "off";
          }
        };
      
      or handleRequestAlarm:   \* VERIFIED
        if (Len(iocontext) >= 1 /\ Head(iocontext) = "requestAlarm") {
          iocontext := Tail(iocontext);
          state := "Disconnected";
          if (asyncRunning = "write") {
            asyncRunning := "writeBAD";
          };
          if (asyncRunning = "read") {
            makeReadBad:
              asyncRunning := "readBAD";
          };
          \* note that we might have been scheduled by an off-going
          \* alarm but the writeOK or writeBAD which wants to disarm the
          \* alarm was scheduled already earlier, in this case we must
          \* not interfere
        };
      
      or handleIdleAlarm:      \* VERIFIED
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
    while (TRUE) {            \* VERIFIED
        await queueSize < 4;
      pushQueue:
        queueSize := queueSize + 1;
      postActivate:
        if (~ active) {
          active := TRUE;
          iocontext := Append(iocontext, "activate");
        };
      };
    };
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
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "connectDone"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "writeDone"]
           \/ /\ pc' = [pc EXCEPT !["fuertethread"] = "readDone"]
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
                  THEN /\ Assert(active /\ state /= "Connecting", 
                                 "Failure of assertion at line 66, column 11.")
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

connectDone == /\ pc["fuertethread"] = "connectDone"
               /\ IF /\ Len(iocontext) >= 1
                     /\ Head(iocontext) \in {"connect", "connectBAD"}
                     THEN /\ Assert(active /\ state \in {"Connecting", "Disconnecting"}, 
                                    "Failure of assertion at line 81, column 11.")
                          /\ alarm' = "off"
                          /\ IF Head(iocontext) = "connect" /\ state = "Disconnecting"
                                THEN /\ iocontext' = Tail(iocontext)
                                     /\ state' = "Connected"
                                     /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "asyncWriteNextRequest",
                                                                                        pc        |->  "loop" ] >>
                                                                                    \o stack["fuertethread"]]
                                     /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncWriteNextRequestBegin"]
                                     /\ UNCHANGED << active, queueSize >>
                                ELSE /\ iocontext' = Tail(iocontext)
                                     /\ state' = "Failed"
                                     /\ active' = FALSE
                                     /\ queueSize' = 0
                                     /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                                     /\ stack' = stack
                     ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                          /\ UNCHANGED << state, active, queueSize, alarm, 
                                          iocontext, stack >>
               /\ UNCHANGED asyncRunning

writeDone == /\ pc["fuertethread"] = "writeDone"
             /\ IF /\ Len(iocontext) >= 1
                   /\ Head(iocontext) \in {"write", "writeBAD"}
                   THEN /\ Assert(active /\ state \in {"Connected", "Disconnected"}, 
                                  "Failure of assertion at line 100, column 11.")
                        /\ IF Head(iocontext) = "write" /\ state = "Connected"
                              THEN /\ iocontext' = Tail(iocontext)
                                   /\ asyncRunning' = "read"
                                   /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                                   /\ UNCHANGED << state, alarm, stack >>
                              ELSE /\ iocontext' = Tail(iocontext)
                                   /\ alarm' = "off"
                                   /\ state' = "Disconnected"
                                   /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "startConnection",
                                                                                      pc        |->  "loop" ] >>
                                                                                  \o stack["fuertethread"]]
                                   /\ pc' = [pc EXCEPT !["fuertethread"] = "startConnectionBegin"]
                                   /\ UNCHANGED asyncRunning
                   ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                        /\ UNCHANGED << state, alarm, asyncRunning, iocontext, 
                                        stack >>
             /\ UNCHANGED << active, queueSize >>

readDone == /\ pc["fuertethread"] = "readDone"
            /\ IF /\ Len(iocontext) >= 1
                  /\ Head(iocontext) \in {"read", "readBAD"}
                  THEN /\ Assert(active /\ state \in {"Connected", "Disconnected"}, 
                                 "Failure of assertion at line 118, column 11.")
                       /\ alarm' = "off"
                       /\ IF Head(iocontext) = "read" /\ state = "Connected"
                             THEN /\ iocontext' = Tail(iocontext)
                                  /\ stack' = [stack EXCEPT !["fuertethread"] = << [ procedure |->  "asyncWriteNextRequest",
                                                                                     pc        |->  "loop" ] >>
                                                                                 \o stack["fuertethread"]]
                                  /\ pc' = [pc EXCEPT !["fuertethread"] = "asyncWriteNextRequestBegin"]
                                  /\ state' = state
                             ELSE /\ iocontext' = Tail(iocontext)
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
                                 /\ IF asyncRunning = "connect"
                                       THEN /\ asyncRunning' = "connectBAD"
                                            /\ Assert(alarm = "off", 
                                                      "Failure of assertion at line 147, column 13.")
                                       ELSE /\ TRUE
                                            /\ UNCHANGED asyncRunning
                            ELSE /\ TRUE
                                 /\ UNCHANGED << asyncRunning, iocontext >>
                      /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                      /\ UNCHANGED << state, active, queueSize, alarm, stack >>

handleRequestAlarm == /\ pc["fuertethread"] = "handleRequestAlarm"
                      /\ IF Len(iocontext) >= 1 /\ Head(iocontext) = "requestAlarm"
                            THEN /\ iocontext' = Tail(iocontext)
                                 /\ state' = "Disconnected"
                                 /\ IF asyncRunning = "write"
                                       THEN /\ asyncRunning' = "writeBAD"
                                       ELSE /\ TRUE
                                            /\ UNCHANGED asyncRunning
                                 /\ IF asyncRunning' = "read"
                                       THEN /\ pc' = [pc EXCEPT !["fuertethread"] = "makeReadBad"]
                                       ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                            ELSE /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
                                 /\ UNCHANGED << state, asyncRunning, 
                                                 iocontext >>
                      /\ UNCHANGED << active, queueSize, alarm, stack >>

makeReadBad == /\ pc["fuertethread"] = "makeReadBad"
               /\ asyncRunning' = "readBAD"
               /\ pc' = [pc EXCEPT !["fuertethread"] = "loop"]
               /\ UNCHANGED << state, active, queueSize, alarm, iocontext, 
                               stack >>

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

fuerte == loop \/ activate \/ connectDone \/ writeDone \/ readDone
             \/ alarmTriggered \/ asyncFinished \/ handleConnectAlarm
             \/ handleRequestAlarm \/ makeReadBad \/ handleIdleAlarm

sendMessage == /\ pc["clientthread"] = "sendMessage"
               /\ queueSize < 4
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
          /\ asyncRunning \in {"none", "connect", "connectBAD", "write",
                               "writeBAD", "read", "readBAD"}

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
\* Last modified Fri Apr 24 12:39:28 CEST 2020 by neunhoef
\* Created Mi 22. Apr 22:46:19 CEST 2020 by neunhoef
