BIN = prog
OBJDIR := obj
SRCDIR := src
SITEDIR := site
SRCS := $(wildcard $(SRCDIR)/*.cc)
OBJS := $(patsubst $(SRCDIR)/%.cc,$(OBJDIR)/%.o,$(SRCS))

CXX := g++
LINK_FLAGS += -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lglew32
CXXFLAGS += -Wall -Wextra -fno-rtti -fno-exceptions

ifeq ($(OMODE), RELEASE)
	CXXFLAGS += -O2
else
	LINK_FLAGS += -g
	CXXFLAGS += -Og -g
endif

$(BIN): $(OBJS)
	$(CXX) $^ $(LINK_FLAGS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cc | $(OBJDIR)
	$(CXX) -c -MMD $(CXXFLAGS) $< -o $@

$(OBJDIR):
	@mkdir -p $@

-include $(OBJDIR)/*.d

.PHONY: clean 
clean:
	del $(OBJDIR)\*

wasm:
	emcc -std=c++11 $(SRCS) $(CXXFLAGS) \
	-sEXPORTED_RUNTIME_METHODS=ccall,cwrap \
	-s USE_SDL=2 -s FULL_ES2=1 -o $(SITEDIR)/index.js