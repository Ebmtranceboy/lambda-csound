# Separate flags for building objects for DSOs and for the main
# executable.
#
CC = gcc
CCFLAGS = -pedantic -Wall -std=c99
CCOPT_SO = $(CCFLAGS) -fpic
CCOPT_EXE = $(CCFLAGS)

CLIB_DIR = clib
PLUGIN_DIR = plugin
GENERATOR_DIR = generator

FAUST = faust

POLYPHONY = 10

all: main $(patsubst %.c,%.so,$(wildcard $(PLUGIN_DIR)/*.c)) \
	$(patsubst %.dsp,%.so,$(wildcard $(PLUGIN_DIR)/$(GENERATOR_DIR)/*.dsp))
	make room

#
# Compiling clib into objects and the clib.a archive
#
dstring.o: $(CLIB_DIR)/dstring.c
	$(CC) -c $(CCOPT_EXE) $(CLIB_DIR)/dstring.c

mem.o: $(CLIB_DIR)/mem.c $(CLIB_DIR)/mem.h
	$(CC) -c $(CCOPT_EXE) $(CLIB_DIR)/mem.c

die.o: $(CLIB_DIR)/die.c $(CLIB_DIR)/die.h
	$(CC) -c $(CCOPT_EXE) $(CLIB_DIR)/die.c

clib.a: die.o mem.o dstring.o
	ar rs clib.a dstring.o mem.o die.o

#
# Plugins
#

$(PLUGIN_DIR)/$(GENERATOR_DIR)/%.ch: $(PLUGIN_DIR)/$(GENERATOR_DIR)/%.dsp
	$(FAUST) -lang c $< > $@

$(PLUGIN_DIR)/$(GENERATOR_DIR)/%.h :$(PLUGIN_DIR)/$(GENERATOR_DIR)/%.ch
	printf "#include <glue.h>\n" > tem.p; \
	cp $< $@; awk '/typedef/,/mydsp;/' $@ >> tem.p; \
	sed -ie "s/FAUSTFLOAT/float/g" tem.p; \
	printf "mydsp* newmydsp();\n" >> tem.p; \
	printf "void initmydsp(mydsp* dsp, int samplingFreq);\n" >> tem.p; \
	printf "void computemydsp(mydsp* dsp, int count, float** inputs, float** outputs);\n" >> tem.p; \
	printf "void deletemydsp(mydsp* dsp);\n" >> tem.p; \
	mv tem.p $@

$(PLUGIN_DIR)/$(GENERATOR_DIR)/%.c :$(PLUGIN_DIR)/$(GENERATOR_DIR)/%.ch
	printf "#include <$(basename $(notdir $@)).h>\n" > tem.p; \
	cp $< $@; perl -i -p0e 's/typedef struct.*?mydsp;//s' $@; \
	cat $@ >> tem.p; \
	mv tem.p $@; sed -ie "s/inline//g" $@

$(PLUGIN_DIR)/$(GENERATOR_DIR)/glue-%.c: $(PLUGIN_DIR)/$(GENERATOR_DIR)/%.h 
	printf "#include <$(basename $(notdir $<)).h>\n" > $@; \
	sed -e "s/tt/$(basename $(notdir $<))/g" glue.c >> $@;

$(PLUGIN_DIR)/$(GENERATOR_DIR)/%.so:$(PLUGIN_DIR)/$(GENERATOR_DIR)/%.c \
                                    $(PLUGIN_DIR)/$(GENERATOR_DIR)/glue-%.c
	$(CC) -o $@ $^ -I. -I$(PLUGIN_DIR)/$(GENERATOR_DIR) -shared ; \
	mv $@ $(PLUGIN_DIR)
	for n in $(shell seq 1 $(POLYPHONY)); do \
		cp $$@ $${$$@%.so}$$n.so; \
	done

$(PLUGIN_DIR)/%.o: $(PLUGIN_DIR)/%.c
	$(CC) -c $< -o $@ $(CCOPT_SO) -I$(PLUGIN_DIR) -I.

$(PLUGIN_DIR)/%.so: $(PLUGIN_DIR)/%.o
	$(CC) -o $@ $^ -shared

#
# The main program
#
SYNTHESIZE_H = $(wildcard *.h)

%.o: %.c $(SYNTHESIZE_H)
	$(CC) -c $< $(CCOPT_EXE)

# Link with --export-dynamic, so that public symbols will be exposed
# to DSOs.
main: main.o synthesize.o db.o clib.a plugin_manager.o \
			  plugin_discovery.o
	$(CC) -o $@ $^  -ldl -Wl,--export-dynamic -lasound -lm; \

room:
	rm -rf core *.o	tem.p* $(PLUGIN_DIR)/*.o $(PLUGIN_DIR)/$(GENERATOR_DIR)/*.c \
	$(PLUGIN_DIR)/$(GENERATOR_DIR)/*.h $(PLUGIN_DIR)/$(GENERATOR_DIR)/*.o \
	$(PLUGIN_DIR)/$(GENERATOR_DIR)/*.ch $(PLUGIN_DIR)/$(GENERATOR_DIR)/glue-*.c \
	$(PLUGIN_DIR)/$(GENERATOR_DIR)/*.ce


clean:
	make room; \
	rm -rf *.a main $(PLUGIN_DIR)/*.so $(PLUGIN_DIR)/$(GENERATOR_DIR)/*.so \

