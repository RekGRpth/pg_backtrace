MODULE_big = pg_backtrace
OBJS = pg_backtrace.o
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
