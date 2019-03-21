/*
 * SF16A18 clocks.
 *
 * Exposes all configurable internal clock sources to the clk framework.
 *
 * We have:
 *  - Root source, usually 12MHz supplied by an external crystal
 *  - 4 PLLs which generate multiples of root rate [CPU, DDR, CMN, SPC]
 *
 * Dividers:
 *  - 22 clock dividers with:
 *   * selectable source [one of the PLLs],
 *   * output divided between [1 .. 256]
 *   * can be enabled individually.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clk-private.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>


static DEFINE_SPINLOCK(cm_cfg_lock);

#if 0
typedef enum cm_pll_t{
	CPU_PLL,
	DDR_PLL,
	CMN_PLL,
	SPC_PLL,
}sf16a28_pll;
#endif

typedef enum cm_cfg_t{
	BUS1_CFG,
	BUS2_CFG,
	BUS3_CFG,
	CPU_CFG,
	PBUS_CFG,
	MEMPHY_CFG,
	AUDIO_CFG,
	UART_CFG,
	SPDIF_CFG,
	SDIO_CFG,
	EMMC_CFG,
	ETH_REF_CFG,
	ETH_BYP_REF_CFG,
	ETH_TSU_CFG,
	WLAN24_CFG,
	WLAN5_CFG,
	USBPHY_CFG,
	TCLK_CFG,
	NPU_PE_CFG,
	GDU0_CFG,
	GDU0_EITF_CFG,
	TVIF0_CFG
}sf16a28_cfg;

#if 0
/*
 *	CM_PLL_CLK init.
 */
static struct of_device_id sfa18_cpu_pll_of_match[] = {
	{
		.compatible = "siflower,sf16a28-cpupll-clk",
	},
	{}
};
static struct of_device_id sfa18_ddr_pll_of_match[] = {
	{
		.compatible = "siflower,sf16a28-ddrpll-clk",
	},
	{}
};
static struct of_device_id sfa18_cmn_pll_of_match[] = {
	{
		.compatible = "siflower,sf16a28-cmnpll-clk",
	},
	{}
};
static struct of_device_id sfa18_spc_pll_of_match[] = {
	{
		.compatible = "siflower,sf16a28-spcpll-clk",
	},
	{}
};

static void sf16a28_pll_setup(struct device_node *np,int pll_type)
{
	const struct of_device_id *match;
	struct clk *clk;
	const char *parent_name;
	const char *clk_name;

	if(!np)
	{
		pr_err("%s +%d %s: Device node not available for pll %d.\n",__FILE__,__LINE__,__func__,pll_type);
		return;
	}

	switch(pll_type){
		case CPU_PLL:
			match = of_match_node(sfa18_cpu_pll_of_match, np);
			break;
		case DDR_PLL:
			match = of_match_node(sfa18_ddr_pll_of_match, np);
			break;
		case CMN_PLL:
			match = of_match_node(sfa18_cmn_pll_of_match, np);
			break;
		case SPC_PLL:
			match = of_match_node(sfa18_spc_pll_of_match, np);
			break;
		default:
			pr_err("%s +%d %s: No such pll_type: %d.\n",__FILE__,__LINE__,__func__,pll_type);
			return;
	}

	if (!match) {
		pr_err("%s +%d %s: No matching data.\n",__FILE__,__LINE__,__func__);
		return;
	}

	if (of_property_read_string(np, "clock-output-names", &clk_name))
	{
		pr_err("%s +%d %s: No matching name.\n",__FILE__,__LINE__,__func__);
		return;
	}
	if (*clk_name == '\0')
	{
		pr_err("%s +%d %s: Clock name is empty.\n",__FILE__,__LINE__,__func__);
		return;
	}

	parent_name = of_clk_get_parent_name(np,0);

	if(!parent_name)
	{
		pr_err("%s +%d %s: Can't get parent name.\n",__FILE__,__LINE__,__func__);
		return;
	}

	switch(pll_type){
		case CPU_PLL:
			clk = clk_register_fixed_factor(NULL, clk_name,parent_name, CLK_IGNORE_UNUSED, 56, 1);
			break;
		case DDR_PLL:
			clk = clk_register_fixed_factor(NULL, clk_name,parent_name, CLK_IGNORE_UNUSED, 133, 1);
			break;
		case CMN_PLL:
			clk = clk_register_fixed_factor(NULL, clk_name,parent_name, 0, 99, 1);
			break;
		case SPC_PLL:
			clk = clk_register_fixed_factor(NULL, clk_name,parent_name, 0, 125, 1);
			break;
		default:
			pr_err("%s +%d %s: No such pll_type: %d.\n",__FILE__,__LINE__,__func__,pll_type);
			return;
	}

	if (!IS_ERR(clk))
		of_clk_add_provider(np, of_clk_src_simple_get, clk);
	else
		pr_err("%s +%d %s: Clk register failed for %s.\n",__FILE__,__LINE__,__func__,clk_name);

	//	printk("%s +%d %s: SF16A18 %s CLOCK INIT DONE!\n",__FILE__,__LINE__,__func__,clk_name);
	return;
}

static void __init sf16a28_cpu_pll_setup(struct device_node *np)
{
	sf16a28_pll_setup(np,CPU_PLL);
	return;
}

static void __init sf16a28_ddr_pll_setup(struct device_node *np)
{
	sf16a28_pll_setup(np,DDR_PLL);
	return;
}

static void __init sf16a28_cmn_pll_setup(struct device_node *np)
{
	sf16a28_pll_setup(np,CMN_PLL);
	return;
}

static void __init sf16a28_spc_pll_setup(struct device_node *np)
{
	sf16a28_pll_setup(np,SPC_PLL);
	return;
}
CLK_OF_DECLARE(sf16a28_cpu_pll,"siflower,sf16a28-cpupll-clk",sf16a28_cpu_pll_setup);
CLK_OF_DECLARE(sf16a28_ddr_pll,"siflower,sf16a28-ddrpll-clk",sf16a28_ddr_pll_setup);
CLK_OF_DECLARE(sf16a28_cmn_pll,"siflower,sf16a28-cmnpll-clk",sf16a28_cmn_pll_setup);
CLK_OF_DECLARE(sf16a28_spc_pll,"siflower,sf16a28-spcpll-clk",sf16a28_spc_pll_setup);
#endif


/*
 *	CM_CFG_CLK init.
 */
static const char ** __init clk_mux_get_parents(struct device_node *np,
		int *num_parents)
{
	const char **parents;
	int nparents, i;

	nparents = of_count_phandle_with_args(np, "clocks", "#clock-cells");
	if (WARN_ON(nparents <= 0))
		return ERR_PTR(-EINVAL);

	parents = kzalloc(nparents * sizeof(const char *), GFP_KERNEL);
	if (!parents)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < nparents; i++)
		parents[i] = of_clk_get_parent_name(np, i);

	*num_parents = nparents;
	return parents;
}
typedef struct sf_clk_data {
	spinlock_t *lock;
	unsigned long clk_flags;
}sf_clk_data_t;

static sf_clk_data_t sf16a28_clk_data = {
	.lock = &cm_cfg_lock,
	.clk_flags = CLK_SET_PARENT_GATE,
};

static struct of_device_id sfa18_bus1_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-bus1-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_bus2_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-bus2-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_bus3_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-bus3-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_cpu_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-cpu-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_pbus_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-pbus-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_memphy_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-memphy-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_audio_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-audio-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_uart_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-uart-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_spdif_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-spdif-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_sdio_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-sdhc-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_emmc_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-emmc-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_eth_ref_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-eth-ref-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_eth_byp_ref_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-eth-byp-ref-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_eth_tsu_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-eth-tsu-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_wlan24_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-wlan24-mac-wt-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_wlan5_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-wlan5-mac-wt-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_usbphy_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-usbphy-ref-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_tclk_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-tclk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_npupe_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-npupe-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_gdu0_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-gdu0-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_gdu0_eitf_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-gdu0-eitf-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};
static struct of_device_id sfa18_tvif0_cfg_of_match[] = {
	{
		.compatible = "siflower,sf16a28-tvif0-clk",
		.data = &sf16a28_clk_data,
	},
	{}
};

static void sf16a28_cfg_setup(struct device_node *np,int cfg_type)
{
	int num_parents;
	const struct of_device_id *match;
	struct clk *clk;
	const char **parents;
	struct clk_gate *gate;
	struct clk_divider *div;
	struct clk_mux *mux;
	void __iomem *reg = NULL;
	const char *clk_name;
	struct clk_onecell_data *clk_data;
	sf_clk_data_t *data;

	if(!np)
	{
		pr_err("%s +%d %s: Device node not available for pll %d.\n",__FILE__,__LINE__,__func__,cfg_type);
		return;
	}

	switch(cfg_type){
		case BUS1_CFG:
			match = of_match_node(sfa18_bus1_cfg_of_match, np);
			break;
		case BUS2_CFG:
			match = of_match_node(sfa18_bus2_cfg_of_match, np);
			break;
		case BUS3_CFG:
			match = of_match_node(sfa18_bus3_cfg_of_match, np);
			break;
		case CPU_CFG:
			match = of_match_node(sfa18_cpu_cfg_of_match, np);
			break;
		case PBUS_CFG:
			match = of_match_node(sfa18_pbus_cfg_of_match, np);
			break;
		case MEMPHY_CFG:
			match = of_match_node(sfa18_memphy_cfg_of_match, np);
			break;
		case AUDIO_CFG:
			match = of_match_node(sfa18_audio_cfg_of_match, np);
			break;
		case UART_CFG:
			match = of_match_node(sfa18_uart_cfg_of_match, np);
			break;
		case SPDIF_CFG:
			match = of_match_node(sfa18_spdif_cfg_of_match, np);
			break;
		case SDIO_CFG:
			match = of_match_node(sfa18_sdio_cfg_of_match, np);
			break;
		case EMMC_CFG:
			match = of_match_node(sfa18_emmc_cfg_of_match, np);
			break;
		case ETH_REF_CFG:
			match = of_match_node(sfa18_eth_ref_cfg_of_match, np);
			break;
		case ETH_BYP_REF_CFG:
			match = of_match_node(sfa18_eth_byp_ref_cfg_of_match, np);
			break;
		case ETH_TSU_CFG:
			match = of_match_node(sfa18_eth_tsu_cfg_of_match, np);
			break;
		case WLAN24_CFG:
			match = of_match_node(sfa18_wlan24_cfg_of_match, np);
			break;
		case WLAN5_CFG:
			match = of_match_node(sfa18_wlan5_cfg_of_match, np);
			break;
		case USBPHY_CFG:
			match = of_match_node(sfa18_usbphy_cfg_of_match, np);
			break;
		case TCLK_CFG:
			match = of_match_node(sfa18_tclk_cfg_of_match, np);
			break;
		case NPU_PE_CFG:
			match = of_match_node(sfa18_npupe_cfg_of_match, np);
			break;
		case GDU0_CFG:
			match = of_match_node(sfa18_gdu0_cfg_of_match, np);
			break;
		case GDU0_EITF_CFG:
			match = of_match_node(sfa18_gdu0_eitf_cfg_of_match, np);
			break;
		case TVIF0_CFG:
			match = of_match_node(sfa18_tvif0_cfg_of_match, np);
			break;
		default:
			pr_err("%s +%d %s: No such cfg_type: %d.\n",__FILE__,__LINE__,__func__,cfg_type);
			return;
	}

	if (!match) {
		pr_err("%s +%d %s: No matching data.\n",__FILE__,__LINE__,__func__);
		return;
	}

	data = (sf_clk_data_t *)match->data;

	reg = of_iomap(np, 0);
	if (!reg)
	{
		pr_err("%s +%d %s: Can't get clk base address.\n",__FILE__,__LINE__,__func__);
		return;
	}

	if (of_property_read_string(np, "clock-output-names", &clk_name))
	{
		pr_err("%s +%d %s: No matching name.\n",__FILE__,__LINE__,__func__);
		return;
	}
	if (*clk_name == '\0')
	{
		pr_err("%s +%d %s: Clock name is empty.\n",__FILE__,__LINE__,__func__);
		return;
	}

	parents = clk_mux_get_parents(np, &num_parents);

	if(!parents)
	{
		pr_err("%s +%d %s: Can't get parent names.\n",__FILE__,__LINE__,__func__);
		return;
	}

	clk_data = kzalloc(sizeof(struct clk_onecell_data), GFP_KERNEL);
	if (!clk_data)
	{
		pr_err("%s +%d %s: Can not alloc clk data.\n",__FILE__,__LINE__,__func__);
		return;
	}

	clk_data->clk_num = 1;
	clk_data->clks = kzalloc(clk_data->clk_num * sizeof(struct clk *),
			GFP_KERNEL);

	if (!clk_data->clks)
	{
		pr_err("%s +%d %s: Can not alloc clk.\n",__FILE__,__LINE__,__func__);
		return;
	}


	gate = kzalloc(sizeof(struct clk_gate), GFP_KERNEL);
	if (!gate) {
		pr_err("%s +%d %s: Out of memory for gate clk.\n",__FILE__,__LINE__,__func__);
		return;
	}

	div = kzalloc(sizeof(struct clk_divider), GFP_KERNEL);
	if (!div) {
		pr_err("%s +%d %s: Out of memory for div clk.\n",__FILE__,__LINE__,__func__);
		kfree(gate);
		return;
	}

	mux = kzalloc(sizeof(struct clk_mux), GFP_KERNEL);
	if (!mux)
	{
		pr_err("%s +%d %s: Out of memory for mux clk.\n",__FILE__,__LINE__,__func__);
		kfree(gate);
		kfree(div);
		return;
	}

	gate->reg = reg + 0xc;
	gate->bit_idx = 0;
	gate->flags = 0;
	gate->lock = data->lock;

	div->reg = reg + 0x4;
	div->shift = 0;
	div->width = 8;
	div->lock = data->lock;
	div->flags = CLK_DIVIDER_ROUND_CLOSEST | CLK_DIVIDER_ALLOW_ZERO;

	mux->reg = reg;
	mux->shift = 0;
	mux->mask = 0x7;
	mux->lock = data->lock;
	mux->flags = 0;

	switch(cfg_type){
		case BUS1_CFG:
		case BUS2_CFG:
		case BUS3_CFG:
		case CPU_CFG:
		case PBUS_CFG:
		case MEMPHY_CFG:
		case AUDIO_CFG:
		case USBPHY_CFG:
		case WLAN24_CFG:
		case WLAN5_CFG:
			clk = clk_register_composite(NULL,clk_name,parents,num_parents,
					&mux->hw,&clk_mux_ops,
					&div->hw,&clk_divider_ops,
					&gate->hw,&clk_gate_ops,
					data->clk_flags | CLK_IGNORE_UNUSED);
			break;
		case UART_CFG:
		case SPDIF_CFG:
		case SDIO_CFG:
		case EMMC_CFG:
		case ETH_REF_CFG:
		case ETH_BYP_REF_CFG:
		case ETH_TSU_CFG:
			//case WLAN24_CFG:
			//case WLAN5_CFG:
		case TCLK_CFG:
		case NPU_PE_CFG:
		case GDU0_CFG:
		case GDU0_EITF_CFG:
		case TVIF0_CFG:
			clk = clk_register_composite(NULL,clk_name,parents,num_parents,
					&mux->hw,&clk_mux_ops,
					&div->hw,&clk_divider_ops,
					&gate->hw,&clk_gate_ops,
					data->clk_flags);
			break;
		default:
			pr_err("%s +%d %s: No such cfg_type: %d.\n",__FILE__,__LINE__,__func__,cfg_type);
			return;
	}

	if (!IS_ERR(clk))
	{
		clk_data->clks[0] = clk;
		kfree(parents);
		of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
		//		printk("%s +%d %s: SF16A18 %s CLOCK INIT DONE!\n",__FILE__,__LINE__,__func__,clk_name);
	}
	else
	{
		pr_err("%s +%d %s: Clk register failed for %s.\n",__FILE__,__LINE__,__func__,clk_name);
		kfree(gate);
		kfree(div);
		kfree(mux);
		if (clk_data)
			kfree(clk_data->clks);
		kfree(clk_data);
		kfree(parents);
	}

	return;
}

static void __init sf16a28_bus1_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,BUS1_CFG);
	return;
}
static void __init sf16a28_bus2_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,BUS2_CFG);
	return;
}
static void __init sf16a28_bus3_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,BUS3_CFG);
	return;
}
static void __init sf16a28_cpu_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,CPU_CFG);
	return;
}
static void __init sf16a28_pbus_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,PBUS_CFG);
	return;
}
static void __init sf16a28_memphy_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,MEMPHY_CFG);
	return;
}
static void __init sf16a28_audio_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,AUDIO_CFG);
	return;
}
static void __init sf16a28_uart_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,UART_CFG);
	return;
}
static void __init sf16a28_spdif_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,SPDIF_CFG);
	return;
}
static void __init sf16a28_sdio_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,SDIO_CFG);
	return;
}
static void __init sf16a28_emmc_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,EMMC_CFG);
	return;
}
static void __init sf16a28_eth_ref_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,ETH_REF_CFG);
	return;
}
static void __init sf16a28_eth_byp_ref_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,ETH_BYP_REF_CFG);
	return;
}
static void __init sf16a28_eth_tsu_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,ETH_TSU_CFG);
	return;
}
static void __init sf16a28_wlan24_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,WLAN24_CFG);
	return;
}
static void __init sf16a28_wlan5_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,WLAN5_CFG);
	return;
}
static void __init sf16a28_usbphy_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,USBPHY_CFG);
	return;
}
static void __init sf16a28_tclk_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,TCLK_CFG);
	return;
}
static void __init sf16a28_npu_pe_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,NPU_PE_CFG);
	return;
}
static void __init sf16a28_gdu0_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,GDU0_CFG);
	return;
}
static void __init sf16a28_gdu0_eitf_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,GDU0_EITF_CFG);
	return;
}
static void __init sf16a28_tvif0_cfg_setup(struct device_node *np)
{
	sf16a28_cfg_setup(np,TVIF0_CFG);
	return;
}
CLK_OF_DECLARE(sf16a28_bus1_cfg,"siflower,sf16a28-bus1-clk",sf16a28_bus1_cfg_setup);
CLK_OF_DECLARE(sf16a28_bus2_cfg,"siflower,sf16a28-bus2-clk",sf16a28_bus2_cfg_setup);
CLK_OF_DECLARE(sf16a28_bus3_cfg,"siflower,sf16a28-bus3-clk",sf16a28_bus3_cfg_setup);
CLK_OF_DECLARE(sf16a28_cpu_cfg,"siflower,sf16a28-cpu-clk",sf16a28_cpu_cfg_setup);
CLK_OF_DECLARE(sf16a28_pbus_cfg,"siflower,sf16a28-pbus-clk",sf16a28_pbus_cfg_setup);
CLK_OF_DECLARE(sf16a28_memphy_cfg,"siflower,sf16a28-memphy-clk",sf16a28_memphy_cfg_setup);
CLK_OF_DECLARE(sf16a28_audio_cfg,"siflower,sf16a28-audio-clk",sf16a28_audio_cfg_setup);
CLK_OF_DECLARE(sf16a28_uart_cfg,"siflower,sf16a28-uart-clk",sf16a28_uart_cfg_setup);
CLK_OF_DECLARE(sf16a28_spdif_cfg,"siflower,sf16a28-spdif-clk",sf16a28_spdif_cfg_setup);
CLK_OF_DECLARE(sf16a28_sdio_cfg,"siflower,sf16a28-sdhc-clk",sf16a28_sdio_cfg_setup);
CLK_OF_DECLARE(sf16a28_emmc_cfg,"siflower,sf16a8-emmc-clk",sf16a28_emmc_cfg_setup);
CLK_OF_DECLARE(sf16a28_eth_ref_cfg,"siflower,sf16a28-eth-ref-clk",sf16a28_eth_ref_cfg_setup);
CLK_OF_DECLARE(sf16a28_eth_byp_ref_cfg,"siflower,sf16a28-eth-byp-ref-clk",sf16a28_eth_byp_ref_cfg_setup);
CLK_OF_DECLARE(sf16a28_eth_tsu_cfg,"siflower,sf16a28-eth-tsu-clk",sf16a28_eth_tsu_cfg_setup);
CLK_OF_DECLARE(sf16a28_wlan24_cfg,"siflower,sf16a28-wlan24-mac-wt-clk",sf16a28_wlan24_cfg_setup);
CLK_OF_DECLARE(sf16a28_wlan5_cfg,"siflower,sf16a28-wlan5-mac-wt-clk",sf16a28_wlan5_cfg_setup);
CLK_OF_DECLARE(sf16a28_usbphy_cfg,"siflower,sf16a28-usbphy-ref-clk",sf16a28_usbphy_cfg_setup);
CLK_OF_DECLARE(sf16a28_tclk_cfg,"siflower,sf16a28-tclk",sf16a28_tclk_cfg_setup);
CLK_OF_DECLARE(sf16a28_npu_pe_cfg,"siflower,sf16a28-npupe-clk",sf16a28_npu_pe_cfg_setup);
CLK_OF_DECLARE(sf16a28_gdu0_cfg,"siflower,sf16a28-gdu0-clk",sf16a28_gdu0_cfg_setup);
CLK_OF_DECLARE(sf16a28_gdu0_eitf_cfg,"siflower,sf16a28-gdu0-eitf-clk",sf16a28_gdu0_eitf_cfg_setup);
CLK_OF_DECLARE(sf16a28_tvif0_cfg,"siflower,sf16a28-tvif0-clk",sf16a28_tvif0_cfg_setup);

