{
License Agreement

This content is subject to the Mozilla Public License Version 1.1 (the "License");
You may not use this plugin except in compliance with the License. You may
obtain a copy of the License at http://www.mozilla.org/MPL.

Alternatively, you may redistribute this library, use and/or modify it
under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation; either version 2.1 of the License,
or (at your option) any later version. You may obtain a copy
of the LGPL at www.gnu.org/copyleft.

Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the License.

The original code is ServiceControl.pas, released April 16, 2007.

The initial developer of the original code is Rainer Budde (http://www.speed-soft.de).

SimpleSC - NSIS Service Control Plugin is written, published and maintaned by
Rainer Budde (rainer@speed-soft.de).
}
unit ServiceControl;

interface

uses
  Windows, SysUtils, WinSvc, DateUtils;

  function InstallService(ServiceName, DisplayName: String; ServiceType: DWORD; StartType: DWORD; BinaryPathName: String; Dependencies: String; Username: String; Password: String): Integer;
  function RemoveService(ServiceName: String): Integer;
  function GetServiceName(DisplayName: String; var Name: String): Integer;
  function GetServiceDisplayName(ServiceName: String; var Name: String): Integer;
  function GetServiceStatus(ServiceName: String; var Status: DWORD): Integer;
  function GetServiceBinaryPath(ServiceName: String; var BinaryPath: String): Integer;
  function GetServiceStartType(ServiceName: String; var StartType: DWORD): Integer;
  function GetServiceDescription(ServiceName: String; var Description: String): Integer;
  function GetServiceLogon(ServiceName: String; var Username: String): Integer;
  function GetServiceFailure(ServiceName: String; var ResetPeriod: DWORD; var RebootMessage: String; var Command: String; var Action1: Integer; var ActionDelay1: DWORD; var Action2: Integer; var ActionDelay2: DWORD; var Action3: Integer; var ActionDelay3: DWORD): Integer;
  function GetServiceFailureFlag(ServiceName: String; var FailureActionsOnNonCrashFailures: Boolean): Integer;
  function GetServiceDelayedAutoStartInfo(ServiceName: String; var DelayedAutostart: Boolean): Integer;
  function SetServiceStartType(ServiceName: String; StartType: DWORD): Integer;
  function SetServiceDescription(ServiceName: String; Description: String): Integer;
  function SetServiceLogon(ServiceName: String; Username: String; Password: String): Integer;
  function SetServiceBinaryPath(ServiceName: String; BinaryPath: String): Integer;
  function SetServiceFailure(ServiceName: String; ResetPeriod: DWORD; RebootMessage: String; Command: String; Action1: Integer; ActionDelay1: DWORD; Action2: Integer; ActionDelay2: DWORD; Action3: Integer; ActionDelay3: DWORD): Integer;
  function SetServiceFailureFlag(ServiceName: String; FailureActionsOnNonCrashFailures: Boolean): Integer;
  function SetServiceDelayedAutoStartInfo(ServiceName: String; DelayedAutostart: Boolean): Integer;
  function ServiceIsRunning(ServiceName: String; var IsRunning: Boolean): Integer;
  function ServiceIsStopped(ServiceName: String; var IsStopped: Boolean): Integer;
  function ServiceIsPaused(ServiceName: String; var IsPaused: Boolean): Integer;
  function StartService(ServiceName: String; ServiceArguments: String; Timeout: Integer): Integer;
  function StopService(ServiceName: String; WaitForFileRelease: Boolean; Timeout: Integer): Integer;
  function PauseService(ServiceName: String; Timeout: Integer): Integer;
  function ContinueService(ServiceName: String; Timeout: Integer): Integer;
  function RestartService(ServiceName: String; ServiceArguments: String; Timeout: Integer): Integer;
  function ExistsService(ServiceName: String): Integer;
  function GetErrorMessage(ErrorCode: Integer): String;
  function WaitForFileRelease(ServiceName: String; Timeout: Integer): Integer;
  function WaitForStatus(ServiceName: String; Status: DWord; Timeout: Integer): Integer;

implementation

function WaitForFileRelease(ServiceName: String; Timeout: Integer): Integer;

  function GetFilename(ServiceFileName: String): String;
  var
    FilePath: String;
    FileName: String;
  const
    ParameterDelimiter = ' ';
  begin
    FilePath := ExtractFilePath(ServiceFileName);

    FileName := Copy(ServiceFileName, Length(FilePath) + 1, Length(ServiceFileName) - Length(FilePath));

    if Pos(ParameterDelimiter, Filename) <> 0 then
      FileName := Copy(FileName, 0, Pos(ParameterDelimiter, Filename) - Length(ParameterDelimiter));

    Result := FilePath + FileName;
  end;

var
  StatusReached: Boolean;
  TimeOutReached: Boolean;
  TimeoutDate: TDateTime;
  ServiceResult: Integer;
  ServiceFileName: String;
  FileName: String;
  FileHandle: Cardinal;
const
  WAIT_TIMEOUT = 250;
begin
  Result := 0;

  StatusReached := False;
  TimeOutReached := False;

  ServiceResult := GetServiceBinaryPath(ServiceName, ServiceFileName);

  if ServiceResult = 0 then
  begin

    Filename := GetFilename(ServiceFileName);

    if FileExists(FileName) then
    begin
      TimeoutDate := IncSecond(Now, Timeout);

      while not StatusReached and not TimeOutReached do
      begin
        FileHandle := CreateFile(PChar(FileName), GENERIC_READ or GENERIC_WRITE, 0,
                                 nil, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

        if FileHandle <> INVALID_HANDLE_VALUE then
        begin
          CloseHandle(FileHandle);
          StatusReached := True;
        end;

        if not StatusReached and (TimeoutDate < Now) then
        begin
          TimeOutReached := True;
          Result := Windows.WAIT_TIMEOUT;
        end;
      end;

    end;

  end
  else
    Result := ServiceResult;

end;

function WaitForStatus(ServiceName: String; Status: DWord; Timeout: Integer): Integer;
var
  CurrentStatus: DWord;
  StatusResult: Integer;
  StatusReached: Boolean;
  TimeOutReached: Boolean;
  ErrorOccured: Boolean;
  TimeoutDate: TDateTime;
const
  WAIT_TIMEOUT = 250;
begin
  Result := 0;

  StatusReached := False;
  TimeOutReached := False;
  ErrorOccured := False;

  TimeoutDate := IncSecond(Now, Timeout);

  while not StatusReached and not ErrorOccured and not TimeOutReached do
  begin
    StatusResult := GetServiceStatus(ServiceName, CurrentStatus);

    if StatusResult = 0 then
    begin
      if Status = CurrentStatus then
        StatusReached := True
      else
        Sleep(WAIT_TIMEOUT);
    end
    else
    begin
      ErrorOccured := True;
      Result := StatusResult;
    end;

    if not StatusReached and not ErrorOccured and (TimeoutDate < Now) then
    begin
      TimeOutReached := True;
      Result := ERROR_SERVICE_REQUEST_TIMEOUT;
    end;
  end;

end;

function ExistsService(ServiceName: String): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_QUERY_CONFIG);

    if ServiceHandle > 0 then
      CloseServiceHandle(ServiceHandle)
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function StartService(ServiceName: String; ServiceArguments: String; Timeout: Integer): Integer;
type
  TArguments = Array of PChar;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  ServiceArgVectors: TArguments;
  NumServiceArgs: DWORD;
const
  ArgDelimitterQuote: String = '"';
  ArgDelimitterWhiteSpace: String = ' ';

  procedure GetServiceArguments(ServiceArguments: String; var NumServiceArgs: DWORD; var ServiceArgVectors: TArguments);
  var
    Param: String;
    Split: Boolean;
    Quoted: Boolean;
    CharIsDelimitter: Boolean;
  begin
    ServiceArgVectors := nil;
    NumServiceArgs := 0;

    Quoted := False;

    while Length(ServiceArguments) > 0 do
    begin
      Split := False;
      CharIsDelimitter := False;

      if ServiceArguments[1] = ' ' then
        if not Quoted then
        begin
          CharIsDelimitter := True;
          Split := True;
        end;

      if ServiceArguments[1] = '"' then
      begin
        Quoted := not Quoted;
        CharIsDelimitter := True;

        if not Quoted then
          Split := True;
      end;

      if not CharIsDelimitter then
        Param := Param + ServiceArguments[1];

      if Split or (Length(ServiceArguments) = 1) then
      begin
        SetLength(ServiceArgVectors, Length(ServiceArgVectors) + 1);
        GetMem(ServiceArgVectors[Length(ServiceArgVectors) -1], Length(Param) + 1);
        StrPCopy(ServiceArgVectors[Length(ServiceArgVectors) -1], Param);

        Param := '';

        Delete(ServiceArguments, 1, 1);
        ServiceArguments := Trim(ServiceArguments);
      end
      else
        Delete(ServiceArguments, 1, 1);

    end;

    if Length(ServiceArgVectors) > 0 then
      NumServiceArgs := Length(ServiceArgVectors);
  end;

  procedure FreeServiceArguments(ServiceArgVectors: TArguments);
  var
    i: Integer;
  begin
    if Length(ServiceArgVectors) > 0 then
      for i := 0 to Length(ServiceArgVectors) -1 do
        FreeMem(ServiceArgVectors[i]);
  end;

begin
  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_START);

    if ServiceHandle > 0 then
    begin
      GetServiceArguments(ServiceArguments, NumServiceArgs, ServiceArgVectors);

      if WinSvc.StartService(ServiceHandle, NumServiceArgs, ServiceArgVectors[0]) then
        Result := WaitForStatus(ServiceName, SERVICE_RUNNING, Timeout)
      else
        Result := System.GetLastError;

      FreeServiceArguments(ServiceArgVectors);

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;


    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function StopService(ServiceName: String; WaitForFileRelease: Boolean; Timeout: Integer): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  ServiceStatus: TServiceStatus;
  Dependencies: PEnumServiceStatus;
  BytesNeeded: Cardinal;
  ServicesReturned: Cardinal;
  ServicesEnumerated: Boolean;
  EnumerationSuccess: Boolean;
  i: Cardinal;
begin
  Result := 0;

  BytesNeeded := 0;
  ServicesReturned := 0;

  Dependencies := nil;
  ServicesEnumerated := False;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT or SC_MANAGER_ENUMERATE_SERVICE);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_STOP or SERVICE_ENUMERATE_DEPENDENTS);

    if ServiceHandle > 0 then
    begin
      if not EnumDependentServices(ServiceHandle, SERVICE_ACTIVE, Dependencies^, 0, BytesNeeded, ServicesReturned) then
      begin
        ServicesEnumerated := True;
        GetMem(Dependencies, BytesNeeded);

        EnumerationSuccess := EnumDependentServices(ServiceHandle, SERVICE_ACTIVE, Dependencies^, BytesNeeded, BytesNeeded, ServicesReturned);

        if EnumerationSuccess and (ServicesReturned > 0) then
        begin
          for i := 1 to ServicesReturned do
          begin
            Result := StopService(Dependencies.lpServiceName, False, Timeout);

            if Result <> 0 then
              Break;

            Inc(Dependencies);
          end;
        end
        else
          Result := System.GetLastError;
      end;

      if (ServicesEnumerated and (Result = 0)) or not ServicesEnumerated then
      begin
        if ControlService(ServiceHandle, SERVICE_CONTROL_STOP, ServiceStatus) then
          Result := WaitForStatus(ServiceName, SERVICE_STOPPED, Timeout)
        else
          Result := System.GetLastError
      end;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;

  if (Result = 0) and WaitForFileRelease then
    Result := ServiceControl.WaitForFileRelease(ServiceName, Timeout);
end;

function PauseService(ServiceName: String; Timeout: Integer): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  ServiceStatus: TServiceStatus;
begin
  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_PAUSE_CONTINUE);

    if ServiceHandle > 0 then
    begin

      if ControlService(ServiceHandle, SERVICE_CONTROL_PAUSE, ServiceStatus) then
        Result := WaitForStatus(ServiceName, SERVICE_PAUSED, Timeout)
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function ContinueService(ServiceName: String; Timeout: Integer): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  ServiceStatus: TServiceStatus;
begin
  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_PAUSE_CONTINUE);

    if ServiceHandle > 0 then
    begin

      if ControlService(ServiceHandle, SERVICE_CONTROL_CONTINUE, ServiceStatus) then
        Result := WaitForStatus(ServiceName, SERVICE_RUNNING, Timeout)
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function GetServiceName(DisplayName: String; var Name: String): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceName: PChar;
  ServiceBuffer: Cardinal;
begin
  Result := 0;

  ServiceBuffer := 255;
  ServiceName := StrAlloc(ServiceBuffer+1);

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    if WinSvc.GetServiceKeyName(ManagerHandle, PChar(DisplayName), ServiceName, ServiceBuffer) then
      Name := ServiceName
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function GetServiceDisplayName(ServiceName: String; var Name: String): Integer;
var
  ManagerHandle: SC_HANDLE;
  DisplayName: PChar;
  ServiceBuffer: Cardinal;
begin
  Result := 0;

  ServiceBuffer := 255;
  DisplayName := StrAlloc(ServiceBuffer+1);

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    if WinSvc.GetServiceDisplayName(ManagerHandle, PChar(ServiceName), DisplayName, ServiceBuffer) then
      Name := DisplayName
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function GetServiceStatus(ServiceName: String; var Status: DWORD): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  ServiceStatus: TServiceStatus;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_QUERY_STATUS);

    if ServiceHandle > 0 then
    begin
      if QueryServiceStatus(ServiceHandle, ServiceStatus) then
        Status := ServiceStatus.dwCurrentState
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function GetServiceBinaryPath(ServiceName: String; var BinaryPath: String): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  BytesNeeded: DWORD;
  ServiceConfig: PQueryServiceConfig;
begin
  Result := 0;
  ServiceConfig := nil;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_QUERY_CONFIG);

    if ServiceHandle > 0 then
    begin

      if not QueryServiceConfig(ServiceHandle, ServiceConfig, 0, BytesNeeded) and (System.GetLastError = ERROR_INSUFFICIENT_BUFFER) then
      begin
        GetMem(ServiceConfig, BytesNeeded);

        if QueryServiceConfig(ServiceHandle, ServiceConfig, BytesNeeded, BytesNeeded) then
          BinaryPath := ServiceConfig^.lpBinaryPathName
        else
          Result := System.GetLastError;

        FreeMem(ServiceConfig);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function GetServiceStartType(ServiceName: String; var StartType: DWORD): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  BytesNeeded: DWORD;
  ServiceConfig: PQueryServiceConfig;
begin
  Result := 0;
  ServiceConfig := nil;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_QUERY_CONFIG);

    if ServiceHandle > 0 then
    begin

      if not QueryServiceConfig(ServiceHandle, ServiceConfig, 0, BytesNeeded) and (System.GetLastError = ERROR_INSUFFICIENT_BUFFER) then
      begin
        GetMem(ServiceConfig, BytesNeeded);

        if QueryServiceConfig(ServiceHandle, ServiceConfig, BytesNeeded, BytesNeeded) then
          StartType := ServiceConfig^.dwStartType
        else
          Result := System.GetLastError;

        FreeMem(ServiceConfig);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function GetServiceDescription(ServiceName: String; var Description: String): Integer;
const
  SERVICE_CONFIG_DESCRIPTION = 1;
type
  TServiceDescription = record
    lpDescription: PAnsiChar;
  end;
  PServiceDescription = ^TServiceDescription;
var
  QueryServiceConfig2: function(hService: SC_HANDLE; dwInfoLevel: DWORD; pBuffer: Pointer; cbBufSize: DWORD; var cbBytesNeeded: Cardinal): BOOL; stdcall;
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
  ServiceDescription: PServiceDescription;
  BytesNeeded: Cardinal;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_QUERY_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        @QueryServiceConfig2 := GetProcAddress(GetModuleHandle(advapi32), 'QueryServiceConfig2A');

        if Assigned(@QueryServiceConfig2) then
        begin

          if not QueryServiceConfig2(ServiceHandle, SERVICE_CONFIG_DESCRIPTION, nil, 0, BytesNeeded) and (System.GetLastError = ERROR_INSUFFICIENT_BUFFER) then
          begin
            GetMem(ServiceDescription, BytesNeeded);

            if QueryServiceConfig2(ServiceHandle, SERVICE_CONFIG_DESCRIPTION, ServiceDescription, BytesNeeded, BytesNeeded) then
              Description := ServiceDescription.lpDescription
            else
              Result := System.GetLastError;

            FreeMem(ServiceDescription);
          end
          else
            Result := System.GetLastError;

        end
        else
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function GetServiceLogon(ServiceName: String; var Username: String): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  BytesNeeded: DWORD;
  ServiceConfig: PQueryServiceConfig;
begin
  Result := 0;
  ServiceConfig := nil;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_CONNECT);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_QUERY_CONFIG);

    if ServiceHandle > 0 then
    begin

      if not QueryServiceConfig(ServiceHandle, ServiceConfig, 0, BytesNeeded) and (System.GetLastError = ERROR_INSUFFICIENT_BUFFER) then
      begin
        GetMem(ServiceConfig, BytesNeeded);

        if QueryServiceConfig(ServiceHandle, ServiceConfig, BytesNeeded, BytesNeeded) then
          Username := ServiceConfig^.lpServiceStartName
        else
          Result := System.GetLastError;

        FreeMem(ServiceConfig);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function GetServiceFailure(ServiceName: String; var ResetPeriod: DWORD;
  var RebootMessage: String; var Command: String; var Action1: Integer; var ActionDelay1: DWORD;
  var Action2: Integer; var ActionDelay2: DWORD; var Action3: Integer; var ActionDelay3: DWORD): Integer;
const
  SERVICE_CONFIG_FAILURE_ACTIONS = 2;
type
  TServiceAction = record
    aType: DWORD;
    Delay: DWORD;
  end;

  TServiceActions = array[0..2] of TServiceAction;

  TServiceFailureAction = record
    dwResetPeriod: DWORD;
    lpRebootMsg: PWideChar;
    lpCommand: PWideChar;
    cActions: DWORD;
    lpsaActions: ^TServiceActions;
  end;
  PServiceFailureAction = ^TServiceFailureAction;
var
  QueryServiceConfig2: function(hService: SC_HANDLE; dwInfoLevel: DWORD; pBuffer: Pointer; cbBufSize: DWORD; var cbBytesNeeded: Cardinal): BOOL; stdcall;
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
  ServiceFailureAction: PServiceFailureAction;
  BytesNeeded: Cardinal;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_QUERY_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        // Unicode must be used because the ANSI function returns an invalid buffer
        @QueryServiceConfig2 := GetProcAddress(GetModuleHandle(advapi32), 'QueryServiceConfig2W');

        if Assigned(@QueryServiceConfig2) then
        begin

          if not QueryServiceConfig2(ServiceHandle, SERVICE_CONFIG_FAILURE_ACTIONS, nil, 0, BytesNeeded) and (System.GetLastError = ERROR_INSUFFICIENT_BUFFER) then
          begin
            GetMem(ServiceFailureAction, BytesNeeded);

            if QueryServiceConfig2(ServiceHandle, SERVICE_CONFIG_FAILURE_ACTIONS, ServiceFailureAction, BytesNeeded, BytesNeeded) then
            begin
              ResetPeriod := ServiceFailureAction.dwResetPeriod;
              RebootMessage := ServiceFailureAction.lpRebootMsg;
              Command :=ServiceFailureAction.lpCommand;

              if ServiceFailureAction.cActions >= 1 then
              begin
                Action1 := ServiceFailureAction.lpsaActions[0].aType;
                ActionDelay1 := ServiceFailureAction.lpsaActions[0].Delay;
              end;

              if ServiceFailureAction.cActions >= 2 then
              begin
                Action2 := ServiceFailureAction.lpsaActions[1].aType;
                ActionDelay2 := ServiceFailureAction.lpsaActions[1].Delay;
              end;

              if ServiceFailureAction.cActions >= 3 then
              begin
                Action3 := ServiceFailureAction.lpsaActions[2].aType;
                ActionDelay3 := ServiceFailureAction.lpsaActions[2].Delay;
              end;
            end
            else
              Result := System.GetLastError;

            FreeMem(ServiceFailureAction);
          end
          else
            Result := System.GetLastError;

        end
        else
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;

end;

function GetServiceFailureFlag(ServiceName: String; var FailureActionsOnNonCrashFailures: Boolean): Integer;
const
  SERVICE_CONFIG_FAILURE_ACTIONS_FLAG = 4;
type

  TServiceFailureActionsFlag = record
    fFailureActionsOnNonCrashFailures: BOOL;
  end;
  PServiceFailureActionsFlag = ^TServiceFailureActionsFlag;

var
  QueryServiceConfig2: function(hService: SC_HANDLE; dwInfoLevel: DWORD; pBuffer: Pointer; cbBufSize: DWORD; var cbBytesNeeded: Cardinal): BOOL; stdcall;
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
  ServiceFailureActionsFlag: PServiceFailureActionsFlag;
  BytesNeeded: Cardinal;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_QUERY_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        // Unicode must be used because the ANSI function returns an invalid buffer
        @QueryServiceConfig2 := GetProcAddress(GetModuleHandle(advapi32), 'QueryServiceConfig2W');

        if Assigned(@QueryServiceConfig2) then
        begin

          if not QueryServiceConfig2(ServiceHandle, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, nil, 0, BytesNeeded) and (System.GetLastError = ERROR_INSUFFICIENT_BUFFER) then
          begin
            GetMem(ServiceFailureActionsFlag, BytesNeeded);

            if QueryServiceConfig2(ServiceHandle, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, ServiceFailureActionsFlag, BytesNeeded, BytesNeeded) then
              FailureActionsOnNonCrashFailures := Boolean(ServiceFailureActionsFlag.fFailureActionsOnNonCrashFailures)
            else
              Result := System.GetLastError;

            FreeMem(ServiceFailureActionsFlag);
          end
          else
            Result := System.GetLastError;

        end
        else
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;

end;

function GetServiceDelayedAutoStartInfo(ServiceName: String; var DelayedAutostart: Boolean): Integer;
const
  SERVICE_CONFIG_DELAYED_AUTO_START_INFO = 3;
type

  TServiceDelayedAutoStartInfo = record
    fDelayedAutostart: BOOL;
  end;
  PServiceDelayedAutoStartInfo = ^TServiceDelayedAutoStartInfo;

var
  QueryServiceConfig2: function(hService: SC_HANDLE; dwInfoLevel: DWORD; pBuffer: Pointer; cbBufSize: DWORD; var cbBytesNeeded: Cardinal): BOOL; stdcall;
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
  ServiceDelayedAutoStartInfo: PServiceDelayedAutoStartInfo;
  BytesNeeded: Cardinal;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_QUERY_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        // Unicode must be used because the ANSI function returns an invalid buffer
        @QueryServiceConfig2 := GetProcAddress(GetModuleHandle(advapi32), 'QueryServiceConfig2W');

        if Assigned(@QueryServiceConfig2) then
        begin

          if not QueryServiceConfig2(ServiceHandle, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, nil, 0, BytesNeeded) and (System.GetLastError = ERROR_INSUFFICIENT_BUFFER) then
          begin
            GetMem(ServiceDelayedAutoStartInfo, BytesNeeded);

            if QueryServiceConfig2(ServiceHandle, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, ServiceDelayedAutoStartInfo, BytesNeeded, BytesNeeded) then
              DelayedAutostart := Boolean(ServiceDelayedAutoStartInfo.fDelayedAutostart)
            else
              Result := System.GetLastError;

            FreeMem(ServiceDelayedAutoStartInfo);
          end
          else
            Result := System.GetLastError;

        end
        else
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;

end;
    
function SetServiceDescription(ServiceName: String; Description: String): Integer;
const
  SERVICE_CONFIG_DESCRIPTION = 1;
var
  ChangeServiceConfig2: function(hService: SC_HANDLE; dwInfoLevel: DWORD; lpInfo: Pointer): BOOL; stdcall;
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_CHANGE_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        @ChangeServiceConfig2 := GetProcAddress(GetModuleHandle(advapi32), 'ChangeServiceConfig2A');

        if Assigned(@ChangeServiceConfig2) then
        begin
          if not ChangeServiceConfig2(ServiceHandle, SERVICE_CONFIG_DESCRIPTION, @Description) then
            Result := System.GetLastError;
        end
        else
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function SetServiceStartType(ServiceName: String; StartType: DWORD): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_CHANGE_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        if not ChangeServiceConfig(ServiceHandle, SERVICE_NO_CHANGE, StartType, SERVICE_NO_CHANGE, nil, nil, nil, nil, nil, nil, nil) then
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function SetServiceLogon(ServiceName: String; Username: String; Password: String): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if Pos('\', Username) = 0 then
    Username := '.\' + Username;

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_CHANGE_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        if not ChangeServiceConfig(ServiceHandle, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, nil, nil, nil, nil, PChar(Username), PChar(Password), nil) then
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function SetServiceBinaryPath(ServiceName: String; BinaryPath: String): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
begin
  Result := 0;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_CHANGE_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        if not ChangeServiceConfig(ServiceHandle, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, PChar(BinaryPath), nil, nil, nil, nil, nil, nil) then
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function SetServiceFailure(ServiceName: String; ResetPeriod: DWORD;
  RebootMessage: String; Command: String; Action1: Integer; ActionDelay1: DWORD;
  Action2: Integer; ActionDelay2: DWORD; Action3: Integer; ActionDelay3: DWORD): Integer;
const
  SERVICE_CONFIG_FAILURE_ACTIONS = 2;
  SC_ACTION_RESTART = 1;
type

  TServiceAction = record
    aType: DWORD;
    Delay: DWORD;
  end;

  TServiceActions = array[0..2] of TServiceAction;

  TServiceFailureAction = record
    dwResetPeriod: DWORD;
    lpRebootMsg: PAnsiChar;
    lpCommand: PAnsiChar;
    cActions: DWORD;
    lpsaActions: ^TServiceActions;
  end;

var
  ChangeServiceConfig2: function(hService: SC_HANDLE; dwInfoLevel: DWORD; lpInfo: Pointer): BOOL; stdcall;
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
  ServiceFailureAction: TServiceFailureAction;
  ServiceActions: TServiceActions;
  ServiceAccessType: Integer;
begin
  Result := 0;

  if (Action1 = SC_ACTION_RESTART) or (Action2 = SC_ACTION_RESTART) or (Action3 = SC_ACTION_RESTART) then
    ServiceAccessType := SERVICE_CHANGE_CONFIG or SERVICE_START
  else
    ServiceAccessType := SERVICE_CHANGE_CONFIG;

  ServiceActions[0].aType := Action1;
  ServiceActions[0].Delay := ActionDelay1;
  ServiceActions[1].aType := Action2;
  ServiceActions[1].Delay := ActionDelay2;
  ServiceActions[2].aType := Action3;
  ServiceActions[2].Delay := ActionDelay3;

  ServiceFailureAction.dwResetPeriod := ResetPeriod;
  ServiceFailureAction.lpRebootMsg := PAnsiChar(RebootMessage);
  ServiceFailureAction.lpCommand := PAnsiChar(Command);
  ServiceFailureAction.cActions := Length(ServiceActions);
  ServiceFailureAction.lpsaActions := @ServiceActions;

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), ServiceAccessType);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        @ChangeServiceConfig2 := GetProcAddress(GetModuleHandle(advapi32), 'ChangeServiceConfig2A');

        if Assigned(@ChangeServiceConfig2) then
        begin
          if not ChangeServiceConfig2(ServiceHandle, SERVICE_CONFIG_FAILURE_ACTIONS, @ServiceFailureAction) then
            Result := System.GetLastError;
        end
        else
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;

end;

function SetServiceFailureFlag(ServiceName: String; FailureActionsOnNonCrashFailures: Boolean): Integer;
const
  SERVICE_CONFIG_FAILURE_ACTIONS_FLAG = 4;
type

  TServiceFailureActionsFlag = record
    fFailureActionsOnNonCrashFailures: BOOL;
  end;

var
  ChangeServiceConfig2: function(hService: SC_HANDLE; dwInfoLevel: DWORD; lpInfo: Pointer): BOOL; stdcall;
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
  ServiceFailureActionsFlag: TServiceFailureActionsFlag;
begin
  Result := 0;

  DWord(ServiceFailureActionsFlag.fFailureActionsOnNonCrashFailures) := DWord(FailureActionsOnNonCrashFailures);

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_CHANGE_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        @ChangeServiceConfig2 := GetProcAddress(GetModuleHandle(advapi32), 'ChangeServiceConfig2A');

        if Assigned(@ChangeServiceConfig2) then
        begin
          if not ChangeServiceConfig2(ServiceHandle, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, @ServiceFailureActionsFlag) then
             Result := System.GetLastError;
        end
        else
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
    
end;

function SetServiceDelayedAutoStartInfo(ServiceName: String; DelayedAutostart: Boolean): Integer;
const
  SERVICE_CONFIG_DELAYED_AUTO_START_INFO = 3;
type

  TServiceDelayedAutoStartInfo = record
    fDelayedAutostart: BOOL;
  end;
  
var
  ChangeServiceConfig2: function(hService: SC_HANDLE; dwInfoLevel: DWORD; lpInfo: Pointer): BOOL; stdcall;
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
  ServiceDelayedAutoStartInfo: TServiceDelayedAutoStartInfo;
begin
  Result := 0;

  DWord(ServiceDelayedAutoStartInfo.fDelayedAutostart) := DWord(DelayedAutostart);

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_LOCK);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_CHANGE_CONFIG);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        @ChangeServiceConfig2 := GetProcAddress(GetModuleHandle(advapi32), 'ChangeServiceConfig2A');

        if Assigned(@ChangeServiceConfig2) then
        begin
          if not ChangeServiceConfig2(ServiceHandle, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, @ServiceDelayedAutoStartInfo) then
             Result := System.GetLastError;
        end
        else
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
    
end;

function ServiceIsRunning(ServiceName: String; var IsRunning: Boolean): Integer;
var
  Status: DWORD;
begin
  Result := GetServiceStatus(ServiceName, Status);

  if Result = 0 then
    IsRunning := Status = SERVICE_RUNNING
  else
    IsRunning := False;
end;

function ServiceIsStopped(ServiceName: String; var IsStopped: Boolean): Integer;
var
  Status: DWORD;
begin
  Result := GetServiceStatus(ServiceName, Status);

  if Result = 0 then
    IsStopped := Status = SERVICE_STOPPED
  else
    IsStopped := False;
end;

function ServiceIsPaused(ServiceName: String; var IsPaused: Boolean): Integer;
var
  Status: DWORD;
begin
  Result := GetServiceStatus(ServiceName, Status);

  if Result = 0 then
    IsPaused := Status = SERVICE_PAUSED
  else
    IsPaused := False;
end;

function RestartService(ServiceName: String; ServiceArguments: String; Timeout: Integer): Integer;
begin
  Result := StopService(ServiceName, False, Timeout);

  if Result = 0 then
    Result := StartService(ServiceName, ServiceArguments, Timeout);
end;

function InstallService(ServiceName, DisplayName: String; ServiceType: DWORD;
  StartType: DWORD; BinaryPathName: String; Dependencies: String;
  Username: String; Password: String): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  PDependencies: PChar;
  PUsername: PChar;
  PPassword: PChar;
const
  ReplaceDelimitter: String = '/';

  function Replace(Value: String): String;
  begin
    while Pos(ReplaceDelimitter, Value) <> 0 do
    begin
      Result := Result + Copy(Value, 1, Pos(ReplaceDelimitter, Value) -1) + Chr(0);
      Delete(Value, 1, Pos(ReplaceDelimitter, Value));
    end;

    Result := Result + Value + Chr(0) + Chr(0);
  end;

begin
  Result := 0;
  
  if Dependencies = '' then
    PDependencies := nil
  else
    PDependencies := PChar(Replace(Dependencies));

  if UserName = '' then
    PUsername := nil
  else
    PUsername := PChar(Username);

  if Password = '' then
    PPassword := nil
  else
    PPassword := PChar(Password);

  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_ALL_ACCESS);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := CreateService(ManagerHandle,
                                   PChar(ServiceName),
                                   PChar(DisplayName),
                                   SERVICE_START or SERVICE_QUERY_STATUS or _DELETE,
                                   ServiceType,
                                   StartType,
                                   SERVICE_ERROR_NORMAL,
                                   PChar(BinaryPathName),
                                   nil,
                                   nil,
                                   PDependencies,
                                   PUsername,
                                   PPassword);

    if ServiceHandle <> 0 then
      CloseServiceHandle(ServiceHandle)
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function RemoveService(ServiceName: String): Integer;
var
  ManagerHandle: SC_HANDLE;
  ServiceHandle: SC_HANDLE;
  LockHandle: SC_LOCK;
  Deleted: Boolean;
begin
  Result := 0;
  
  ManagerHandle := OpenSCManager('', nil, SC_MANAGER_ALL_ACCESS);

  if ManagerHandle > 0 then
  begin
    ServiceHandle := OpenService(ManagerHandle, PChar(ServiceName), SERVICE_ALL_ACCESS);

    if ServiceHandle > 0 then
    begin
      LockHandle := LockServiceDatabase(ManagerHandle);

      if LockHandle <> nil then
      begin
        Deleted := DeleteService(ServiceHandle);

        if not Deleted then
          Result := System.GetLastError;

        UnlockServiceDatabase(LockHandle);
      end
      else
        Result := System.GetLastError;

      CloseServiceHandle(ServiceHandle);
    end
    else
      Result := System.GetLastError;

    CloseServiceHandle(ManagerHandle);
  end
  else
    Result := System.GetLastError;
end;

function GetErrorMessage(ErrorCode: Integer): String;
begin
  Result := SysErrorMessage(ErrorCode);
end;

end.
