static void 
Win32OutputWSAErrorCode(LPSTR Message)
{ 
    LPSTR ErrorMessage;
    DWORD LastError = WSAGetLastError(); 

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        LastError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&ErrorMessage,
        0, NULL );

    int Characters = (lstrlen(ErrorMessage) + lstrlen(Message) + 40);
    LPTSTR MessageBuffer = (LPTSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Characters * sizeof(CHAR));
    StringCchPrintfA(MessageBuffer, Characters, "%s Error failed with error code %d: %s", 
        Message, LastError, ErrorMessage); 

    MessageBox(NULL, MessageBuffer, "Error", MB_OK); 

    LocalFree(ErrorMessage);
    HeapFree(GetProcessHeap(), 0, MessageBuffer);
}

static void 
Win32OutputErrorCode(LPSTR Message)
{ 
    LPSTR ErrorMessage;
    DWORD LastError = GetLastError(); 

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        LastError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&ErrorMessage,
        0, NULL );

    int Characters = (lstrlen(ErrorMessage) + lstrlen(Message) + 40);
    LPTSTR MessageBuffer = (LPTSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Characters * sizeof(CHAR));
    StringCchPrintfA(MessageBuffer, Characters, "%s Error failed with error code %d: %s", 
        Message, LastError, ErrorMessage); 

    MessageBox(NULL, MessageBuffer, "Error", MB_OK); 

    LocalFree(ErrorMessage);
    HeapFree(GetProcessHeap(), 0, MessageBuffer);
    ExitProcess(LastError); 
}

static void
Win32OutputError(char *Title, char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    unsigned int RequiredCharacters = vsnprintf(0, 0, Format, Args) + 1;
    va_end(Args);

    static char Text[4096] = {0};
    if (RequiredCharacters > 4096)
    {
        RequiredCharacters = 4096;
    }

    va_start(Args, Format);
    vsnprintf(Text, RequiredCharacters, Format, Args);
    va_end(Args);

    Text[RequiredCharacters - 1] = 0;
    MessageBoxA(0, Text, Title, MB_OK);
}