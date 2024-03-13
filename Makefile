PROJECT=R64000
PROFILE?=RELEASE

SRC=$(wildcard src/*.c)
OBJ=$(patsubst %.c,build/$(PROFILE)/%.o,$(SRC))
DEP=$(patsubst %.c,build/$(PROFILE)/%.d,$(SRC))

CFLAGS+=-D_DEFAULT_SOURCE
CFLAGS+=-Wall -Wshadow -std=c2x -ggdb
CFLAGS+=$(shell sdl2-config --cflags)
CFLAGS+=-MMD -MP -MF $(@:.o=.d)
CFLAGS_RELEASE+=-O3
CFLAGS+=$(CFLAGS_$(PROFILE))

LDFLAGS+=$(shell sdl2-config --libs) -lSDL2_image
LDFLAGS+=-lm

$(PROJECT): build/$(PROFILE)/$(PROJECT)
	@cp $< $@

build/$(PROFILE)/$(PROJECT): $(OBJ)
	@mkdir -p $(dir $@)
	@echo $@
	@$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

build/$(PROFILE)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo $@
	@$(CC) $(CFLAGS) -c -o $@ $<

.phony: run
run: build/$(PROFILE)/$(PROJECT)
	@$<

.phony: clean
clean:
	@rm -Rf build

-include Makefile.site
-include $(DEP)
