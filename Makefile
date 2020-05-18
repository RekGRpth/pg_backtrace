MODULE_big = pg_backtrace
OBJS = pg_backtrace.o
PG_CONFIG = pg_config
LIBS += -lexecinfo
SHLIB_LINK := $(LIBS)
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
