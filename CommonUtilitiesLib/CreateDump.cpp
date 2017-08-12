#ifdef _WIN32

#include "CreateDump.h"


//����dump�ļ�
void CreateDumpFile(LPCSTR lpstrDumpFilePathName, EXCEPTION_POINTERS *pException)
{
    //����Dump�ļ�

    HANDLE hDumpFile = CreateFile(lpstrDumpFilePathName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    //Dump��Ϣ
    MINIDUMP_EXCEPTION_INFORMATION	dumpInfo;
    dumpInfo.ExceptionPointers	=	pException;
    dumpInfo.ThreadId			=	GetCurrentThreadId();
    dumpInfo.ClientPointers		=	TRUE;

    //д��Dump�ļ�����
    //MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL);
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithFullMemory, &dumpInfo, NULL, NULL);

    CloseHandle(hDumpFile);

    /*
    wchar_t wszErr[512] = {0,};
    wsprintf(wszErr, TEXT("��־�ļ�: %s\n�쳣����: %0x%8.8X\n�쳣��־\n�쳣��ַ: %0x%8.8X\n\n�뽫���ļ������������."),
        lpstrDumpFilePathName,
        pException->ExceptionRecord->ExceptionCode,
        pException->ExceptionRecord->ExceptionFlags,
        pException->ExceptionRecord->ExceptionAddress);
*/
    //FatalAppExit(-1, wszErr);
}


#endif
