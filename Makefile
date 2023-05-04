PROG = project05
OBJS = project05v2.o
TMP = $(PROG) $(OBJS) *.html *.tmp

%.o: %.c
	gcc -c -g -o $@ $<

$(PROG): $(OBJS)
	gcc -g -o $@ $^

clean:
	rm -rf $(TMP)
