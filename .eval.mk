handin ?= ../handin
srcs = $(wildcard $(handin)/*.sv)

l1: $(srcs:$(handin)/%.sv=eval/l1/%.txt)
l2: $(srcs:$(handin)/%.sv=eval/l2/%.txt)

eval/l2/%.txt: $(handin)/%.sv
	@mkdir -p $(dir $@)
	+$(MAKE) -f Makefile TOP=$^ OBJ=obj_dir/$* OUT=trace/$* SCORE_PREFIX=score/l2/$*/ l2 > /dev/null 2>$@
eval/l1/%.txt: $(handin)/%.sv
	@mkdir -p $(dir $@)
	+$(MAKE) -f Makefile TOP=$^ OBJ=obj_dir/$* OUT=trace/$* SCORE_PREFIX=score/l1/$*/ l1 > /dev/null 2>$@

clean:
	rm -rf l1 l2 obj_dir trace score
