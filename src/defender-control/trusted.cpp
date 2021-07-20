#include "trusted.hpp"

namespace trusted
{
  // Enable prvileges
  //
  bool enable_privilege(std::string privilege)
  {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken))
      return false;

    LUID luid;
    if (!LookupPrivilegeValueA(nullptr, privilege.c_str(), &luid))
    {
      CloseHandle(hToken);
      return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr))
    {
      CloseHandle(hToken);
      return false;
    }

    CloseHandle(hToken);
    return true;
  }

  // Get target process id
  //
  DWORD get_pid(std::string process_name)
  {
    HANDLE hSnapshot;
    if ((hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) == INVALID_HANDLE_VALUE)
    {
      return -1;
    }

    DWORD pid = -1;
    PROCESSENTRY32 pe;
    ZeroMemory(&pe, sizeof(PROCESSENTRY32));
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe))
    {
      while (Process32Next(hSnapshot, &pe))
      {
        if (pe.szExeFile == process_name)
        {
          pid = pe.th32ProcessID;
          break;
        }
      }
    }
    else
    {
      CloseHandle(hSnapshot);
      return -1;
    }

    if (pid == -1)
    {
      CloseHandle(hSnapshot);
      return -1;
    }

    CloseHandle(hSnapshot);
    return pid;
  }

  // Give system permissions
  //
  bool impersonate_system()
  {
    return true;
  }

  // Gives trustedinstaller permissions
  //
  bool impersonate_trusted(DWORD pid)
  {
    enable_privilege(SE_DEBUG_NAME);
    enable_privilege(SE_IMPERSONATE_NAME);
    impersonate_system();

    HANDLE hTIProcess;
    if ((hTIProcess = OpenProcess(
      PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION,
      FALSE,
      pid)) == nullptr)
    {
      return false;
    }

    HANDLE hTIToken;
    if (!OpenProcessToken(
      hTIProcess,
      MAXIMUM_ALLOWED,
      &hTIToken))
    {
      CloseHandle(hTIProcess);
      return false;
    }

    HANDLE hDupToken;
    SECURITY_ATTRIBUTES tokenAttributes;
    tokenAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    tokenAttributes.lpSecurityDescriptor = nullptr;
    tokenAttributes.bInheritHandle = FALSE;
    if (!DuplicateTokenEx(
      hTIToken,
      MAXIMUM_ALLOWED,
      &tokenAttributes,
      SecurityImpersonation,
      TokenImpersonation,
      &hDupToken))
    {
      CloseHandle(hTIToken);
      return false;
    }

    if (!ImpersonateLoggedOnUser(hDupToken))
    {
      CloseHandle(hDupToken);
      return false;
    }
    return true;
  }

  // Start the trusted installer service
  //
  DWORD start_trusted()
  {
    SC_HANDLE hSCManager;
    if ((hSCManager = OpenSCManagerA(
      nullptr,
      SERVICES_ACTIVE_DATABASE,
      GENERIC_EXECUTE)) == nullptr)
    {
      return -1;
    }

    SC_HANDLE hService;
    if ((hService = OpenServiceW(
      hSCManager,
      L"TrustedInstaller",
      GENERIC_READ | GENERIC_EXECUTE)) == nullptr)
    {
      CloseServiceHandle(hSCManager);
      return -1;
    }

    SERVICE_STATUS_PROCESS statusBuffer;
    DWORD bytesNeeded;
    while (QueryServiceStatusEx(
      hService,
      SC_STATUS_PROCESS_INFO,
      reinterpret_cast<LPBYTE>(&statusBuffer),
      sizeof(SERVICE_STATUS_PROCESS),
      &bytesNeeded))
    {
      if (statusBuffer.dwCurrentState == SERVICE_STOPPED)
      {
        if (!StartServiceW(hService, 0, nullptr))
        {
          CloseServiceHandle(hService);
          CloseServiceHandle(hSCManager);
          return -1;
        }
      }
      if (statusBuffer.dwCurrentState == SERVICE_START_PENDING ||
        statusBuffer.dwCurrentState == SERVICE_STOP_PENDING)
      {
        Sleep(statusBuffer.dwWaitHint);
        continue;
      }
      if (statusBuffer.dwCurrentState == SERVICE_RUNNING)
      {
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return statusBuffer.dwProcessId;
      }
    }
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return -1;
  }

  // Run process with trusted installer privilleges
  //
  bool create_process()
  {

  }

}