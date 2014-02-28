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

The original code is LSASecurityControl.pas, released April 16, 2007. 

The initial developer of the original code is Rainer Budde (http://www.speed-soft.de).

SimpleSC - NSIS Service Control Plugin is written, published and maintaned by
Rainer Budde (rainer@speed-soft.de).
}
unit LSASecurityControl;

interface

uses
  Windows, SysUtils;

  function GrantPrivilege(AccountName: String; PrivilegeName: String): Integer;
  function RemovePrivilege(AccountName: String; PrivilegeName: String): Integer;
  function EnablePrivilege(PrivilegeName: String): Integer;
  function DisablePrivilege(PrivilegeName: String): Integer;

implementation

type
  LSA_HANDLE = Pointer;
  TLSAHandle = LSA_HANDLE;

  LSA_UNICODE_STRING = record
    Length: Word;
    MaximumLength: Word;
    Buffer: PWideChar;
  end;
  TLSAUnicodeString = LSA_UNICODE_STRING;
  PLSAUnicodeString = ^TLSAUnicodeString;

  LSA_OBJECT_ATTRIBUTES = record
    Length: ULONG;
    RootDirectory: THandle;
    ObjectName: PLSAUnicodeString;
    Attributes: ULONG;
    SecurityDescriptor: Pointer;
    SecurityQualityOfService: Pointer; 
  end;
  TLsaObjectAttributes = LSA_OBJECT_ATTRIBUTES;
  PLsaObjectAttributes = ^TLsaObjectAttributes;

  function LsaOpenPolicy(SystemName: PLSAUnicodeString; var ObjectAttributes: TLsaObjectAttributes; DesiredAccess: ACCESS_MASK;  var PolicyHandle: LSA_HANDLE): DWORD; stdcall; external 'advapi32.dll';
  function LsaAddAccountRights(PolicyHandle: LSA_HANDLE; AccountSid: PSID; UserRights: PLSAUnicodeString; CountOfRights: ULONG): DWORD; stdcall; external 'advapi32.dll';
  function LsaRemoveAccountRights(PolicyHandle: LSA_HANDLE; AccountSid: PSID; AllRights: Boolean; UserRights: PLSAUnicodeString; CountOfRights: ULONG): DWORD; stdcall; external 'advapi32.dll';
  function LsaClose(ObjectHandle: LSA_HANDLE): DWORD; stdcall; external 'advapi32.dll';


function GetAccountSid(const AccountName: String; var Sid: PSID): Integer;
var
  DomainSize: LongWord;
  SidSize: LongWord;
  Domain: String;
  Use: SID_NAME_USE;
begin
  Result := 0;

  SidSize := 0;
  DomainSize := 0;

  if not LookupAccountName(nil, PChar(AccountName), nil, SidSize, nil, DomainSize, Use) and (GetLastError = ERROR_INSUFFICIENT_BUFFER) then
  begin
    SetLength(Domain, DomainSize);
    Sid := AllocMem(SidSize);

    if not LookupAccountName(nil, PChar(AccountName), Sid, SidSize, PChar(Domain), DomainSize, Use) then
    begin
      Result := GetLastError;
      FreeMem(Sid);
      Sid := nil;
    end;
  end
  else
    Result := GetLastError;
end;

function GrantPrivilege(AccountName: String; PrivilegeName: String): Integer;
const
  UNICODE_NULL = WCHAR(0);
  POLICY_CREATE_ACCOUNT = $00000010;
  POLICY_LOOKUP_NAMES = $00000800;
var
  SID: PSID;
  PolicyHandle: TLSAHandle;
  LSAPrivilegeName: TLSAUnicodeString;
  LSAObjectAttributes: TLsaObjectAttributes;
  pwszPrivilegeName: PWideChar;
  PrivilegeNameLength: Cardinal;
  Status: DWORD;
begin
  Result := 0;

  GetMem(pwszPrivilegeName, Length(PrivilegeName) * SizeOf(WideChar) + 1);
  StringToWideChar(PrivilegeName, pwszPrivilegeName, Length(PrivilegeName) * SizeOf(WideChar) + 1);
  ZeroMemory(@LSAObjectAttributes, SizeOf(TLsaObjectAttributes));
  PrivilegeNameLength := Length(pwszPrivilegeName);

  if PrivilegeNameLength > 0 then
  begin
    Result := GetAccountSid(AccountName, SID);

    if Result = 0 then
    begin
      LSAPrivilegeName.Length := PrivilegeNameLength * SizeOf(WideChar);
      LSAPrivilegeName.MaximumLength := LSAPrivilegeName.Length + SizeOf(UNICODE_NULL);
      LSAPrivilegeName.Buffer := pwszPrivilegeName;

      Status := LsaOpenPolicy(nil, LSAObjectAttributes, POLICY_LOOKUP_NAMES or POLICY_CREATE_ACCOUNT, PolicyHandle);
      try
        if Status = 0 then
          Result := LsaAddAccountRights(PolicyHandle, Sid, @LSAPrivilegeName, 1)
        else
          Result := Status;
      finally
        LsaClose(PolicyHandle);
      end;
    end;

  end;
    
  FreeMem(pwszPrivilegeName);
end;

function RemovePrivilege(AccountName: String; PrivilegeName: String): Integer;
const
  UNICODE_NULL = WCHAR(0);
  POLICY_CREATE_ACCOUNT = $00000010;
  POLICY_LOOKUP_NAMES = $00000800;
var
  SID: PSID;
  PolicyHandle: TLSAHandle;
  LSAPrivilegeName: TLSAUnicodeString;
  LSAObjectAttributes: TLsaObjectAttributes;
  pwszPrivilegeName: PWideChar;
  PrivilegeNameLength: Cardinal;
  Status: DWORD;
begin
  Result := 0;

  GetMem(pwszPrivilegeName, Length(PrivilegeName) * SizeOf(WideChar) + 1);
  StringToWideChar(PrivilegeName, pwszPrivilegeName, Length(PrivilegeName) * SizeOf(WideChar) + 1);
  ZeroMemory(@LSAObjectAttributes, SizeOf(TLsaObjectAttributes));
  PrivilegeNameLength := Length(pwszPrivilegeName);

  if PrivilegeNameLength > 0 then
  begin
    Result := GetAccountSid(AccountName, SID);

    if Result = 0 then
    begin
      LSAPrivilegeName.Length := PrivilegeNameLength * SizeOf(WideChar);
      LSAPrivilegeName.MaximumLength := LSAPrivilegeName.Length + SizeOf(UNICODE_NULL);
      LSAPrivilegeName.Buffer := pwszPrivilegeName;

      Status := LsaOpenPolicy(nil, LSAObjectAttributes, POLICY_LOOKUP_NAMES or POLICY_CREATE_ACCOUNT, PolicyHandle);

      try
        if Status = 0 then
          Result := LsaRemoveAccountRights(PolicyHandle, Sid, False, @LSAPrivilegeName, 1)
        else
          Result := Status;
      finally
        LsaClose(PolicyHandle);
      end;
    end;

  end;
    
  FreeMem(pwszPrivilegeName);
end;

function EnablePrivilege(PrivilegeName: String): Integer;
var
  TokenHandle: THandle;
  TokenPrivileges: TOKEN_PRIVILEGES;
  PreviousState: TOKEN_PRIVILEGES;
  ReturnLength: Cardinal;
begin
  Result := 0;

  if OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES or TOKEN_QUERY, TokenHandle) then
  begin
    try

      if LookupPrivilegeValue(nil, PChar(PrivilegeName), TokenPrivileges.Privileges[0].Luid) then
      begin
        TokenPrivileges.PrivilegeCount := 1;
        TokenPrivileges.Privileges[0].Attributes := SE_PRIVILEGE_ENABLED;

        if not AdjustTokenPrivileges(TokenHandle, False, TokenPrivileges, SizeOf(TokenPrivileges), PreviousState, ReturnLength) then
          Result := System.GetLastError;
      end
      else
        Result := System.GetLastError;

    finally
      CloseHandle(TokenHandle);
    end;
  end
  else
    Result := System.GetLastError;

end;

function DisablePrivilege(PrivilegeName: String): Integer;
var
  TokenHandle: THandle;
  TokenPrivileges: TOKEN_PRIVILEGES;
  PreviousState: TOKEN_PRIVILEGES;
  ReturnLength: Cardinal;
begin

  Result := 0;

  if OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES or TOKEN_QUERY, TokenHandle) then
  begin
    try

      if LookupPrivilegeValue(nil, PChar(PrivilegeName), TokenPrivileges.Privileges[0].Luid) then
      begin
        TokenPrivileges.PrivilegeCount := 1;
        TokenPrivileges.Privileges[0].Attributes := SE_PRIVILEGE_ENABLED;

        if not AdjustTokenPrivileges(TokenHandle, False, TokenPrivileges, SizeOf(TokenPrivileges), PreviousState, ReturnLength) then
          Result := System.GetLastError;
      end
      else
        Result := System.GetLastError;

    finally
      CloseHandle(TokenHandle);
    end;
  end
  else
    Result := System.GetLastError;

end;

end.
