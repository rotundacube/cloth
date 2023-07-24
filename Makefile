BIN = prog
OBJDIR := obj
SRCDIR := src
SRCS := $(wildcard $(SRCDIR)/*.cc)
OBJS := $(patsubst $(SRCDIR)/%.cc,$(OBJDIR)/%.o,$(SRCS))

CXX := g++
LINK_FLAGS += -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lglew32
CXXFLAGS += -Og -g -Wall -Wextra

$(BIN): $(OBJS)
	$(CXX) $^ $(LINK_FLAGS) -g -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cc | $(OBJDIR)
	$(CXX) -c -MMD $(CXXFLAGS) $< -o $@

$(OBJDIR):
	@mkdir -p $@

-include $(OBJDIR)/*.d

.PHONY: clean 
clean:
	del $(OBJDIR)\*
