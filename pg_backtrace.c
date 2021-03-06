#include <postgres.h>

#include <fmgr.h>
#include <miscadmin.h>
#include <libunwind.h>
#include <dlfcn.h>

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

#define E(fmt, ...) ereport(ERROR, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))

PG_MODULE_MAGIC;

static pqsigfunc handlers[_NSIG];

static void handler(SIGNAL_ARGS) {
    unw_cursor_t cursor;
    unw_context_t context;
    if (unw_getcontext(&context) != UNW_ESUCCESS) E("unw_getcontext != UNW_ESUCCESS");
    if (unw_init_local(&cursor, &context)) E("unw_init_local");
    for (int nptrs = 0; unw_step(&cursor) > 0; nptrs++) {
        char name[128] = { '\0', };
        unw_word_t ip, off;
        Dl_info info;
        if (unw_get_proc_name(&cursor, name, sizeof(name), &off)) E("unw_get_proc_name");
        if (unw_get_reg(&cursor, UNW_REG_IP, &ip)) E("unw_get_reg");
        if (!dladdr((void *)ip, &info)) E("dladdr");
        pg_printf("#%02d: 0x%lx <%s+%li> at %s\n", nptrs, (long)ip, name[0] ? name : "???", (long)off, info.dli_fname);
    }
    E("%s(%i)", pg_strsignal(WTERMSIG(postgres_signal_arg)), postgres_signal_arg);
}

void _PG_init(void); void _PG_init(void) {
    if (!process_shared_preload_libraries_in_progress) return;
    handlers[SIGABRT] = pqsignal(SIGABRT, handler);
#ifdef SIGBUS
    handlers[SIGBUS] = pqsignal(SIGBUS, handler);
#endif
    handlers[SIGFPE] = pqsignal(SIGFPE, handler);
    handlers[SIGILL] = pqsignal(SIGILL, handler);
    handlers[SIGIOT] = pqsignal(SIGIOT, handler);
    handlers[SIGSEGV] = pqsignal(SIGSEGV, handler);
}

void _PG_fini(void); void _PG_fini(void) {
    pqsignal(SIGABRT, handlers[SIGABRT]);
#ifdef SIGBUS
    pqsignal(SIGBUS,  handlers[SIGBUS]);
#endif
    pqsignal(SIGFPE,  handlers[SIGFPE]);
    pqsignal(SIGILL,  handlers[SIGILL]);
    pqsignal(SIGIOT,  handlers[SIGIOT]);
    pqsignal(SIGSEGV, handlers[SIGSEGV]);
}
