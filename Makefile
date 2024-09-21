NAME = bf_interp
VERSION = 3.8

ALL = bf2c bf_interp bf_jit

all: $(ALL)

bf_jit:
	$(CC) src/$@.c -Os -o $@
	@strip $@

%: src/%.c
	$(CC) $< -O3 -o $@
	@strip $@

clean:
	rm $(ALL)
