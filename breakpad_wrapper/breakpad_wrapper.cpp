#include <cstdio>
#include <locale>
#include <codecvt>
#include <windows.h>
#include <string>
#include <vector>
#include <ostream>

#include "client/windows/handler/exception_handler.h"

#include "breakpad_wrapper.h"

static bool dumpCallback(const wchar_t* dump_path,
                         const wchar_t* minidump_id,
                         void* context,
                         EXCEPTION_POINTERS* exinfo,
                         MDRawAssertionInfo* assertion,
                         bool succeeded)
{
    printf("Dump path: %s/%s.dmp\n", dump_path, minidump_id);

    return succeeded;
}

bool filterCallback(void* context, EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion)
{
    printf("filterCallback\n");
    return true;
}

extern "C" CExceptionHandler newCExceptionHandler(const char *dump_path)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    const std::wstring dump_path_w = converter.from_bytes(dump_path);

    return reinterpret_cast<void *>(
            new google_breakpad::ExceptionHandler(dump_path_w,
                                                  filterCallback,
                                                  NULL,
                                                  NULL,
                                                  (int)google_breakpad::ExceptionHandler::HANDLER_ALL));
}

extern "C" void removeCExceptionHandler(CExceptionHandler handler)
{
    google_breakpad::ExceptionHandler* google_breakpad_handler =
            reinterpret_cast<google_breakpad::ExceptionHandler*>(handler);

    delete google_breakpad_handler;
}
