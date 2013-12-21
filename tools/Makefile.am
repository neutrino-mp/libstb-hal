bin_PROGRAMS =

if BOXTYPE_SPARK
bin_PROGRAMS += spark_fp
spark_fp_SOURCES = spark_fp.c
endif

# no longer use the compiled code...
noinst_PROGRAMS = pic2m2v
pic2m2v_SOURCES = pic2m2v.c

# ...use the script instead.
# hack...
install-exec-hook:
	install -D -m 0755 $(srcdir)/pic2m2v.sh $(DESTDIR)$(bindir)/pic2m2v
