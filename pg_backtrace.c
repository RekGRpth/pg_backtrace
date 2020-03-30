#include <postgres.h>

#include <fmgr.h>
#include <miscadmin.h>

#define FORMAT_0(fmt, ...) "%s(%s:%d): %s", __func__, __FILE__, __LINE__, fmt
#define FORMAT_1(fmt, ...) "%s(%s:%d): " fmt,  __func__, __FILE__, __LINE__
#define GET_FORMAT(fmt, ...) GET_FORMAT_PRIVATE(fmt, 0, ##__VA_ARGS__, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#define GET_FORMAT_PRIVATE(fmt, \
      _0,  _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, \
     _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
     _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
     _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, \
     _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, \
     _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, \
     _60, _61, _62, _63, _64, _65, _66, _67, _68, _69, \
     _70, format, ...) FORMAT_ ## format(fmt)

#define E(fmt, ...) ereport(ERROR, (errbacktrace(), errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))

PG_MODULE_MAGIC;

static pqsigfunc handlers[_NSIG];

static void handler(SIGNAL_ARGS) {
    pqsignal_no_restart(postgres_signal_arg, SIG_DFL);
    E("%s(%i)", pg_strsignal(WTERMSIG(postgres_signal_arg)), postgres_signal_arg);
}

void _PG_init(void); void _PG_init(void) {
    if (!process_shared_preload_libraries_in_progress) return;
//    handlers[SIGABRT] = pqsignal_no_restart(SIGABRT, handler);
#ifdef SIGBUS
    handlers[SIGBUS] = pqsignal_no_restart(SIGBUS, handler);
#endif
    handlers[SIGFPE] = pqsignal_no_restart(SIGFPE, handler);
    handlers[SIGILL] = pqsignal_no_restart(SIGILL, handler);
    handlers[SIGIOT] = pqsignal_no_restart(SIGIOT, handler);
    handlers[SIGSEGV] = pqsignal_no_restart(SIGSEGV, handler);
}

void _PG_fini(void); void _PG_fini(void) {
//    pqsignal_no_restart(SIGABRT, handlers[SIGABRT]);
#ifdef SIGBUS
    pqsignal_no_restart(SIGBUS,  handlers[SIGBUS]);
#endif
    pqsignal_no_restart(SIGFPE,  handlers[SIGFPE]);
    pqsignal_no_restart(SIGILL,  handlers[SIGILL]);
    pqsignal_no_restart(SIGIOT,  handlers[SIGIOT]);
    pqsignal_no_restart(SIGSEGV, handlers[SIGSEGV]);
}
