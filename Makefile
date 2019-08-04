# if any -O... is explicitly requested, use that; otherwise, do -O3
ifeq (,$(filter -O%,$(CXXFLAGS)))
  FLAGS_OPTIMIZATION := -O3 -ffast-math
endif

CXXFLAGS += -g $(FLAGS_OPTIMIZATION) -Wall -Wextra -MMD -Wno-missing-field-initializers -pthread
CXXFLAGS += -Wno-unused-parameter -Wno-unused-variable

LDLIBS += -lfltk -lpthread -pthread

all: drive
.PHONY: all

drive: drive.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include *.d
