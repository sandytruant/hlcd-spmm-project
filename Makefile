N ?= 4

.phony: all clean rdu
all: RedUnit PE SpMM
clean:
	rm -rf obj_dir

# Alias rdu = RedUnit, type less chars
rdu: RedUnit

define gen_verilator_target_mk
.phony: $(1)
$(1): obj_dir/$(1)/V$(1)
	$$< | tee trace/$(1)/run.log
obj_dir/$(1)/V$(1): SpMM.sv $(1).tb.cpp
	@mkdir -p obj_dir/$(1) trace/$(1)
	verilator --cc --trace --exe -Wno-fatal -Mdir obj_dir/$(1) -DN=$(N) --top $(1) $$^
	$(MAKE) -j -C obj_dir/$(1) -f V$(1).mk
endef
$(eval $(call gen_verilator_target_mk,RedUnit))
$(eval $(call gen_verilator_target_mk,PE))
$(eval $(call gen_verilator_target_mk,SpMM))
