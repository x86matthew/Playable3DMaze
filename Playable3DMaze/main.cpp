#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <windows.h>

#define GET_EXE_ADDR(X) (gpExeImageBase + X)

BYTE *gpExeImageBase = NULL;
BYTE gbOrigEntryPointBytes[5];
BYTE gbAllowMove = 0;
BYTE gbPlayerMoved = 0;
BYTE gbCurrEntityIsPlayer = 0;
INT giSpinDirection = 0;

DWORD (*pOrig_EntryPoint)(VOID *pPEB) = NULL;
VOID* (__stdcall *pOrig_EntityMove)(VOID *pParam1, VOID *pParam2) = NULL;
LRESULT (__stdcall *pOrig_WndProc)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) = NULL;

struct ConfigEntryStruct
{
	WCHAR *pFieldName;
	DWORD dwValue;
	WCHAR *pValue;
};

ConfigEntryStruct gConfigEntryList[] =
{
	{ L"WallTextureFile", 0, L"" },
	{ L"FloorTextureFile", 0, L"" },
	{ L"CeilingTextureFile", 0, L"" },
	{ L"DefaultTextureEnable", 7, NULL },
	{ L"DefWallTexture", 0, NULL },
	{ L"DefFloorTexture", 1, NULL },
	{ L"DefCeilingTexture", 2, NULL },
	{ L"WallTextureOffset", 0, NULL },
	{ L"FloorTextureOffset", 0, NULL },
	{ L"CeilingTextureOffset", 0, NULL },
	{ L"Overlay", 0, NULL },
	{ L"RatCount", 5, NULL },
	{ L"ImageQuality", 0, NULL },
	{ L"Size", 100, NULL },
	{ L"TurboMode", 0, NULL },
};

LPWSTR __stdcall Hook_GetCommandLineW()
{
	// force window-mode
	return L"ssmaze.scr /w";
}

ConfigEntryStruct *FindConfigEntry(WCHAR *pFieldName)
{
	for(DWORD i = 0; i < sizeof(gConfigEntryList) / sizeof(gConfigEntryList[0]); i++)
	{
		if(wcscmp(pFieldName, gConfigEntryList[i].pFieldName) == 0)
		{
			return &gConfigEntryList[i];
		}
	}

	return NULL;
}

DWORD __stdcall Hook_GetPrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName, LPCWSTR lpDefault, LPWSTR lpReturnedString, DWORD nSize, LPCWSTR lpFileName)
{
	ConfigEntryStruct *pConfigEntry = NULL;
	DWORD dwLength = 0;

	// lookup config value
	pConfigEntry = FindConfigEntry((WCHAR*)lpKeyName);
	if(pConfigEntry == NULL)
	{
		// error
		return 0;
	}

	// copy result
	dwLength = (DWORD)wcslen(pConfigEntry->pValue);
	if(dwLength >= nSize)
	{
		// error
		return 0;
	}
	wcsncpy(lpReturnedString, pConfigEntry->pValue, nSize);

	return dwLength;
}

UINT __stdcall Hook_GetPrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName, INT nDefault, LPCWSTR lpFileName)
{
	ConfigEntryStruct *pConfigEntry = NULL;

	// lookup config value
	pConfigEntry = FindConfigEntry((WCHAR*)lpKeyName);
	if(pConfigEntry == NULL)
	{
		// error
		return 0;
	}

	return pConfigEntry->dwValue;
}

BYTE IsWindowFocused()
{
	// check if the window is currently focused, this flag is already stored in an internal structure
	if(*(BYTE*)(*(ULONG_PTR*)GET_EXE_ADDR(0x1B3BC) + 8) != 0)
	{
		return 1;
	}

	return 0;
}

BYTE IsUpsideDown()
{
	// check current roll angle
	if(*(double*)GET_EXE_ADDR(0x1FB58) == 180.0)
	{
		return 1;
	}

	return 0;
}

VOID CheckMoveKeys(DWORD dwMoveType)
{
	BYTE bKey = 0;

	// check current entity type
	if(gbCurrEntityIsPlayer == 0)
	{
		// rat - always allow movement
		gbAllowMove = 1;
	}
	else
	{
		// player
		gbAllowMove = 0;

		// check if the window is currently focused
		if(IsWindowFocused())
		{
			// check move type, get corresponding key
			if(dwMoveType == 0)
			{
				// left (right if upside down)
				if(IsUpsideDown())
				{
					bKey = VK_RIGHT;
				}
				else
				{
					bKey = VK_LEFT;
				}
			}
			else if(dwMoveType == 1)
			{
				// forward
				bKey = VK_UP;
			}
			else if(dwMoveType == 2)
			{
				// right (left if upside down)
				if(IsUpsideDown())
				{
					bKey = VK_LEFT;
				}
				else
				{
					bKey = VK_RIGHT;
				}
			}

			if(bKey != 0)
			{
				// check if the target key is currently down
				if(GetAsyncKeyState(bKey) & 0x8000)
				{
					// allow the current movement
					gbAllowMove = 1;

					// at least 1 move has been made - set flag
					gbPlayerMoved = 1;
				}
			}
		}
	}
}

VOID __declspec(naked) Hook_MoveBranch()
{
	_asm
	{
		// check key states (edx contains the current "move type")
		pushad
		push edx
		call CheckMoveKeys
		add esp, 4
		popad

		// check if the current movement is allowed
		mov al, gbAllowMove
		test al, al
		jnz AllowMove

		// movement blocked, ignore
		mov eax, gpExeImageBase
		add eax, 0x601C
		jmp eax

AllowMove:
		// push jump destination
		mov eax, gpExeImageBase
		add eax, 0x603C
		push eax

		// execute original bytes first
		mov eax, gpExeImageBase
		add eax, 0x19DD0
		mov eax, dword ptr [eax+edi*8]

		// jump to destination
		ret
	}
}

VOID CheckSpinKeys()
{
	if(gbCurrEntityIsPlayer == 0)
	{
		// rat - always allow spin
		gbAllowMove = 1;
	}
	else
	{
		// player
		gbAllowMove = 0;

		// only allow spin if the player has made at least 1 move.
		// this is because the player can be "spawned" within an outer wall.
		if(gbPlayerMoved != 0)
		{
			// check if the window is currently focused
			if(IsWindowFocused())
			{
				// check key states
				if(GetAsyncKeyState(VK_LEFT) & 0x8000)
				{
					// left key - set spin direction
					if(IsUpsideDown())
					{
						giSpinDirection = 1;
					}
					else
					{
						giSpinDirection = -1;
					}
					gbAllowMove = 1;
				}
				else if(GetAsyncKeyState(VK_RIGHT) & 0x8000)
				{
					// right key - set spin direction
					if(IsUpsideDown())
					{
						giSpinDirection = -1;
					}
					else
					{
						giSpinDirection = 1;
					}
					gbAllowMove = 1;
				}
				else if(GetAsyncKeyState(VK_DOWN) & 0x8000)
				{
					// down key - always spin (direction doesn't matter)
					giSpinDirection = 1;
					gbAllowMove = 1;
				}
			}
		}
	}
}

VOID __declspec(naked) Hook_SpinBranch()
{
	_asm
	{
		// check key states
		pushad
		call CheckSpinKeys
		popad

		// check if the current movement is allowed
		mov al, gbAllowMove
		test al, al
		jnz AllowMove

		// movement blocked, ignore
		mov eax, gpExeImageBase
		add eax, 0x616A
		jmp eax

AllowMove:
		// push jump destination
		mov eax, gpExeImageBase
		add eax, 0x608F
		push eax

		// execute original bytes first
		mov eax, dword ptr [esi+8]
		mov edx, dword ptr [ecx+eax*4]

		// jump to destination
		ret
	}
}

VOID __declspec(naked) Hook_EntitySpin()
{
	_asm
	{
		// check if the current entity is the player
		mov al, gbCurrEntityIsPlayer
		test al, al
		jnz OverrideDirection

		// current entity is a rat - execute original branch
		mov eax, gpExeImageBase
		add eax, 0x615B
		jmp eax

OverrideDirection:
		// player - override the default spin direction
		fild giSpinDirection
		mov eax, gpExeImageBase
		add eax, 0x615E
		jmp eax
	}
}

VOID *__stdcall Hook_EntityMove_Player(VOID *pParam1, VOID *pParam2)
{
	// current entity is the player
	gbCurrEntityIsPlayer = 1;

	// check for tab key
	if(GetAsyncKeyState(VK_TAB) & 1)
	{
		// check if windows is currently in focus
		if(IsWindowFocused())
		{
			// toggle map
			*(DWORD*)GET_EXE_ADDR(0x19D14) ^= 1;
		}
	}

	return pOrig_EntityMove(pParam1, pParam2);
}

VOID *__stdcall Hook_EntityMove_Rat(VOID *pParam1, VOID *pParam2)
{
	// current entity is a rat
	gbCurrEntityIsPlayer = 0;

	return pOrig_EntityMove(pParam1, pParam2);
}

LRESULT __stdcall Hook_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// use the default cursor when hovering over the window (the original binary doesn't handle this)
	if(uMsg == WM_SETCURSOR)
	{
		if(LOWORD(lParam) == HTCLIENT)
		{
			// use default cursor
			SetCursor(LoadCursor(NULL, IDC_ARROW));

			// message handled
			return 1;
		}
	}

	return pOrig_WndProc(hWnd, uMsg, wParam, lParam);
}

VOID __declspec(naked) Hook_BeginGame()
{
	_asm
	{
		// new game - reset flag
		mov gbPlayerMoved, 0

		// call original function
		mov eax, gpExeImageBase
		add eax, 0x5121
		jmp eax
	}
}

VOID OverwriteRelative32(VOID *pAddress, VOID *pDestination)
{
	// overwrite rel32 destination
	*(DWORD*)((BYTE*)pAddress) = (DWORD)((ULONG_PTR)pDestination - (ULONG_PTR)((BYTE*)pAddress + 4));
}

VOID InlineHook(VOID *pAddress, VOID *pDestination)
{
	// add jmp rel32
	*(BYTE*)pAddress = 0xE9;
	OverwriteRelative32((BYTE*)pAddress + 1, pDestination);
}

DWORD Hook_EntryPoint(VOID *pPEB)
{
	// store function pointers
	pOrig_EntityMove = (VOID*(__stdcall*)(VOID*,VOID*))GET_EXE_ADDR(0x5F71);
	pOrig_WndProc = (LRESULT(__stdcall*)(HWND,UINT,WPARAM,LPARAM))GET_EXE_ADDR(0x80D4);

	// restore original entry-point bytes
	memcpy(pOrig_EntryPoint, gbOrigEntryPointBytes, sizeof(gbOrigEntryPointBytes));

	// install IAT hooks
	*(ULONG_PTR*)GET_EXE_ADDR(0x10CC) = (ULONG_PTR)Hook_GetPrivateProfileIntW;
	*(ULONG_PTR*)GET_EXE_ADDR(0x10D0) = (ULONG_PTR)Hook_GetPrivateProfileStringW;
	*(ULONG_PTR*)GET_EXE_ADDR(0x10EC) = (ULONG_PTR)Hook_GetCommandLineW;

	// install game hooks
	OverwriteRelative32(GET_EXE_ADDR(0x5505), Hook_EntityMove_Player);
	OverwriteRelative32(GET_EXE_ADDR(0x54E2), Hook_EntityMove_Rat);
	OverwriteRelative32(GET_EXE_ADDR(0x5FC2), Hook_EntitySpin);
	OverwriteRelative32(GET_EXE_ADDR(0x5490), Hook_BeginGame);
	OverwriteRelative32(GET_EXE_ADDR(0xBA19), Hook_WndProc);
	InlineHook(GET_EXE_ADDR(0x6035), Hook_MoveBranch);
	InlineHook(GET_EXE_ADDR(0x6089), Hook_SpinBranch);

	// force the default player direction to always be "left"
	memset(GET_EXE_ADDR(0x53FF), 0x90, 13);

	// call original entry-point
	pOrig_EntryPoint(pPEB);

	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	DWORD dwOrigProtect = 0;
	DWORD dwAddressOfEntryPoint = 0;

	if(fdwReason == DLL_PROCESS_ATTACH)
	{
		// get exe base address
		gpExeImageBase = (BYTE*)GetModuleHandleA(NULL);

		// set entire exe to RWX
		VirtualProtect((HMODULE)gpExeImageBase, 0x80000, PAGE_EXECUTE_READWRITE, &dwOrigProtect);

		// hook entry-point
		dwAddressOfEntryPoint = *(DWORD*)GET_EXE_ADDR(0xA8);
		pOrig_EntryPoint = (DWORD(*)(VOID*))((ULONG_PTR)GET_EXE_ADDR(dwAddressOfEntryPoint));
		memcpy(gbOrigEntryPointBytes, pOrig_EntryPoint, sizeof(gbOrigEntryPointBytes));
		InlineHook(pOrig_EntryPoint, Hook_EntryPoint);
	}

	return 1;
}
