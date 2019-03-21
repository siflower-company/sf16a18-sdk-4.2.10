#include <common.h>
#include <malloc.h>
#include <net.h>
#include <config.h>

#include <phy.h>
#include <miiphy.h>

#include <asm/io.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <netdev.h>
#include "sfa18_gmac.h"
static int32_t g_phy_addr = -1;
int sgmac_invalidate_cache(struct sgmac_priv *priv, uint is_tx, uint is_tbl, uint buf_index){
#ifndef NO_CACHE
	if(is_tx){
		if(is_tbl)
			invalidate_dcache_range((uint)(priv->dma_tx + buf_index), (uint)((uint)(priv->dma_tx + buf_index) + sizeof(struct sgmac_dma_desc ) ));
		else
			invalidate_dcache_range((uint)priv->tx_buf + priv->dma_buf_sz* buf_index, (uint)(priv->tx_buf + priv->dma_buf_sz* (buf_index + 1) ));
	}
	else{
		if(is_tbl)
			invalidate_dcache_range((uint)(priv->dma_rx+ buf_index), (uint)((uint)(priv->dma_rx+ buf_index) + sizeof(struct sgmac_dma_desc)));
		else
			invalidate_dcache_range((uint)priv->rx_buf+ priv->dma_buf_sz*buf_index, (uint)(priv->rx_buf+ priv->dma_buf_sz* (buf_index + 1) ));
	}
#endif
	return 0;
}

int sgmac_flush_cache(struct sgmac_priv *priv, uint is_tx, uint is_tbl, uint buf_index){
#ifndef NO_CACHE
	if(is_tx){
		if(is_tbl)
			flush_dcache_range((uint)(priv->dma_tx + buf_index), (uint)((uint)(priv->dma_tx+ buf_index) + sizeof(struct sgmac_dma_desc) ));
		else
			flush_dcache_range((uint)priv->tx_buf+ priv->dma_buf_sz* buf_index, (uint)(priv->tx_buf+ priv->dma_buf_sz* (buf_index + 1) ));
	}
	else{
		if(is_tbl)
			flush_dcache_range((uint)(priv->dma_rx+ buf_index), (uint)((uint)(priv->dma_rx+ buf_index) + sizeof(struct sgmac_dma_desc ) ));
		else
			flush_dcache_range((uint)priv->rx_buf+ priv->dma_buf_sz* buf_index, (uint)(priv->rx_buf+ priv->dma_buf_sz* (buf_index + 1) ));
	}
#endif
	return 0;
}
static inline void sgmac_mac_enable(struct sgmac_priv *priv)
{
	u32 value = readl(priv->base + GMAC_CONTROL);
	value |= GMAC_CONTROL_RE | GMAC_CONTROL_TE;
	writel(value, priv->base + GMAC_CONTROL);
	int i = 0;
	for(;i < DMA_TX_RING_SZ; i++){
		sgmac_invalidate_cache(priv, 1, 1, i);
		sgmac_invalidate_cache(priv, 1, 0, i);
	}
	i = 0;
	for(;i < DMA_RX_RING_SZ; i++){
		sgmac_invalidate_cache(priv, 0, 1, i);
		sgmac_invalidate_cache(priv, 0, 0, i);
	}

	value = readl(priv->base + GMAC_DMA_OPERATION);
	value |= DMA_OPERATION_ST | DMA_OPERATION_SR;
	writel(value, priv->base + GMAC_DMA_OPERATION);
}

static inline void sgmac_mac_disable(struct sgmac_priv *priv)
{
	u32 value = readl(priv->base + GMAC_DMA_OPERATION);
	value &= ~(DMA_OPERATION_ST | DMA_OPERATION_SR);
	writel(value, priv->base + GMAC_DMA_OPERATION);

	value = readl(priv->base + GMAC_CONTROL);
	value &= ~(GMAC_CONTROL_TE | GMAC_CONTROL_RE);
	writel(value, priv->base + GMAC_CONTROL);
}

int sf_gmac_write_hwaddr(struct eth_device *dev) {
	u32 data;
	int num = 0;
	unsigned char *addr = dev->enetaddr;
	struct sgmac_priv* priv = (struct sgmac_priv *)dev->priv;
	if (addr) {
		data = (addr[5] << 8) | addr[4] | (num ? GMAC_ADDR_AE : 0);
		writel(data, priv->base + GMAC_ADDR_HIGH(num));
		data = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) |
		       addr[0];
		writel(data, priv->base + GMAC_ADDR_LOW(num));
	} else {
		writel(0, priv->base + GMAC_ADDR_HIGH(num));
		writel(0, priv->base + GMAC_ADDR_LOW(num));
	}
	return 0;
}

static void sgmac_set_mdc_clk_div(struct sgmac_priv *priv)
{
	int value = readl(priv->base + GMAC_GMII_ADDR);
	value &= ~GMAC_GMII_ADDR_CR_MASK;
	value |= GMAC_GMII_ADDR_CR;
	writel(value, priv->base + GMAC_GMII_ADDR);
}

static int sgmac_hw_init(struct sgmac_priv *priv)
{
	u32 value, ctrl;
	/* Save the ctrl register value */
	ctrl = readl(priv->base + GMAC_CONTROL) & GMAC_CONTROL_SPD_MASK;
	/* SW reset */

	writel(DMA_BUS_MODE_SFT_RESET, priv->base + GMAC_DMA_BUS_MODE);
	udelay(10000);

	value = (0x10 << DMA_BUS_MODE_PBL_SHIFT) |
		(0x10 << DMA_BUS_MODE_RPBL_SHIFT) | DMA_BUS_MODE_FB |
		DMA_BUS_MODE_ATDS | DMA_BUS_MODE_AAL;
	writel(value, priv->base + GMAC_DMA_BUS_MODE);

	writel(0, priv->base + GMAC_DMA_INTR_ENA);

	/* Mask power mgt interrupt */
	writel(GMAC_INT_MASK_PMTIM, priv->base + GMAC_INT_STAT);

	/* GMAC requires AXI bus init. This is a 'magic number' for now */
	writel(0x0077000E, priv->base + GMAC_DMA_AXI_BUS);
	// austin:set link, speed and duplex here
	// no link
	priv->link = 0;
	priv->duplex = DUPLEX_FULL;
#ifdef CONFIG_SFAX8_RGMII_GMAC
	priv->speed = SPEED_1000;
	ctrl = GMAC_CONTROL_CST | GMAC_CONTROL_JE | GMAC_CONTROL_ACS |
	       GMAC_SPEED_1000M | GMAC_CONTROL_DM;
#else
	priv->speed = SPEED_100;
	ctrl = GMAC_CONTROL_CST | GMAC_CONTROL_JE | GMAC_CONTROL_ACS |
	       GMAC_SPEED_100M | GMAC_CONTROL_DM;
#endif

	ctrl |= GMAC_CONTROL_IPC;
	writel(ctrl, priv->base + GMAC_CONTROL);
	/* Set the HW DMA mode and the COE */
	writel(DMA_OPERATION_TSF | DMA_OPERATION_RFD | DMA_OPERATION_RFA |
					DMA_OPERATION_RTC_256 |
					DMA_OPERATION_OSF,
			priv->base + GMAC_DMA_OPERATION);

	return 0;
}
static inline void
desc_init_rx_desc(struct sgmac_dma_desc *p, int ring_size, int buf_sz)
{
	struct sgmac_dma_desc *end = p + ring_size - 1;

	memset(p, 0, sizeof(*p) * ring_size);

	for (; p <= end; p++){
		desc_set_buf_len(p, buf_sz);
	}

	end->buf_size |= cpu_to_le32(RXDESC1_END_RING);
}

static inline void desc_init_tx_desc(struct sgmac_dma_desc *p, u32 ring_size)
{
	memset(p, 0, sizeof(*p) * ring_size);
	p[ring_size - 1].flags = cpu_to_le32(TXDESC_END_RING);
}
/**
 * init_sgmac_dma_desc_rings - init the RX/TX descriptor rings
 * @dev: net device structure
 * Description:  this function initializes the DMA RX/TX descriptors
 * and allocates the socket buffers.
 */
static inline void
desc_set_buf_addr(struct sgmac_dma_desc *p, u32 paddr, int len)
{
	p->buf1_addr = cpu_to_le32(paddr);
	if (len > MAX_DESC_BUF_SZ)
		p->buf2_addr = cpu_to_le32(paddr + MAX_DESC_BUF_SZ);
}

static inline void desc_set_rx_owner(struct sgmac_dma_desc *p)
{
	/* Clear all fields and set the owner */
	p->flags = cpu_to_le32(DESC_OWN);
}

static void sgmac_rx_fill(struct sgmac_priv *priv)
{
	struct sgmac_dma_desc *p;
	int bufsz = priv->dma_buf_sz;
	unsigned char i = 0;
	char * paddr_start = (char * )virt_to_phys(priv->rx_buf);
	char * paddr = NULL;
	for(; i < DMA_RX_RING_SZ; i++){
		paddr = paddr_start  + bufsz * i;
		p = priv->dma_rx + i;
		desc_set_buf_addr(p, (u32) paddr, priv->dma_buf_sz);
		desc_set_rx_owner(p);
	}
}

static inline void desc_set_tx_owner(struct sgmac_dma_desc *p)
{
	u32 tmpflags = le32_to_cpu(p->flags);
	tmpflags &= TXDESC_END_RING;
	tmpflags |=  TXDESC_FIRST_SEG| TXDESC_LAST_SEG |DESC_OWN;
	p->flags = cpu_to_le32(tmpflags);
}

static void sgmac_tx_fill(struct sgmac_priv *priv) {
	struct sgmac_dma_desc *p;
	char *paddr = (char *)virt_to_phys(priv->tx_buf);
	unsigned char i = 0;
	for(;i < DMA_TX_RING_SZ; i++){
		p = priv->dma_tx + i;
		desc_set_buf_addr(p, (u32)paddr, priv->dma_buf_sz);
		paddr += priv->dma_buf_sz;
	}
}

static int sgmac_dma_desc_rings_init(struct sgmac_priv *priv) {
	unsigned int bfsize;

	/* Set the Buffer size according to the MTU;
	 * The total buffer size including any IP offset must be a multiple
	 * of 8 bytes.
	 */
	bfsize = ALIGN(MAX_FRAME_SIZE + ETH_HLEN + ETH_FCS_LEN + 2, BUF_ALIGN);

	priv->dma_rx_alloc = malloc( DMA_RX_RING_SZ * sizeof(struct sgmac_dma_desc) + BUF_ALIGN);
	priv->dma_rx = (struct sgmac_dma_desc *)(priv->dma_rx_alloc+ BUF_ALIGN - ((uint)priv->dma_rx_alloc % BUF_ALIGN));
	priv->dma_rx_phy = (u32 )virt_to_phys(priv->dma_rx);

	priv->dma_tx_alloc = malloc( DMA_TX_RING_SZ * sizeof(struct sgmac_dma_desc) + BUF_ALIGN);
	priv->dma_tx = (struct sgmac_dma_desc *)(priv->dma_tx_alloc+ BUF_ALIGN - ((uint)priv->dma_tx_alloc % BUF_ALIGN));
	priv->dma_tx_phy = (u32 )virt_to_phys(priv->dma_tx);

	priv->rx_index= 0;
	priv->dma_buf_sz = bfsize;
	desc_init_rx_desc(priv->dma_rx, DMA_RX_RING_SZ, priv->dma_buf_sz);
	priv->rx_buf_alloc = malloc(bfsize* DMA_RX_RING_SZ + BUF_ALIGN);
	priv->rx_buf= (priv->rx_buf_alloc + BUF_ALIGN - ((uint)priv->rx_buf_alloc % BUF_ALIGN));

	sgmac_rx_fill(priv);

	desc_init_tx_desc(priv->dma_tx, DMA_TX_RING_SZ);
	priv->tx_buf_alloc = malloc(bfsize* DMA_TX_RING_SZ + BUF_ALIGN);
	priv->tx_buf= (priv->tx_buf_alloc + BUF_ALIGN - ((uint)priv->tx_buf_alloc % BUF_ALIGN));
	sgmac_tx_fill(priv);

	writel(priv->dma_tx_phy, priv->base + GMAC_DMA_TX_BASE_ADDR);
	writel(priv->dma_rx_phy, priv->base + GMAC_DMA_RX_BASE_ADDR);
	return 0;

}

int sf_gmac_init(struct eth_device *dev, bd_t *bt) {
	/* Enable Emac Registers */
	if(sgmac_adjust_link((struct sgmac_priv *)dev->priv) < 0)
				return -1;
	sgmac_mac_enable((struct sgmac_priv *)dev->priv);
	return 0;
}

void sf_gmac_halt(struct eth_device *dev) {
	sgmac_mac_disable((struct sgmac_priv *)dev->priv);
	return ;
}

/* GMAC Descriptor Access Helpers */
inline void desc_set_buf_len(struct sgmac_dma_desc *p, u32 buf_sz)
{
	if (buf_sz > MAX_DESC_BUF_SZ)
		p->buf_size = cpu_to_le32(
				MAX_DESC_BUF_SZ |
				(buf_sz - MAX_DESC_BUF_SZ)
						<< DESC_BUFFER2_SZ_OFFSET);
	else
		p->buf_size = cpu_to_le32(buf_sz);
}
static inline void
desc_set_buf_addr_and_size(struct sgmac_dma_desc *p, u32 paddr, int len)
{
	desc_set_buf_len(p, len);
	desc_set_buf_addr(p, (u32)paddr, len);
}
/**
 *  sgmac_xmit:
 *  @skb : the socket buffer
 *  @ndev : device pointer
 *  Description : Tx entry point of the driver.
 */

static inline int desc_get_owner(struct sgmac_dma_desc *p) {
	return le32_to_cpu(p->flags) & DESC_OWN;
}

int sf_gmac_send(struct eth_device *dev, void *packet, int length) {
	struct sgmac_priv *priv = dev->priv;
	struct sgmac_dma_desc *desc;
	// austin:why 32 frames will set TXDESC_INTERRUPT?
	// FIXME: we do a little when we receive tx irq, so maybe we can tx 32

	desc = priv->dma_tx + priv->tx_index;
	sgmac_invalidate_cache(priv, 1, 1, priv->tx_index);
	if (desc_get_owner(desc)){
		error(" desc owned by dma");
		return -1;
	}

	// printf(" tx idx %d\n", priv->tx_index);
	memcpy(priv->tx_buf + priv->dma_buf_sz * priv->tx_index, packet, length);
	sgmac_flush_cache(priv,1,0,priv->tx_index);

	desc_set_buf_addr_and_size(desc, virt_to_phys(priv->tx_buf + priv->dma_buf_sz * priv->tx_index), length);

	desc_set_tx_owner(desc);

	sgmac_flush_cache(priv,1,1,priv->tx_index);
	writel(1, (void *)priv->base + GMAC_DMA_TX_POLL);

	priv->tx_index= dma_ring_incr(priv->tx_index, DMA_TX_RING_SZ);

	return 0;

}

static inline int desc_get_rx_frame_len(struct sgmac_dma_desc *p)
{
	u32 data, len;
	data = le32_to_cpu(p->flags);
	len = (data & RXDESC_FRAME_LEN_MASK) >> RXDESC_FRAME_LEN_OFFSET;
	//	if (data & RXDESC_FRAME_TYPE)
	//		len -= ETH_FCS_LEN;

	return len;
}


int sf_gmac_recv(struct eth_device *dev) {
	struct sgmac_dma_desc *p;
	int frame_len;
	unsigned char * buf;
	struct sgmac_priv *priv = dev->priv;


	do{
		sgmac_invalidate_cache(priv,0,1,priv->rx_index);
		p = priv->dma_rx +  priv->rx_index;
		if (desc_get_owner(p)){
		  return -1;
		}

		frame_len = desc_get_rx_frame_len(p);


		buf = net_rx_packets[priv->rx_index % PKTBUFSRX];

		char * rx_pkt = (char *)phys_to_virt(p->buf1_addr);
		sgmac_invalidate_cache(priv,0,0,priv->rx_index);
		memcpy(buf, rx_pkt, frame_len);
		// printf("recv pkt to buf len %d \n",frame_len);
		net_process_received_packet(buf , frame_len );
		desc_set_rx_owner(p);
		sgmac_flush_cache(priv,0,1,priv->rx_index);
		priv->rx_index = dma_ring_incr(priv->rx_index, DMA_RX_RING_SZ);
	}while(1);

	return 0;
}

#define REG_GMAC_GmiiAddr         GMAC_ADDR_BASE + 0x0010
#define REG_GMAC_GmiiData         GMAC_ADDR_BASE + 0x0014

static  int32_t wait_phy_rw_not_busy(void) {
	   int32_t reg = readl((void*)REG_GMAC_GmiiAddr);
	   // wait for GMII Register GB is not busy
		   while (reg & GMII_ADDR_MASK_GB) {
			      reg = readl((void*)REG_GMAC_GmiiAddr);
			   }
	   return reg;
}

// set phy reg with mac register
static void write_phy_reg(int phy_reg, int value) {
   int addr_value = wait_phy_rw_not_busy();
   // prepare data for register 5
   writel(value & GMII_DATA_MASK, (void*)REG_GMAC_GmiiData);
   // set address flag
   addr_value = (addr_value & (~GMII_ADDR_MASK_GR))|((phy_reg << 6) & GMII_ADDR_MASK_GR);
   // set phy addr
   addr_value = (addr_value & (~GMII_ADDR_MASK_PA))|((g_phy_addr << 11) & GMII_ADDR_MASK_PA);
   // set write flag
   addr_value = addr_value  | GMII_ADDR_VALUE_GW_WRITE;
   // set GB flag to indica is busy now
   addr_value = addr_value | GMII_ADDR_MASK_GB;
   writel(addr_value, (void*)REG_GMAC_GmiiAddr);
   // wait for finish
   wait_phy_rw_not_busy();
}


static int read_phy_reg_direct(int phy_addr, int phy_reg)
{
	   int value = wait_phy_rw_not_busy();
	   // clear the data first
	      writel(0xffffffff, (void*)REG_GMAC_GmiiData);
	   // set address flag
	     value = (value & (~GMII_ADDR_MASK_GR)) |
	         ((phy_reg << 6) & GMII_ADDR_MASK_GR);
	   // set phy addr
	      value = (value & (~GMII_ADDR_MASK_PA)) |
	         ((phy_addr << 11) & GMII_ADDR_MASK_PA);
	   // set read flag
	      value = value & (~GMII_ADDR_VALUE_GW_WRITE);
	   // set GB flag to indica is busy now
	      value = value | GMII_ADDR_MASK_GB;
	   writel(value, (void*)REG_GMAC_GmiiAddr);
	   // wait for complete
	      wait_phy_rw_not_busy();
	   return readl((void*)REG_GMAC_GmiiData) & GMII_DATA_MASK;
}

static int read_phy_reg(int phy_reg) {
	   return read_phy_reg_direct(g_phy_addr, phy_reg);
}

static void mdiobus_scan(void)
{
	int i, phy_id1, phy_id2, phy_id;
	// g_phy_addr = 31, so we can save about 2ms in simultion.
	for (i = 31; i >= 0; i--) {
		phy_id1 = read_phy_reg_direct(i, MII_PHYSID1) & 0xffff;
		phy_id2 = read_phy_reg_direct(i, MII_PHYSID2) & 0xffff;
		phy_id = (phy_id2 | (phy_id1 << 16));

		// If the phy_id is mostly Fs, there is no device there(accord
		// with linux standard phy driver detect code)
		if ((phy_id & 0x1fffffff) != 0x1fffffff) {
			g_phy_addr = i;
			printf("address phy is %d\n", g_phy_addr);
			break;
		}
	}
	return;
}
static int32_t gmac_init_phy(void)
{
	int phy_ctrl = 0;
	mdiobus_scan();
	if(g_phy_addr == -1){
	  printf("phy not find \n");
	  return -1;
	}
	phy_ctrl |= BMCR_ANENABLE;
	// phy_ctrl |= BMCR_FULLDPLX;

	write_phy_reg(MII_BMCR, phy_ctrl);
	// int phy_spec_ctrl = read_phy_reg(IP10XX_SPEC_CTRL_STATUS);
	// phy_spec_ctrl |= IP1001_RXPHASE_SEL;
	// write_phy_reg(IP10XX_SPEC_CTRL_STATUS, phy_spec_ctrl);
	// wait for 4 seconds
	udelay(400000);
	return 0;
}

#define MIIM_RTL8211F_PHY_STATUS       0x1a
#define MIIM_RTL8211F_PAGE_SELECT      0x1f
#define MIIM_RTL8211F_PHYSTAT_DUPLEX   0x0008
#define MIIM_RTL8211F_PHYSTAT_SPEED    0x0030
#define MIIM_RTL8211F_PHYSTAT_GBIT     0x0020
#define MIIM_RTL8211F_PHYSTAT_100      0x0010

int sgmac_adjust_link(struct sgmac_priv *priv)
{
	int speed = SPEED_100;
	int duplex = DUPLEX_FULL;
	int link = read_phy_reg(MII_BMSR);
	int i = 0;
	unsigned int mii_reg = 0;
	while (!(link & BMSR_LSTATUS)) {
		if(i == 3){
			printf("phy link fail %d\n",i);
			return -1;
		}
		printf("cannot find phy link retry %d\n",i);
		udelay(1000000);
		link = read_phy_reg(MII_BMSR);
		i++;
	}

	write_phy_reg(MIIM_RTL8211F_PAGE_SELECT, 0xa43);
	mii_reg = read_phy_reg(MIIM_RTL8211F_PHY_STATUS);

	if (mii_reg & MIIM_RTL8211F_PHYSTAT_DUPLEX)
		duplex = DUPLEX_FULL;
	else
		duplex = DUPLEX_HALF;

	speed = (mii_reg & MIIM_RTL8211F_PHYSTAT_SPEED);

	switch (speed) {
	case MIIM_RTL8211F_PHYSTAT_GBIT:
		printf("gmac phy is 1000M\n");
		speed = SPEED_1000;
		break;
	case MIIM_RTL8211F_PHYSTAT_100:
		printf("gmac phy is 100M\n");
		speed = SPEED_100;
		break;
	default:
		printf("gmac phy is 10M\n");
		speed = SPEED_10;
	}

	   int reg = readl(priv->base + GMAC_CONTROL);
	   reg &= ~GMAC_CONTROL_SPD_MASK;
	   if (speed == SPEED_10)
		 reg |= GMAC_SPEED_10M;
	   else if (speed == SPEED_100)
		 reg |= GMAC_SPEED_100M;
	   else if (speed == SPEED_1000)
		 reg |= GMAC_SPEED_1000M;


	   if (DUPLEX_FULL == duplex)
		 reg |= GMAC_CONTROL_DM;
	   else
		 reg &= ~GMAC_CONTROL_DM;

	   writel(reg, priv->base + GMAC_CONTROL);
	   return 0;
}

int sf_gmac_register(void){
	struct eth_device *dev;
	struct sgmac_priv    *priv;
	dev = (struct eth_device *)malloc(sizeof(struct eth_device));
	if (dev == NULL) {
		error("%s: Not enough memory!\n", __func__);
		return -1;
	}
	priv = (struct sgmac_priv *)malloc(sizeof(struct sgmac_priv));
	if (priv == NULL) {
		error("%s: Not enough memory!\n", __func__);
		return -1;
	}
	memset(dev, 0, sizeof(struct eth_device));
	memset(priv, 0, sizeof(struct sgmac_priv));

	sprintf(dev->name, "sf_eth1");
	dev->priv = (void *)priv;
	dev->iobase = GMAC_ADDR_BASE;
	dev->init = sf_gmac_init;
	dev->halt = sf_gmac_halt;
	dev->send = sf_gmac_send;
	dev->recv = sf_gmac_recv;
	dev->write_hwaddr = sf_gmac_write_hwaddr;
	priv->base = (void *)GMAC_ADDR_BASE;
	eth_register(dev);
	// TODO: need phy init here
	//
	printf("read version 0x%x\n", readl(priv->base + GMAC_VERSION));
	sgmac_set_mdc_clk_div(priv);
	if(gmac_init_phy() < 0)
	  return -1;
	udelay(10000);
	sgmac_hw_init(priv);
	sgmac_dma_desc_rings_init(priv);
	return 0;
}
