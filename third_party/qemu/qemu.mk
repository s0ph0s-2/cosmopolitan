#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

o/third_party/qemu/qemu-aarch64:				\
		third_party/qemu/qemu-aarch64.gz		\
		o/$(MODE)/tool/build/gzip.com
	@$(MKDIR) $(@D)
	@o/$(MODE)/tool/build/gzip.com $(ZFLAGS) -cd <$< >$@
