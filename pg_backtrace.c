//#include <execinfo.h>
#include <unistd.h>
#include <libunwind.h> /* from -llibuwind */
#include <postgres.h>

#include <access/xact.h>
#include <catalog/pg_type.h>
#include <commands/async.h>
#include <commands/dbcommands.h>
#include <commands/defrem.h>
#include <executor/spi.h>
#include <fmgr.h>
#include <libpq/libpq-be.h>
#include <miscadmin.h>
#include <nodes/makefuncs.h>
#include <pgstat.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/pmsignal.h>
#include <storage/procarray.h>
#include <tcop/tcopprot.h>
#include <utils/builtins.h>
#include <utils/guc.h>
#include <utils/lsyscache.h>
#include <utils/memutils.h>
#include <utils/ps_status.h>
#include <utils/snapmgr.h>
#include <utils/syscache.h>
#include <utils/timeout.h>
#include <utils/varlena.h>

#if defined(REG_RIP)
# define SIGSEGV_STACK_IA64
# define REGFORMAT "%016lx"
#elif defined(REG_EIP)
# define SIGSEGV_STACK_X86
# define REGFORMAT "%08x"
#else
# define SIGSEGV_STACK_GENERIC
# define REGFORMAT "%x"
#endif

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
#define F(fmt, ...) ereport(FATAL, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define L(fmt, ...) ereport(LOG, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define W(fmt, ...) ereport(WARNING, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))

PG_MODULE_MAGIC;

static pqsigfunc handlers[_NSIG];

static void handler(SIGNAL_ARGS) {
//    int ret;
//    size_t size;
//    void *buffer[100];
    unw_cursor_t cursor;
    unw_context_t context;
    L("postgres_signal_arg = %d (%s)", postgres_signal_arg, pg_strsignal(WTERMSIG(postgres_signal_arg)));
    pqsignal_no_restart(SIGILL, SIG_DFL);
//    size = backtrace(buffer, 100);
//    backtrace_symbols_fd(buffer, size, STDERR_FILENO);
    if (unw_getcontext(&context) != UNW_ESUCCESS) E("unw_getcontext != UNW_ESUCCESS");
    if (unw_init_local(&cursor, &context)) E("unw_init_local");
//    if (unw_init_local2(&cursor, &context, UNW_INIT_SIGNAL_FRAME)) E("unw_init_local2");
    for (int i = 0; unw_step(&cursor) > 0; i++) {
        char fname[128] = { '\0', };
        unw_word_t ip, sp, offp;
        unw_get_proc_name(&cursor, fname, sizeof(fname), &offp);
        if (unw_get_reg(&cursor, UNW_REG_IP, &ip)) E("unw_get_reg");
        if (unw_get_reg(&cursor, UNW_REG_SP, &sp)) E("unw_get_reg");
        if (!strcmp(fname, "__restore_rt")) continue;
        if (!strcmp(fname, "__libc_start_main")) break;
        L("\t#%02d: 0x"REGFORMAT" in %s(), sp = 0x"REGFORMAT, i, (long)ip, fname[0] ? fname : "??", (long)sp);
    }
//    pqsignal_no_restart(postgres_signal_arg, handlers[postgres_signal_arg]);
//    kill(MyProcPid, postgres_signal_arg);
}

void _PG_init(void); void _PG_init(void) {
    if (!process_shared_preload_libraries_in_progress) return;
    handlers[SIGABRT] = pqsignal_no_restart(SIGABRT, handler);
#ifdef SIGBUS
    handlers[SIGBUS] = pqsignal_no_restart(SIGBUS, handler);
#endif
    handlers[SIGFPE] = pqsignal_no_restart(SIGFPE, handler);
    handlers[SIGILL] = pqsignal_no_restart(SIGILL, handler);
    handlers[SIGIOT] = pqsignal_no_restart(SIGIOT, handler);
    handlers[SIGSEGV] = pqsignal_no_restart(SIGSEGV, handler);
}

void _PG_fini(void); void _PG_fini(void) {
    pqsignal_no_restart(SIGABRT, handlers[SIGABRT]);
#ifdef SIGBUS
    pqsignal_no_restart(SIGBUS,  handlers[SIGBUS]);
#endif
    pqsignal_no_restart(SIGFPE,  handlers[SIGFPE]);
    pqsignal_no_restart(SIGILL,  handlers[SIGILL]);
    pqsignal_no_restart(SIGIOT,  handlers[SIGIOT]);
    pqsignal_no_restart(SIGSEGV, handlers[SIGSEGV]);
}
