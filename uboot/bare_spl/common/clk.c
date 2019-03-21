#include <io.h>
#include <sf_mmap.h>
#include <clk.h>

#ifdef SFA18_CRYSTAL_6M
static int clk_use_crystal_6m(void)
{
	int i, para = 0;

	for (i = 0; i < 4; i++) {
		/* calculate new parameters */
		para = readb(CPU_PLL_PARA70 + i * 0x40);
		para += readb(CPU_PLL_PARA158 + i * 0x40) << 8;
		para *= 2;

		if (i == 0) {
			/* switch cpu clk to cmn pll */
			writeb(9, CPU_CLK_DIV);
			writeb(2, CPU_CLK_CONFIG);
		}

		if (i == 2) {
			/* switch pbus clk to cpu pll */
			writeb(0, PBUS_CLK_CONFIG);
		}

		/*
		 * 1.power down pll
		 * 2.clear load bit
		 * 3.set pll parameters
		 * 4.set load bit
		 * 5.clear power down bit
		 * 6.check lock & stable bit
		 */
		writeb(0x3b, CPU_PLL_POWER + i * 0x40);
		writeb(0x18, CPU_PLL_CONFIG + i * 0x40);
		writeb(para & 0xff, CPU_PLL_PARA70 + i * 0x40);
		writeb(para >> 8, CPU_PLL_PARA158 + i * 0x40);
		writeb(0x38, CPU_PLL_CONFIG + i * 0x40);
		writeb(0x3a, CPU_PLL_POWER + i * 0x40);
		while(!readb(CPU_PLL_POWER + i * 0x40 + 0x20));

		if (i == 2) {
			/* switch pbus clk back to cmn pll */
			writeb(2, PBUS_CLK_CONFIG);
		}

		if (i == 0) {
			/* switch cpu clk back to cpu pll */
			writeb(0, CPU_CLK_CONFIG);
			writeb(1, CPU_CLK_DIV);
		}

	}

	return 0;
}
#endif

int clk_gating_init(void)
{
	writeb(readb(CPU_PLL_CONFIG) | (1 << 4) | (1 << 3), CPU_PLL_CONFIG);
	writeb(readb(DDR_PLL_CONFIG) | (1 << 4) | (1 << 3), DDR_PLL_CONFIG);
	writeb(readb(CMN_PLL_CONFIG) | (1 << 4) | (1 << 3), CMN_PLL_CONFIG);
	writeb(readb(SPC_PLL_CONFIG) | (1 << 4) | (1 << 3), SPC_PLL_CONFIG);
#ifdef SFA18_CRYSTAL_6M
	clk_use_crystal_6m();
#endif
	writeb(0xFF, BUS1_XN_CLK_ENABLE);
	writeb(0xFF, BUS2_XN_CLK_ENABLE);
	writeb(0xFF, BUS3_XN_CLK_ENABLE);
	writeb(0xFF, CPU_CLK_ENABLE);
	writeb(0xFF, PBUS_CLK_ENABLE);
	writeb(0xFF, MEM_PHY_CLK_ENABLE);
	writeb(0x4, MEM_PHY_CLK_DIV);
	writeb(0xFF, AUDIO_EXTCLK_ENABLE);
	writeb(0xFF, UART_EXTCLK_ENABLE);
	writeb(0xFF, SPDIF_EXTCLK_ENABLE);
	writeb(0xFF, SDHC_EXT_CLK_ENABLE);
	writeb(0xFF, EMMC_EXT_CLK_ENABLE);
	// writeb(0xFF, GDU0_CLK_ENABLE);
	// writeb(0xFF, GDU0_EITF_CLK_ENABLE);
	// writeb(0xFF, TVIF0_CLK_ENABLE);
	writeb(0xFF, ETH_REF_CLK_ENABLE);
	writeb(0xFF, ETH_BYP_REF_CLK_ENABLE);
	writeb(0xFF, ETH_TSU_CLK_ENABLE);
	#ifndef MPW0
	writeb(0xFF, NPU_PE_CLK_ENABLE);
	#endif
	writeb(0xFF, ETH_BYP_REF_CLK_ENABLE);
	writeb(0xFF, ETH_TSU_CLK_ENABLE);
	/* set 50MHZ clock of ptp clock */
	writeb(0x1D, ETH_TSU_CLK_DIV);
	#ifdef ENH_EXT_OMINI_PHY
	/* for ext phy only support which need 50MHZ clock */
	writeb(0x1D, ETH_BYP_REF_CLK_DIV);
	#endif
	writeb(0xFF, WLAN24_MAC_WT_CLK_ENABLE);
	writeb(0xFF, WLAN5_MAC_WT_CLK_ENABLE);
	writeb(0xFF, USBPHY_REF_CLK_ENABLE);
	/* close tclk to avoid clk output in GPIO62. */
	writeb(0x00, TCLK_ENABLE);
	writeb(0xFF, CRYPTO_CLK_ENABLE);

	return 0;
}

#ifndef MPW0
struct clk_info {
	int offset;
	int src;
	int ratio;
	int enable;
};

int clk_update(void)
{
	int i;
#ifdef FULLMASK
	struct clk_info clk[] = {
		{  0, 3, 4, 1 }, /* BUS1_CLK */
		{ 20, 0, 1, 1 }, /* M6251_0_CLK */
		{ 21, 0, 1, 1 }, /* M6251_1_CLK */
		{ -1, -1, -1, -1 },
	};
#else
	struct clk_info clk[] = {
		{ 20, 0, 3, 1 }, /* M6251_0_CLK */
		{ 21, 0, 3, 1 }, /* M6251_1_CLK */
		{ 22, 3, 5, 1 }, /* WLAN_24_CLK */
		{ 23, 3, 5, 1 }, /* WLAN_5_CLK */
		{ 27, 2, 4, 1 }, /* GDU_CLK */
		{ 28, 2, 4, 1 }, /* GDU_EITF_CLK */
		{ 29, 2, 4, 1 }, /* TVIF_CLK */
		{ -1, -1, -1, -1 },
	};
#endif

	for (i = 0; clk[i].offset != -1; i++) {
		if (clk[i].enable) {
			writel(clk[i].ratio, CLK_BASE
						+ clk[i].offset * CLK_OFFSET
						+ CLK_REG_WID);
			writel(clk[i].src, CLK_BASE + clk[i].offset * CLK_OFFSET);
		}
	}

#ifndef FULLMASK
	/* switch cpu clk to cmn pll */
	writeb(9, CPU_CLK_DIV);
	writeb(2, CPU_CLK_CONFIG);

	/*
	 * 1.power down pll
	 * 2.clear load bit
	 * 3.set pll parameters
	 * 4.set load bit
	 * 5.clear power down bit
	 * 6.check lock & stable bit
	 */
	writeb(0x3b, CPU_PLL_POWER);
	writeb(0x18, CPU_PLL_CONFIG);
	writeb(0xfa, CPU_PLL_PARA70); /* 1000MHZ */
	writeb(0x00, CPU_PLL_PARA158);
	writeb(0x38, CPU_PLL_CONFIG);
	writeb(0x3a, CPU_PLL_POWER);
#ifndef FPGA
	while(!readb(CPU_PLL_POWER));
#endif
	/* switch cpu clk back to cpu pll */
	writeb(0, CPU_CLK_CONFIG);
	writeb(1, CPU_CLK_DIV); /* 1:2 */
#endif /* FULLMASK */
	return 0;
}
#endif
