library SimpleSC;

uses
  NSIS, Windows, ServiceControl, LSASecurityControl, SysUtils;

function BoolToStr(Value: Boolean): String;
begin
  if Value then
    Result := '1'
  else
    Result := '0';
end;

function StrToBool(Value: String): Boolean;
begin
  Result := Value = '1';
end;

procedure InstallService(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  DisplayName: String;
  ServiceType: Cardinal;
  StartType: Cardinal;
  BinaryPath: String;
  Dependencies: String;
  Username: String;
  Password: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  DisplayName := PopString;
  ServiceType := StrToInt(PopString);
  StartType := StrToInt(PopString);
  BinaryPath := PopString;
  Dependencies := PopString;
  Username := PopString;
  Password := PopString;

  ServiceResult := IntToStr(ServiceControl.InstallService(ServiceName, DisplayName, ServiceType, StartType, BinaryPath, Dependencies, Username, Password));
  PushString(ServiceResult);
end;

procedure RemoveService(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;

  ServiceResult := IntToStr(ServiceControl.RemoveService(ServiceName));
  PushString(ServiceResult);
end;

procedure StartService(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  ServiceArguments: String;
  Timeout: Integer;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceArguments := PopString;
  Timeout := StrToInt(PopString);
  ServiceResult := IntToStr(ServiceControl.StartService(ServiceName, ServiceArguments, Timeout));
  PushString(ServiceResult);
end;

procedure StopService(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  WaitForFileRelease: Boolean;
  Timeout: Integer;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  WaitForFileRelease := StrToBool(PopString);
  Timeout := StrToInt(PopString);
  ServiceResult := IntToStr(ServiceControl.StopService(ServiceName, WaitForFileRelease, Timeout));
  PushString(ServiceResult)
end;

procedure PauseService(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  Timeout: Integer;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  Timeout := StrToInt(PopString);
  ServiceResult := IntToStr(ServiceControl.PauseService(ServiceName, Timeout));
  PushString(ServiceResult)
end;

procedure ContinueService(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  Timeout: Integer;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  Timeout := StrToInt(PopString);
  ServiceResult := IntToStr(ServiceControl.ContinueService(ServiceName, Timeout));
  PushString(ServiceResult)
end;

procedure GetServiceName(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
Var
  DisplayName: String;
  ServiceResult: String;
  ServiceName: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  DisplayName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceName(DisplayName, ServiceName));
  PushString(ServiceName);
  PushString(ServiceResult);
end;

procedure GetServiceDisplayName(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
Var
  ServiceName: String;
  DisplayName: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceDisplayName(ServiceName, DisplayName));
  PushString(DisplayName);
  PushString(ServiceResult);
end;

procedure GetServiceStatus(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  Status: DWORD;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceStatus(ServiceName, Status));
  PushString(IntToStr(Status));
  PushString(ServiceResult);
end;

procedure GetServiceBinaryPath(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  BinaryPath: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceBinaryPath(ServiceName, BinaryPath));
  PushString(BinaryPath);
  PushString(ServiceResult);
end;

procedure GetServiceDescription(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  Description: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceDescription(ServiceName, Description));
  PushString(Description);
  PushString(ServiceResult);
end;

procedure GetServiceStartType(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  StartType: DWORD;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceStartType(ServiceName, StartType));
  PushString(IntToStr(StartType));
  PushString(ServiceResult);
end;

procedure GetServiceLogon(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  Username: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceLogon(ServiceName, Username));
  PushString(Username);
  PushString(ServiceResult);
end;

procedure GetServiceFailure(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  ResetPeriod: DWORD;
  RebootMessage: String;
  Command: String;
  Action1: Integer;
  ActionDelay1: DWORD;
  Action2: Integer;
  ActionDelay2: DWORD;
  Action3: Integer;
  ActionDelay3: DWORD;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceFailure(ServiceName, ResetPeriod, RebootMessage, Command, Action1, ActionDelay1, Action2, ActionDelay2, Action3, ActionDelay3));
  PushString(IntToStr(ActionDelay3));
  PushString(IntToStr(Action3));
  PushString(IntToStr(ActionDelay2));
  PushString(IntToStr(Action2));
  PushString(IntToStr(ActionDelay1));
  PushString(IntToStr(Action1));
  PushString(Command);
  PushString(RebootMessage);
  PushString(IntToStr(ResetPeriod));
  PushString(ServiceResult);
end;

procedure GetServiceFailureFlag(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  FailureActionsOnNonCrashFailures: Boolean;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceFailureFlag(ServiceName, FailureActionsOnNonCrashFailures));
  PushString(BoolToStr(FailureActionsOnNonCrashFailures));
  PushString(ServiceResult);
end;

procedure GetServiceDelayedAutoStartInfo(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  DelayedAutostart: Boolean;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.GetServiceDelayedAutoStartInfo(ServiceName, DelayedAutostart));
  PushString(BoolToStr(DelayedAutostart));
  PushString(ServiceResult);
end;

procedure SetServiceDescription(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  Description: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  Description := PopString;
  ServiceResult := IntToStr(ServiceControl.SetServiceDescription(ServiceName, Description));
  PushString(ServiceResult);
end;

procedure SetServiceStartType(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  ServiceStartType: DWORD;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceStartType := StrToInt(PopString);
  ServiceResult := IntToStr(ServiceControl.SetServiceStartType(ServiceName, ServiceStartType));
  PushString(ServiceResult);
end;

procedure SetServiceLogon(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  Username: String;
  Password: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  Username := PopString;
  Password := PopString;

  ServiceResult := IntToStr(ServiceControl.SetServiceLogon(ServiceName, Username, Password));
  PushString(ServiceResult);
end;

procedure SetServiceBinaryPath(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  BinaryPath: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  BinaryPath := PopString;

  ServiceResult := IntToStr(ServiceControl.SetServiceBinaryPath(ServiceName, BinaryPath));
  PushString(ServiceResult);
end;

procedure SetServiceFailure(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  ResetPeriod: DWORD;
  RebootMessage: String;
  Command: String;
  Action1: Integer;
  ActionDelay1: DWORD;
  Action2: Integer;
  ActionDelay2: DWORD;
  Action3: Integer;
  ActionDelay3: DWORD;
  ServiceResult: Integer;
  PrivilegeResult: Integer;
const
  SE_SHUTDOWN_PRIVILEGE = 'SeShutdownPrivilege';
  SC_ACTION_REBOOT = 2;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ResetPeriod := StrToInt(PopString);
  RebootMessage := PopString;
  Command := PopString;
  Action1 := StrToInt(PopString);
  ActionDelay1 := StrToInt(PopString);
  Action2 := StrToInt(PopString);
  ActionDelay2 := StrToInt(PopString);
  Action3 := StrToInt(PopString);
  ActionDelay3 := StrToInt(PopString);

  if (Action1 = SC_ACTION_REBOOT) or (Action2 = SC_ACTION_REBOOT) or (Action3 = SC_ACTION_REBOOT) then
  begin
    PrivilegeResult := LSASecurityControl.EnablePrivilege(SE_SHUTDOWN_PRIVILEGE);

    if not PrivilegeResult = 0 then
    begin
      PushString(IntToStr(PrivilegeResult));
      Exit;
    end;
  end;

  ServiceResult := ServiceControl.SetServiceFailure(ServiceName, ResetPeriod, RebootMessage, Command, Action1, ActionDelay1,
                                                    Action2, ActionDelay2, Action3, ActionDelay3);


  if (Action1 = SC_ACTION_REBOOT) or (Action2 = SC_ACTION_REBOOT) or (Action3 = SC_ACTION_REBOOT) then
  begin
    PrivilegeResult := LSASecurityControl.DisablePrivilege(SE_SHUTDOWN_PRIVILEGE);

    if not PrivilegeResult = 0 then
    begin
      PushString(IntToStr(PrivilegeResult));
      Exit;
    end;
  end;

  PushString(IntToStr(ServiceResult));

end;

procedure SetServiceFailureFlag(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  FailureActionsOnNonCrashFailures: Boolean;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  FailureActionsOnNonCrashFailures := StrToBool(PopString);
  ServiceResult := IntToStr(ServiceControl.SetServiceFailureFlag(ServiceName, FailureActionsOnNonCrashFailures));
  PushString(ServiceResult)
end;

procedure SetServiceDelayedAutoStartInfo(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  DelayedAutostart: Boolean;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  DelayedAutostart := StrToBool(PopString);
  ServiceResult := IntToStr(ServiceControl.SetServiceDelayedAutoStartInfo(ServiceName, DelayedAutostart));
  PushString(ServiceResult)
end;

procedure ServiceIsRunning(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  IsRunning: Boolean;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.ServiceIsRunning(ServiceName, IsRunning));
  PushString(BoolToStr(IsRunning));
  PushString(ServiceResult);
end;

procedure ServiceIsStopped(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  IsStopped: Boolean;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.ServiceIsStopped(ServiceName, IsStopped));
  PushString(BoolToStr(IsStopped));
  PushString(ServiceResult);
end;

procedure ServiceIsPaused(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  IsPaused: Boolean;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceResult := IntToStr(ServiceControl.ServiceIsPaused(ServiceName, IsPaused));
  PushString(BoolToStr(IsPaused));
  PushString(ServiceResult);
end;

procedure RestartService(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  ServiceArguments: String;
  Timeout: Integer;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;
  ServiceArguments := PopString;
  Timeout := StrToInt(PopString);
  ServiceResult := IntToStr(ServiceControl.RestartService(ServiceName, ServiceArguments, Timeout));
  PushString(ServiceResult);
end;

procedure ExistsService(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ServiceName: String;
  ServiceResult: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ServiceName := PopString;

  ServiceResult := IntToStr(ServiceControl.ExistsService(ServiceName));
  PushString(ServiceResult);
end;

procedure GrantServiceLogonPrivilege(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  AccountName: String;
  LSAResult: String;
const
  SE_SERVICE_LOGON_RIGHT = 'SeServiceLogonRight';
begin
  Init(hwndParent, string_size, variables, stacktop);

  AccountName := PopString;

  LSAResult := IntToStr(LSASecurityControl.GrantPrivilege(AccountName, SE_SERVICE_LOGON_RIGHT));
  PushString(LSAResult);
end;

procedure RemoveServiceLogonPrivilege(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  AccountName: String;
  LSAResult: String;
const
  SE_SERVICE_LOGON_RIGHT = 'SeServiceLogonRight';
begin
  Init(hwndParent, string_size, variables, stacktop);

  AccountName := PopString;

  LSAResult := IntToStr(LSASecurityControl.RemovePrivilege(AccountName, SE_SERVICE_LOGON_RIGHT));
  PushString(LSAResult);
end;

procedure GetErrorMessage(const hwndParent: HWND; const string_size: integer;
  const variables: PChar; const stacktop: pointer); cdecl;
var
  ErrorCode: Integer;
  ErrorMessage: String;
begin
  Init(hwndParent, string_size, variables, stacktop);

  ErrorCode := StrToInt(PopString);

  ErrorMessage := ServiceControl.GetErrorMessage(ErrorCode);
  PushString(ErrorMessage);
end;

exports InstallService;
exports RemoveService;
exports StartService;
exports StopService;
exports PauseService;
exports ContinueService;
exports GetServiceName;
exports GetServiceDisplayName;
exports GetServiceStatus;
exports GetServiceBinaryPath;
exports GetServiceDescription;
exports GetServiceStartType;
exports GetServiceLogon;
exports GetServiceFailure;
exports GetServiceFailureFlag;
exports GetServiceDelayedAutoStartInfo;
exports SetServiceDescription;
exports SetServiceStartType;
exports SetServiceLogon;
exports SetServiceBinaryPath;
exports SetServiceFailure;
exports SetServiceFailureFlag;
exports SetServiceDelayedAutoStartInfo;
exports ServiceIsRunning;
exports ServiceIsStopped;
exports ServiceIsPaused;
exports RestartService;
exports ExistsService;
exports GrantServiceLogonPrivilege;
exports RemoveServiceLogonPrivilege;
exports GetErrorMessage;

end.
