N ?= 16

TOP ?= SpMM.sv
OBJ ?= obj_dir
OUT ?= trace
SCORE_PREFIX ?= "score/"

.phony: all clean clean-trace rdu
all: RedUnit PE SpMM
l1: RedUnit PE SpMM
l2: $(SCORE_PREFIX)/score-l2 PE2 SpMM2
	$<

clean:
	rm -rf $(OBJ)
clean-trace:
	rm -rf $(OUT)

# Alias rdu = RedUnit, type less chars
rdu: RedUnit

$(SCORE_PREFIX)/score-l2: score-l2.cpp
	@mkdir -p $(SCORE_PREFIX)/
	g++ -O2 $^ -DSCORE_PREFIX="\"$(SCORE_PREFIX)\"" -o $@

define gen_verilator_target_mk
.phony: $(1)
$(1): $(OBJ)/$(1)/V$(2)
	@mkdir -p $(OUT)/$(1) score
	$$< | tee $(OUT)/$(1)/run.log
$(OBJ)/$(1)/V$(2): $(TOP) $(1).tb.cpp
	@mkdir -p $(OBJ)/$(1) $(SCORE_PREFIX)
	verilator --cc --trace  --trace-max-array 1024 --trace-max-width 1024 --trace-depth 99 --exe -Wno-fatal -Mdir $(OBJ)/$(1) -DN=$(N) -CFLAGS "-DSCORE_PREFIX=\"\\\"$(SCORE_PREFIX)\\\"\"" --top $(2) $$^
	+$(MAKE) -C $(OBJ)/$(1) -f V$(2).mk
endef
$(eval $(call gen_verilator_target_mk,RedUnit,RedUnit))
$(eval $(call gen_verilator_target_mk,PE2,PE))
$(eval $(call gen_verilator_target_mk,PE,PE))
$(eval $(call gen_verilator_target_mk,SpMM,SpMM))
$(eval $(call gen_verilator_target_mk,SpMM2,SpMM))
