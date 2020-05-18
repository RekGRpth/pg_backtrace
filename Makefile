MODULE_big = pg_backtrace
OBJS = pg_backtrace.o
PG_CONFIG = pg_config
LIBS += -lunwind -lunwind-x86_64
SHLIB_LINK := $(LIBS)
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
