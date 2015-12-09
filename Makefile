CC  = gcc
CXX = g++

CCFLAGS =  -Wall  -g 

#-O3 -DNDEBUG

#-g

# -g -Wall  # -O3 -DNDEBUG


ALL_SRC_NAME  =  inspect_util   inspect_pthread   \
                 inspect_event  object_table      \
                 event_buffer                     \
                 event_relation_graph             \
                 scheduler       state            \
                 lockset         state_stack      \
                 scheduler_object_table           \
                 inspect_main                     \
                 transition_set                   \
                 thread_info                      \
                 clock_vector                     \
                 program_state                    \
                 scheduler_setting                  \
                 next_state                         \

#                 inspect_trace                     \


DEP_DIR = .deps
DEPS = $(foreach file, $(ALL_SRC_NAME), .deps/$(file).d )

LIBDIR =  ../lib
INSPECTOR_LIB = $(LIBDIR)/libinspect.a

LIB_OBJS = inspect_util.o   inspect_pthread.o      \
           inspect_event.o  object_table.o         \


SCHED_OBJS =  scheduler.o        scheduler_object_table.o \
              state.o            state_stack.o            \
              transition_set.o   lockset.o                \
              event_buffer.o     \
              thread_info.o      \
              inspect_main.o     clock_vector.o           \
              program_state.o    \
              scheduler_setting.o                         \
              next_state.o                                \
              event_relation_graph.o                      \


#   inspect_trace.o    


ALL_SRC = $(foreach src_name, $(ALL_SRC_NAME), $(src_name).cc )


all:   test_dep_dirs  dep scheduler


# 	make dep
# 	make scheduler


test_dep_dirs:
	if test -d $(DEP_DIR); then :; else mkdir $(DEP_DIR); fi


scheduler: $(INSPECTOR_LIB)  $(SCHED_OBJS)
	$(CXX) $(CCFLAGS) -o inspect  $(SCHED_OBJS) \
         -L $(LIBDIR) -linspect -lpthread -lssl


$(INSPECTOR_LIB): $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)
	ranlib $@

cleandep:
	@echo "Cleaning dependent info files .... "
	@rm -f $(DEPS)


clean:  cleandep
	@echo "Cleaning object files ...."
	@rm -f *.o
	@echo "Cleaning backup files ...."
	@rm -f *~


dep : $(DEPS)


include Makefile.dep


%.o : %.cc
#	@echo "Compiling" $^
#	@
	$(CXX) $(CCFLAGS) -c $^  -o $@

%.o : %.c
	$(CXX) $(CCFLAGS) -c $^  -o $@


.deps/%.d : %.cc
	@echo  "Generating dependency information for " $<
	@$(CXX) -MM -MF $@ -MT ext/$*.o $(USR_DEFS) $(INCDIRS)  $<
	@cat Makefile.dep $@ > Makefile.dep.new
	@mv Makefile.dep.new Makefile.dep

