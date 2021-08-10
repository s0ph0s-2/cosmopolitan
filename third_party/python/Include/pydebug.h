#ifndef Py_LIMITED_API
#ifndef Py_PYDEBUG_H
#define Py_PYDEBUG_H
COSMOPOLITAN_C_START_
/* clang-format off */

/* These global variable are defined in pylifecycle.c */
/* XXX (ncoghlan): move these declarations to pylifecycle.h? */
PyAPI_DATA(int) Py_DebugFlag;
PyAPI_DATA(int) Py_VerboseFlag;
PyAPI_DATA(int) Py_QuietFlag;
PyAPI_DATA(int) Py_InteractiveFlag;
PyAPI_DATA(int) Py_InspectFlag;
PyAPI_DATA(int) Py_OptimizeFlag;
PyAPI_DATA(int) Py_NoSiteFlag;
PyAPI_DATA(int) Py_BytesWarningFlag;
PyAPI_DATA(int) Py_UseClassExceptionsFlag;
PyAPI_DATA(int) Py_FrozenFlag;
PyAPI_DATA(int) Py_IgnoreEnvironmentFlag;
PyAPI_DATA(int) Py_DontWriteBytecodeFlag;
PyAPI_DATA(int) Py_NoUserSiteDirectory;
PyAPI_DATA(int) Py_UnbufferedStdioFlag;
PyAPI_DATA(int) Py_HashRandomizationFlag;
PyAPI_DATA(int) Py_IsolatedFlag;

#ifdef MS_WINDOWS
PyAPI_DATA(int) Py_LegacyWindowsStdioFlag;
#endif

/* this is a wrapper around getenv() that pays attention to
   Py_IgnoreEnvironmentFlag.  It should be used for getting variables like
   PYTHONPATH and PYTHONHOME from the environment */
#define Py_GETENV(s) (Py_IgnoreEnvironmentFlag ? NULL : getenv(s))

COSMOPOLITAN_C_END_
#endif /* !Py_PYDEBUG_H */
#endif /* Py_LIMITED_API */
