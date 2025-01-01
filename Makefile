N ?= 4

.phony: all clean clean-trace rdu
all: RedUnit PE SpMM
l1: RedUnit PE SpMM
l2: score/score-l2 PE2 SpMM2
	score/score-l2

clean:
	rm -rf obj_dir
clean-trace:
	rm -rf trace

# Alias rdu = RedUnit, type less chars
rdu: RedUnit

score/score-l2: score-l2.cpp
	@mkdir -p score
	g++ -O2 $^ -o $@

define gen_verilator_target_mk
.phony: $(1)
$(1): obj_dir/$(1)/V$(2)
	@mkdir -p trace/$(1) score
	$$< | tee trace/$(1)/run.log
obj_dir/$(1)/V$(2): SpMM.sv $(1).tb.cpp
	@mkdir -p obj_dir/$(1)
	verilator --cc --trace --exe -Wno-fatal -Mdir obj_dir/$(1) -DN=$(N) --top $(2) $$^
	$(MAKE) -j`nproc` -C obj_dir/$(1) -f V$(2).mk CFLAGS=-g CXXFLAGS=-g
endef
$(eval $(call gen_verilator_target_mk,RedUnit,RedUnit))
$(eval $(call gen_verilator_target_mk,PE2,PE))
$(eval $(call gen_verilator_target_mk,PE,PE))
$(eval $(call gen_verilator_target_mk,SpMM,SpMM))
$(eval $(call gen_verilator_target_mk,SpMM2,SpMM))
