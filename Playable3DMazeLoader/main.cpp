#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <windows.h>

char gszCurrDirectory[512];

DWORD GetCurrentDirectoryPath()
{
	char *pLastSlash = NULL;

	memset(gszCurrDirectory, 0, sizeof(gszCurrDirectory));
	GetModuleFileNameA(NULL, gszCurrDirectory, sizeof(gszCurrDirectory) - 1);
	pLastSlash = strrchr(gszCurrDirectory, '\\');
	if(pLastSlash == NULL)
	{
		return 1;
	}
	*pLastSlash = '\0';

	return 0;
}

DWORD LaunchMazeProcess(HANDLE *phProcess, HANDLE *phThread)
{
	char szMazeExePath[512];
	STARTUPINFOA StartupInfo;
	PROCESS_INFORMATION ProcessInfo;

	memset(szMazeExePath, 0, sizeof(szMazeExePath));
	_snprintf(szMazeExePath, sizeof(szMazeExePath) - 1, "%s\\ssmaze.scr", gszCurrDirectory);

	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	if(CreateProcessA(NULL, szMazeExePath, NULL, NULL, 0, CREATE_NEW_CONSOLE | CREATE_SUSPENDED, NULL, NULL, &StartupInfo, &ProcessInfo) == 0)
	{
		return 1;
	}

	*phProcess = ProcessInfo.hProcess;
	*phThread = ProcessInfo.hThread;

	return 0;
}

DWORD InjectDLL(HANDLE hProcess)
{
	VOID *pRemoteDllPath = NULL;
	DWORD dwBytesWritten = 0;
	HANDLE hInjectThread = NULL;
	DWORD dwExitCode = 0;
	char szDllPath[512];

	memset(szDllPath, 0, sizeof(szDllPath));
	_snprintf(szDllPath, sizeof(szDllPath) - 1, "%s\\Playable3DMaze.dll", gszCurrDirectory);

	pRemoteDllPath = VirtualAllocEx(hProcess, NULL, sizeof(szDllPath), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if(pRemoteDllPath == NULL)
	{
		return 1;
	}

	if(WriteProcessMemory(hProcess, pRemoteDllPath, szDllPath, sizeof(szDllPath), &dwBytesWritten) == 0)
	{
		return 1;
	}

	hInjectThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, pRemoteDllPath, 0, NULL);
	if(hInjectThread == NULL)
	{
		return 1;
	}

	WaitForSingleObject(hInjectThread, INFINITE);
	GetExitCodeThread(hInjectThread, &dwExitCode);
	if(dwExitCode == 0)
	{
		return 1;
	}

	return 0;
}

int main()
{
	HANDLE hProcess = NULL;
	HANDLE hThread = NULL;

	printf("Playable3DMaze\n");
	printf("- x86matthew\n");

	// get current directory
	if(GetCurrentDirectoryPath() != 0)
	{
		printf("Error: Failed to get directory\n");
		return 1;
	}

	printf("Launching process...\n");

	// launch suspended maze (expected hash: 78852d8d5404fdbdcf9d785c9c0393c85de64efed4bd66cca02050bbf3608eaf)
	if(LaunchMazeProcess(&hProcess, &hThread) != 0)
	{
		printf("Error: Failed to launch process\n");
		return 1;
	}

	printf("Injecting DLL...\n");

	// inject DLL into remote process
	if(InjectDLL(hProcess) != 0)
	{
		printf("Error: Failed to inject DLL\n");
		return 1;
	}

	printf("Starting game...\n");

	// resume process
	ResumeThread(hThread);

	return 0;
}
