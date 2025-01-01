N ?= 4

.phony: all clean clean-trace rdu
all: RedUnit PE SpMM SpMM2
clean:
	rm -rf obj_dir
clean-trace:
	rm -rf trace

# Alias rdu = RedUnit, type less chars
rdu: RedUnit

final: PE2 SpMM2

define gen_verilator_target_mk
.phony: $(1)
$(1): obj_dir/$(1)/V$(2)
	@mkdir -p trace/$(1)
	$$< | tee trace/$(1)/run.log
obj_dir/$(1)/V$(2): SpMM.sv $(1).tb.cpp
	@mkdir -p obj_dir/$(1)
	verilator --cc --trace --exe -Wno-fatal -Mdir obj_dir/$(1) -DN=$(N) --top $(2) $$^
	$(MAKE) -j`nproc` -C obj_dir/$(1) -f V$(2).mk
endef
$(eval $(call gen_verilator_target_mk,RedUnit,RedUnit))
$(eval $(call gen_verilator_target_mk,PE2,PE))
$(eval $(call gen_verilator_target_mk,PE,PE))
$(eval $(call gen_verilator_target_mk,SpMM,SpMM))
$(eval $(call gen_verilator_target_mk,SpMM2,SpMM))
