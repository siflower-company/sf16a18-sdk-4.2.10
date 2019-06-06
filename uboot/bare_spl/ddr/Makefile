CFLAGS +=-DDDR -I $(BASE)/ddr3

#ifeq ($(ddr2)$(ddr), )
#nt5cc128m16ip=y
#CFLAGS += -D'DDR_MODEL="nt5cc128m16ip/em6gd16ewkg/em6gd16ewbh/m15t1g1664a"'
#endif
ifdef ddr3
CFLAGS += -DDDR3
$(ddr3):=y
CFLAGS += -D'DDR_MODEL="$(ddr3)"'
CFLAGS += -D$(ddr3)
endif

ifdef ddr2
CFLAGS += -DDDR2
$(ddr2):=y
CFLAGS += -D'DDR_MODEL="$(ddr2)"'
CFLAGS += -D$(ddr2)
endif

obj-y += ddr_init.c ddr_common.c

obj-$(em68b16cwqh) += ddr2_em68b16cwqh.c
obj-$(micron) += ddr2_micron.c
obj-$(m14d5121632a) += ddr2_em68b16cwqh.c
obj-$(hy5ps1g1631c) += ddr2_hy5ps1g1631c.c
obj-$(a3r12e40dbf) += ddr2_a3r12e40dbf.c
obj-$(n2tu51216dg) += ddr2_n2tu51216dg.c
obj-$(nt5tu32m16eg) += ddr2_nt5tu32m16eg.c
obj-$(w9751g6kb) += ddr2_w9751g6kb.c


obj-$(nt5cc128m16ip) += ddr3_nt5cc128m16ip.c
obj-$(em6gd16ewkg) += ddr3_nt5cc128m16ip.c
obj-$(em6gd16ewbh) += ddr3_nt5cc128m16ip.c
obj-$(m15t1g1664a) += ddr3_m15t1g1664a.c
obj-$(p2p2gf4alf) += ddr3_nt5cc128m16ip.c
