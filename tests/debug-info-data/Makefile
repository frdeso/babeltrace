OBJS=dwarf_full/libhello_so elf_only/libhello_so build_id/libhello_so debug_link/libhello_so

BUILD_ID_PREFIX=cd
BUILD_ID_SUFFIX=d98cdd87f7fe64c13b6daad553987eafd40cbb
BUILD_ID=$(BUILD_ID_PREFIX)$(BUILD_ID_SUFFIX)

all: $(OBJS)

%.o: %.c
	$(CC) -gdwarf -fdebug-prefix-map=$(CURDIR)=. -fPIC -c -I. -o $@ $<

# Master copy: ELF with DWARF and build-id
dwarf_full/libhello_so: tp.o libhello.o
	mkdir -p $(@D)
	$(CC) -shared -g -llttng-ust -ldl -Wl,-soname,libhello.so -Wl,--build-id=0x$(BUILD_ID) -o $@ $^

# ELF only, no debug symbols, no build-d
elf_only/libhello_so: dwarf_full/libhello_so
	mkdir -p $(@D)
	objcopy -g $< $@.tmp
	objcopy --remove-section=.note.gnu.build-id $@.tmp
	mv $@.tmp $@

# ELF with external build-id DWARF
build_id/libhello_so: dwarf_full/libhello_so
	mkdir -p $(@D)/.build-id/$(BUILD_ID_PREFIX)
	objcopy --only-keep-debug $< $(@D)/.build-id/$(BUILD_ID_PREFIX)/$(BUILD_ID_SUFFIX).debug
	objcopy -g $< $@

# ELF with external debug link DWARF
debug_link/libhello_so: dwarf_full/libhello_so
	mkdir -p $(@D)
	objcopy --remove-section=.note.gnu.build-id $< $@.tmp
	objcopy --only-keep-debug $@.tmp $(@D)/libhello_so.debug
	objcopy -g $@.tmp
	cd $(@D) && objcopy --add-gnu-debuglink=libhello_so.debug $(@F).tmp
	mv $@.tmp $@

clean:
	rm -f *.o

dist-clean: clean
	rm -f $(OBJS)

.PHONY: all clean dist-clean
