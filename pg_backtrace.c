#include <execinfo.h>
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

PG_MODULE_MAGIC;

static pqsigfunc handlers[_NSIG];

static void handler(SIGNAL_ARGS) {
    int ret;
    size_t size;
    void *buffer[100];
    unw_cursor_t cursor;
    unw_context_t uc;
    elog(LOG, "postgres_signal_arg = %d (%s)", postgres_signal_arg, pg_strsignal(WTERMSIG(postgres_signal_arg)));
    pqsignal_no_restart(SIGILL, SIG_DFL);
    size = backtrace(buffer, 100);
    backtrace_symbols_fd(buffer, size, STDERR_FILENO);
    if ((ret = unw_getcontext(&uc)) != UNW_ESUCCESS) ereport(ERROR, (errmsg("%s(%s:%d): unw_getcontext != UNW_ESUCCESS", __func__, __FILE__, __LINE__)));
    if ((ret = unw_init_local(&cursor, &uc)) != 0) ereport(ERROR, (errmsg("%s(%s:%d): unw_init_local != 0", __func__, __FILE__, __LINE__)));
    for (int nptrs = 0; unw_step(&cursor) > 0; nptrs++) {
        char fname[128] = { '\0', };
        unw_word_t ip, sp, offp;
        unw_get_proc_name(&cursor, fname, sizeof(fname), &offp);
        if ((ret = unw_get_reg(&cursor, UNW_REG_IP, &ip)) != 0) ereport(ERROR, (errmsg("%s(%s:%d): unw_get_reg != 0", __func__, __FILE__, __LINE__)));
        if ((ret = unw_get_reg(&cursor, UNW_REG_SP, &sp)) != 0) ereport(ERROR, (errmsg("%s(%s:%d): unw_get_reg != 0", __func__, __FILE__, __LINE__)));
        if (!strcmp(fname, "__restore_rt")) continue;
        if (!strcmp(fname, "__libc_start_main")) break;
        elog(LOG, "\t#%02d: 0x"REGFORMAT" in %s(), sp = 0x"REGFORMAT, nptrs, (long) ip, fname[0] ? fname : "??", (long)sp);
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
