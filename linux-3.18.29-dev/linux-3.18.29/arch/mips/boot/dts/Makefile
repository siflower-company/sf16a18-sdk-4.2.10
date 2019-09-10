dtb-$(CONFIG_CAVIUM_OCTEON_SOC)		+= octeon_3xxx.dtb octeon_68xx.dtb
dtb-$(CONFIG_DT_EASY50712)		+= easy50712.dtb
dtb-$(CONFIG_DT_XLP_EVP)		+= xlp_evp.dtb
dtb-$(CONFIG_DT_XLP_SVP)		+= xlp_svp.dtb
dtb-$(CONFIG_DT_XLP_FVP)		+= xlp_fvp.dtb
dtb-$(CONFIG_DT_XLP_GVP)		+= xlp_gvp.dtb
dtb-$(CONFIG_DTB_RT2880_EVAL)		+= rt2880_eval.dtb
dtb-$(CONFIG_DTB_RT305X_EVAL)		+= rt3052_eval.dtb
dtb-$(CONFIG_DTB_RT3883_EVAL)		+= rt3883_eval.dtb
dtb-$(CONFIG_DTB_MT7620A_EVAL)		+= mt7620a_eval.dtb
dtb-$(CONFIG_MIPS_SEAD3)		   += sead3.dtb
dtb-$(CONFIG_DT_SF16A18_MPW0_P10)	+= sf16a18_mpw0_p10.dtb
dtb-$(CONFIG_DT_SF16A18_MPW0_P10_FLASH)	+= sf16a18_mpw0_p10_flash.dtb
dtb-$(CONFIG_DT_SF16A18_MPW0_P10_RN5T567)	+= sf16a18_mpw0_p10_rn5t567.dtb
dtb-$(CONFIG_DT_SF16A18_MPW0_P20)	+= sf16a18_mpw0_p20.dtb
dtb-$(CONFIG_DT_SF16A18_MPW1_P10)	+= sf16a18_mpw1_p10.dtb
dtb-$(CONFIG_DT_SF16A18_MPW1_P20)	+= sf16a18_mpw1_p20.dtb
dtb-$(CONFIG_DT_SF16A18_MPW1_FPGA)	+= sf16a18_mpw1_fpga.dtb
dtb-$(CONFIG_DT_SF19A28_FULLMASK_FPGA)	+= sf19a28_fullmask_fpga.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_P10)	+= sf16a18_fullmask_p10.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_P10M)	+= sf16a18_fullmask_p10m.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_X10)	+= sf16a18_fullmask_x10.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_P10H)	+= sf16a18_fullmask_p10h.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_86V)	+= sf16a18_fullmask_86v.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_86V_C2)	+= sf16a18_fullmask_86v_c2.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_REP_V1)	+= sf16a18_fullmask_rep_v1.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_REP_NOPA)	+= sf16a18_fullmask_rep_nopa.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_P10_FLASH)	+= sf16a18_fullmask_p10_flash.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_P10_8M)	+= sf16a18_fullmask_p10_8m.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_P20)	+= sf16a18_fullmask_p20.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_ACCTL)	+= sf16a18_fullmask_ac.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_EVB_V5)	+= sf16a18_fullmask_evb_v5.dtb
dtb-$(CONFIG_DT_SF16A18_FULLMASK_CPE)	+= sf16a18_fullmask_cpe.dtb

obj-y		+= $(patsubst %.dtb, %.dtb.o, $(dtb-y))

targets		+= dtbs
targets		+= $(dtb-y)

dtbs: $(addprefix $(obj)/, $(dtb-y))

clean-files	+= *.dtb *.dtb.S
