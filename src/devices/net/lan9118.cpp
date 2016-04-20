#include <devices/net/lan9118.h>

#define SCR_ID_REV		0x50
#define SCR_IRQ_CFG		0x54
	#define IRQ_INT			0x00001000
	#define IRQ_EN			0x00000100
	#define IRQ_POL			0x00000010
	#define IRQ_TYPE		0x00000001
#define SCR_INT_STS		0x58
	#define SW_INT          0x80000000
	#define TXSTOP_INT      0x02000000
	#define RXSTOP_INT      0x01000000
	#define RXDFH_INT       0x00800000
	#define TX_IOC_INT      0x00200000
	#define RXD_INT         0x00100000
	#define GPT_INT         0x00080000
	#define PHY_INT         0x00040000
	#define PME_INT         0x00020000
	#define TXSO_INT        0x00010000
	#define RWT_INT         0x00008000
	#define RXE_INT         0x00004000
	#define TXE_INT         0x00002000
	#define TDFU_INT        0x00000800
	#define TDFO_INT        0x00000400
	#define TDFA_INT        0x00000200
	#define TSFF_INT        0x00000100
	#define TSFL_INT        0x00000080
	#define RXDF_INT        0x00000040
	#define RDFL_INT        0x00000020
	#define RSFF_INT        0x00000010
	#define RSFL_INT        0x00000008
	#define GPIO2_INT       0x00000004
	#define GPIO1_INT       0x00000002
	#define GPIO0_INT       0x00000001
	#define RESERVED_INT    0x7c001000
#define SCR_INT_EN		0x5c
#define SCR_BYTE_TEST	0x64
#define SCR_FIFO_INT	0x68
#define SCR_RX_CFG		0x6c
#define SCR_TX_CFG		0x70
#define SCR_HW_CFG		0x74
#define SCR_RX_DP_CTL	0x78
#define SCR_RX_FIFO_INF	0x7c
#define SCR_TX_FIFO_INF	0x80
#define SCR_PMT_CTRL	0x84
#define SCR_GPIO_CFG	0x88
#define SCR_GPT_CFG		0x8c
	#define GPT_TIMER_EN    0x20000000
#define SCR_GPT_CNT		0x90
#define SCR_WORD_SWAP	0x98
#define SCR_FREE_RUN	0x9c
#define SCR_RX_DROP		0xa0
#define SCR_MAC_CSR_CMD	0xa4
#define SCR_MAC_CSR_DATA	0xa8
#define SCR_AFC_CFG		0xac
#define SCR_E2P_CMD		0xb0
#define SCR_E2P_DATA	0xb4

using namespace captive::devices::net;

LAN9118::LAN9118(irq::IRQLine& irq) 
	: irq(irq),
		pmt_ctrl(1),
		irq_cfg(0), int_sts(0), int_en(0), fifo_int(0x48000000),
		rx_cfg(0), tx_cfg(0), hw_cfg(0x00050004), gpio_cfg(0xffff), gpt_cfg(0), word_swap(0), afc_cfg(0),
		mac_cmd(0), mac_data(0), e2p_cmd(0), e2p_data(0)
{
}

LAN9118::~LAN9118()
{

}

bool LAN9118::read(uint64_t off, uint8_t len, uint64_t& data)
{
	data = 0;
	
	if (len != 4) {
		data = 0;
		fprintf(stderr, "***** net invalid read to %02x of size %d\n", off, len);
		return true;
	}
	
	switch (off) {
	case SCR_ID_REV:	data = 0x92200000; break;
	case SCR_IRQ_CFG:	data = irq_cfg; break;
	case SCR_INT_STS:	data = int_sts; break;
	case SCR_INT_EN:	data = int_en; break;
	case SCR_BYTE_TEST: data = 0x87654321; break;
	case SCR_FIFO_INT:	data = fifo_int; break;
	case SCR_RX_CFG:	data = rx_cfg; break;
	case SCR_TX_CFG:	data = tx_cfg; break;
	case SCR_HW_CFG:	data = hw_cfg; break;
	case SCR_RX_DP_CTL: data = 0; break;
	case SCR_RX_FIFO_INF:	data = 0; break;
	case SCR_TX_FIFO_INF:	data = 0; break;
	case SCR_PMT_CTRL:	data = pmt_ctrl; break;
	case SCR_GPIO_CFG:	data = gpio_cfg; break;
	case SCR_GPT_CFG:	data = gpt_cfg; break;
	case SCR_GPT_CNT:	data = 0; break;
	case SCR_WORD_SWAP:	data = word_swap; break;
	case SCR_FREE_RUN:	data = 0; break;
	case SCR_RX_DROP:	data = 0; break;
	case SCR_MAC_CSR_CMD:	data = mac_cmd; break;
	case SCR_MAC_CSR_DATA:	data = mac_data; break;
	case SCR_AFC_CFG:	data = afc_cfg; break;
	case SCR_E2P_CMD:	data = e2p_cmd; break;
	case SCR_E2P_DATA:	data = e2p_data; break;
	}
	
	fprintf(stderr, "***** net: read: %02x %08x (%d)\n", off, data, len);
	return true;
}

bool LAN9118::write(uint64_t off, uint8_t len, uint64_t data)
{
	if (len != 4) {
		data = 0;
		fprintf(stderr, "***** net invalid write to %02x of size %d\n", off, len);
		return true;
	}
	
	switch (off) {
	case SCR_IRQ_CFG:	irq_cfg = (irq_cfg & IRQ_INT) | (data & (IRQ_EN | IRQ_POL | IRQ_TYPE)); break;
	case SCR_INT_STS:	int_sts &= ~data; break;
	case SCR_INT_EN:	int_en = data & ~RESERVED_INT; int_sts |= data & SW_INT; break;	
	case SCR_FIFO_INT:	fifo_int = data; break;
	case SCR_RX_CFG:	rx_cfg = data & 0xcfff1ff0; break;
	case SCR_TX_CFG:	tx_cfg = data & 6; break;
	case SCR_HW_CFG:	if (data & 1) fprintf(stderr, "**** RESET ****\n"); else hw_cfg = (data & 0x003f300) | (hw_cfg & 0x4); break;
	case SCR_RX_DP_CTL: break;
	case SCR_PMT_CTRL:	if (data & 0x400) fprintf(stderr, "**** PHY RESET ****\n"); pmt_ctrl &= ~0x34e; pmt_ctrl |= (data & 0x34e); break;
	case SCR_GPIO_CFG:	gpio_cfg = data & 0x7777071f; break;
	case SCR_GPT_CFG:	gpt_cfg = data & (GPT_TIMER_EN | 0xffff); break;
	case SCR_WORD_SWAP:	word_swap = data; break;
	case SCR_MAC_CSR_CMD:	mac_cmd = data & 0x4000000f; break;
	case SCR_MAC_CSR_DATA:	mac_data = data; break;
	case SCR_AFC_CFG:	afc_cfg = data & 0x00ffffff; break;
	case SCR_E2P_CMD:	break;
	case SCR_E2P_DATA:	break;
	}
	
	fprintf(stderr, "***** net: write: %02x %08x (%d)\n", off, data, len);
	return true;
}
