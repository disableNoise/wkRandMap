#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

_Static_assert(sizeof(void *) == 4, "Build as Win32 x86");

typedef int (__fastcall *HostCommand)(BYTE *, void *, char **, char **);
typedef int (__fastcall *EditorInit)(BYTE *, void *);
typedef void (__fastcall *EditorAction)(BYTE *, void *);
typedef void (__fastcall *OpenEditor)(BYTE *, void *, int, int);
typedef void (__fastcall *StringCtor)(char **, void *, const char *);
typedef void (__fastcall *StringAssign)(char **, void *, char **);
typedef void (__fastcall *StringDtor)(char **, void *);
typedef int (__stdcall *LobbyPrint)(void *, char *);

static BYTE *wa;
static HostCommand originalCommand;
static EditorInit originalInit;
static WCHAR installDir[MAX_PATH];
static WCHAR stagedPath[MAX_PATH];
static WCHAR chosenName[MAX_PATH];
static char stagedName[64];
static HWND editorWindow;
static BYTE *editorObject;
static UINT_PTR editorTimer;
static DWORD sequence;
static BOOL busy, loaded;

#define WA(type, address) ((type)(wa + ((address) - 0x400000)))

static void printLine(BYTE *host, const char *message)
{
    char line[512];
    snprintf(line, sizeof(line), "SYS:wkRandMap:ALL:wkRandMap: %s", message);
    WA(LobbyPrint, 0x493cb0)(host + 0x10318, line);
}

static const char *extension(const WCHAR *name)
{
    const WCHAR *dot = wcsrchr(name, L'.');
    if (!dot) return NULL;
    if (CompareStringOrdinal(dot, -1, L".png", -1, TRUE) == CSTR_EQUAL) return ".png";
    if (CompareStringOrdinal(dot, -1, L".bit", -1, TRUE) == CSTR_EQUAL) return ".bit";
    if (CompareStringOrdinal(dot, -1, L".lev", -1, TRUE) == CSTR_EQUAL) return ".lev";
    if (CompareStringOrdinal(dot, -1, L".img", -1, TRUE) == CSTR_EQUAL) return ".img";
    return NULL;
}

static BOOL sample(DWORD count, BOOL *selected)
{
    DWORD draw;
    // Rejection sampling avoids modulo bias in the reservoir.
    DWORD threshold = (DWORD)(0u - count) % count;
    do {
        if (BCryptGenRandom(NULL, (PUCHAR)&draw, sizeof(draw),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return FALSE;
    } while (draw < threshold);
    *selected = draw % count == 0;
    return TRUE;
}

static const char *stageMap(void)
{
    WCHAR ini[MAX_PATH], setting[MAX_PATH], folder[MAX_PATH], search[MAX_PATH];
    if (swprintf(ini, MAX_PATH, L"%lswkRandMap.ini", installDir) < 0)
        return "Ruta del juego demasiado larga.";

    DWORD size = GetPrivateProfileStringW(L"RandMap", L"Folder",
        L"User\\SavedLevels\\RandMap", setting, MAX_PATH, ini);
    if (!size || size >= MAX_PATH - 1) return "Revisa Folder en wkRandMap.ini.";

    BOOL unc = setting[0] == L'\\' && setting[1] == L'\\';
    BOOL drive = size >= 3 && setting[1] == L':' &&
                 (setting[2] == L'\\' || setting[2] == L'/');
    if (!unc && !drive && (setting[0] == L'\\' || setting[0] == L'/' ||
                           (size > 1 && setting[1] == L':')))
        return "Usa una ruta absoluta o relativa a WA.exe.";

    if (swprintf(folder, MAX_PATH, L"%ls%ls", unc || drive ? L"" : installDir,
                 setting) < 0 ||
        swprintf(search, MAX_PATH, L"%ls\\*", folder) < 0)
        return "Ruta de mapas demasiado larga.";

    WIN32_FIND_DATAW entry;
    HANDLE scan = FindFirstFileW(search, &entry);
    if (scan == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND ?
            "La carpeta no contiene mapas compatibles." :
            "No se pudo abrir Folder en wkRandMap.ini.";

    DWORD count = 0;
    const char *error = NULL;
    do {
        if ((entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
            (!entry.nFileSizeHigh && !entry.nFileSizeLow) || !extension(entry.cFileName))
            continue;
        BOOL selected;
        if (!sample(++count, &selected)) {
            error = "No se pudo sortear el mapa.";
            break;
        }
        if (selected) wcscpy(chosenName, entry.cFileName);
    } while (FindNextFileW(scan, &entry));
    DWORD scanError = GetLastError();
    FindClose(scan);

    if (error) return error;
    if (scanError != ERROR_NO_MORE_FILES) return "No se pudo leer toda la carpeta.";
    if (!count) return "La carpeta no contiene mapas .png, .bit, .lev ni .img.";

    WCHAR source[MAX_PATH];
    if (swprintf(source, MAX_PATH, L"%ls\\%ls", folder, chosenName) < 0)
        return "Ruta del mapa demasiado larga.";

    // The editor imports from SavedLevels. Stage under an ASCII filename
    // because the 3.8.1 frontend passes ANSI paths to its map loader.
    for (int attempt = 0; attempt < 8; ++attempt) {
        snprintf(stagedName, sizeof(stagedName), "wkRandMap_%08lx_%08lx%s",
            (unsigned long)GetCurrentProcessId(), (unsigned long)++sequence,
            extension(chosenName));
        WCHAR wideName[64];
        if (!MultiByteToWideChar(CP_ACP, 0, stagedName, -1, wideName, 64) ||
            swprintf(stagedPath, MAX_PATH, L"%lsUser\\SavedLevels\\%ls",
                     installDir, wideName) < 0)
            return "Ruta temporal demasiado larga.";
        if (CopyFileW(source, stagedPath, TRUE)) {
            // Do not inherit read-only attributes that would prevent cleanup.
            SetFileAttributesW(stagedPath, FILE_ATTRIBUTE_NORMAL);
            return NULL;
        }
        DWORD copyError = GetLastError();
        if (copyError != ERROR_FILE_EXISTS && copyError != ERROR_ALREADY_EXISTS)
            break;
    }
    return "No se pudo copiar el mapa a User\\SavedLevels.";
}

static void setGameString(char **field, const char *value)
{
    char *temporary;
    WA(StringCtor, 0x4019f0)(&temporary, NULL, value);
    WA(StringAssign, 0x401d20)(field, NULL, &temporary);
    WA(StringDtor, 0x401510)(&temporary, NULL);
}

static void CALLBACK importMap(HWND hwnd, UINT message, UINT_PTR timer, DWORD time)
{
    (void)message;
    (void)time;
    if (hwnd != editorWindow || timer != editorTimer || !busy) return;
    KillTimer(hwnd, timer);
    editorTimer = 0;

    BYTE *editor = editorObject;
    char **folder = (char **)(editor + 0x506c);
    char *savedFolder;
    WA(StringCtor, 0x4019f0)(&savedFolder, NULL, "");
    WA(StringAssign, 0x401d20)(&savedFolder, NULL, folder);

    setGameString(folder, "");
    setGameString((char **)(editor + 0x4d94), stagedName);
    int *result = (int *)(editor + 0x14c);
    *result = 0x28;
    WA(EditorAction, 0x48f2d0)(editor, NULL);
    loaded = *result == 0x27;

    WA(StringAssign, 0x401d20)(folder, NULL, &savedFolder);
    WA(StringDtor, 0x401510)(&savedFolder, NULL);
    if (!loaded) {
        // A failed import must close without saving. The native close
        // handler promotes a dirty editor to the save result.
        *(int *)(editor + 0x2584) = 0;
        *result = 0x28;
    }
    WA(EditorAction, 0x48dab0)(editor, NULL);
}

static int __fastcall onEditorInit(BYTE *editor, void *edx)
{
    int result = originalInit(editor, edx);
    if (busy && !editorWindow) {
        editorObject = editor;
        editorWindow = *(HWND *)(editor + 0x20);
        if (editorWindow) {
            editorTimer = SetTimer(editorWindow, 0x524d, 1, importMap);
            if (!editorTimer) PostMessageW(editorWindow, WM_CLOSE, 0, 0);
        }
    }
    return result;
}

static int __fastcall onHostCommand(BYTE *host, void *edx, char **command, char **args)
{
    if (!command || !*command || _stricmp(*command, "randmap"))
        return originalCommand(host, edx, command, args);
    if (args && *args && (*args)[strspn(*args, " \t")]) {
        printLine(host, "Uso: /randmap");
        return 1;
    }
    if (busy) return 1;

    const char *error = stageMap();
    if (error) {
        printLine(host, error);
        return 1;
    }

    busy = TRUE;
    loaded = FALSE;
    WA(OpenEditor, 0x4ba2f0)(host, NULL, 0, 0);
    if (editorTimer) KillTimer(editorWindow, editorTimer);
    editorTimer = 0;
    editorWindow = NULL;
    editorObject = NULL;
    busy = FALSE;
    DeleteFileW(stagedPath);

    if (loaded) {
        char name[MAX_PATH], line[MAX_PATH + 32];
        if (WideCharToMultiByte(CP_ACP, 0, chosenName, -1, name, MAX_PATH, NULL, NULL)) {
            // Colons are chat protocol separators.
            for (char *p = name; *p; ++p) if (*p == ':') *p = '_';
            snprintf(line, sizeof(line), "Mapa cargado: %s", name);
            printLine(host, line);
        } else {
            printLine(host, "Mapa cargado.");
        }
    } else {
        printLine(host, "No se pudo importar el mapa elegido.");
    }
    return 1;
}

static BOOL patchSlot(void **slot, void *replacement)
{
    DWORD protection, ignored;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection)) return FALSE;
    InterlockedExchangePointer(slot, replacement);
    VirtualProtect(slot, sizeof(*slot), protection, &ignored);
    return TRUE;
}

static BOOL install(void)
{
    wa = (BYTE *)GetModuleHandleW(NULL);
    if (!wa) return FALSE;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)wa;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(wa + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.SizeOfImage < 0x260000) return FALSE;

    void **commandSlot = WA(void **, 0x65bd24);
    void **editorSlot = WA(void **, 0x656328);
    // Check the vtables and surrounding code before writing any pointer.
    if (*commandSlot != WA(void *, 0x4b9b00) || *editorSlot != WA(void *, 0x488cc0) ||
        memcmp(WA(void *, 0x4ba2f0), "\x56\x8b\xf1\x8d\x86\x28\x05\x01\x00\x50\xe8\x51\x0f\xf9\xff", 15) ||
        memcmp(WA(void *, 0x48f2d0), "\x55\x8b\xec\x83\xe4\xf8\x6a\xff\x68", 9) ||
        memcmp(WA(void *, 0x48dab0), "\x55\x8b\xec\x83\xe4\xf8\x56\x8b\xf1\x57\x8b\xfe", 12) ||
        memcmp(WA(void *, 0x401d20), "\x8b\x44\x24\x04\x56\x57\x8b\xf9", 8))
        return FALSE;

    DWORD length = GetModuleFileNameW(NULL, installDir, MAX_PATH);
    if (!length || length >= MAX_PATH) return FALSE;
    WCHAR *slash = wcsrchr(installDir, L'\\');
    if (!slash) return FALSE;
    slash[1] = 0;

    originalCommand = (HostCommand)*commandSlot;
    originalInit = (EditorInit)*editorSlot;
    if (!patchSlot(editorSlot, onEditorInit)) return FALSE;
    if (patchSlot(commandSlot, onHostCommand)) return TRUE;
    patchSlot(editorSlot, originalInit);
    return FALSE;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (!install())
            OutputDebugStringA("wkRandMap: unsupported WA.exe or unable to install hooks.");
    }
    return TRUE;
}
