#ifndef BREAKPAD_WRAPPER_H
#define BREAKPAD_WRAPPER_H

typedef void* CExceptionHandler;

#ifdef __cplusplus
extern "C"
{
#endif
CExceptionHandler newCExceptionHandler(const char *dump_path);
void removeCExceptionHandler(CExceptionHandler handler);
#ifdef __cplusplus
}
#endif

#endif  /* BREAKPAD_WRAPPER_H*/