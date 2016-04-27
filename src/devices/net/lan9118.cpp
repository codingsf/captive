#include <devices/net/lan9118.h>
#include <devices/net/network-interface.h>
#include <string.h>

//#define DEBUG_NET

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

#define MAC_CR          1
#define MAC_ADDRH       2
#define MAC_ADDRL       3
#define MAC_HASHH       4
#define MAC_HASHL       5
#define MAC_MII_ACC     6
#define MAC_MII_DATA    7
#define MAC_FLOW        8
#define MAC_VLAN1       9 /* TODO */
#define MAC_VLAN2       10 /* TODO */
#define MAC_WUFF        11 /* TODO */
#define MAC_WUCSR       12 /* TODO */

#define MAC_CR_RXALL    0x80000000
#define MAC_CR_RCVOWN   0x00800000
#define MAC_CR_LOOPBK   0x00200000
#define MAC_CR_FDPX     0x00100000
#define MAC_CR_MCPAS    0x00080000
#define MAC_CR_PRMS     0x00040000
#define MAC_CR_INVFILT  0x00020000
#define MAC_CR_PASSBAD  0x00010000
#define MAC_CR_HO       0x00008000
#define MAC_CR_HPFILT   0x00002000
#define MAC_CR_LCOLL    0x00001000
#define MAC_CR_BCAST    0x00000800
#define MAC_CR_DISRTY   0x00000400
#define MAC_CR_PADSTR   0x00000100
#define MAC_CR_BOLMT    0x000000c0
#define MAC_CR_DFCHK    0x00000020
#define MAC_CR_TXEN     0x00000008
#define MAC_CR_RXEN     0x00000004
#define MAC_CR_RESERVED 0x7f404213

#define PHY_INT_ENERGYON            0x80
#define PHY_INT_AUTONEG_COMPLETE    0x40
#define PHY_INT_FAULT               0x20
#define PHY_INT_DOWN                0x10
#define PHY_INT_AUTONEG_LP          0x08
#define PHY_INT_PARFAULT            0x04
#define PHY_INT_AUTONEG_PAGE        0x02

#define TX_DATA_IDLE				0
#define TX_DATA_B					1
#define TX_DATA_DATA				2

using namespace captive::devices::net;

LAN9118::LAN9118(irq::IRQLine& irq) 
	: irq(irq),
		pmt_ctrl(1),
		irq_cfg(0), irq_status(0), irq_en(0), fifo_int(0),
		rx_cfg(0), tx_cfg(0), hw_cfg(0), gpio_cfg(0), gpt_cfg(0), word_swap(0), afc_cfg(0)
{
	bzero(&phy, sizeof(phy));
	bzero(&mac, sizeof(mac));
	bzero(&txp, sizeof(txp));
	bzero(&eeprom, sizeof(eeprom));
			
	mac.addr[5] = 0x02;
	mac.addr[4] = 0x02;
	mac.addr[3] = 0x02;
	mac.addr[2] = 0x02;
	mac.addr[1] = 0x02;
	mac.addr[0] = 0x02;
	
	reset();
}

LAN9118::~LAN9118()
{
}

bool LAN9118::read(uint64_t off, uint8_t len, uint64_t& data)
{
	data = 0;
	
	if (len != 4) {
		data = 0;
		return true;
	}
	
	if (off < 0x20) {
		data = fifos.rx_data.pop();
		return true;
	}
	
	switch (off) {
	case 0x40: data = fifos.rx_status.pop(); break;
	case 0x44: data = fifos.rx_status.peek(); break;
	case 0x48: data = fifos.tx_status.pop(); break;
	case 0x4c: data = fifos.tx_status.peek(); break;
		
	case SCR_ID_REV:	data = 0x92200000; break;
	case SCR_IRQ_CFG:	data = irq_cfg; break;
	case SCR_INT_STS:	data = irq_status; break;
	case SCR_INT_EN:	data = irq_en; break;
	case SCR_BYTE_TEST: data = 0x87654321; break;
	case SCR_FIFO_INT:	data = fifo_int; break;
	case SCR_RX_CFG:	data = rx_cfg; break;
	case SCR_TX_CFG:	data = tx_cfg; break;
	case SCR_HW_CFG:	data = hw_cfg; break;
	case SCR_RX_DP_CTL: data = 0; break;
	case SCR_RX_FIFO_INF:	data = ((fifos.rx_status.count() & 0xff) << 16) | ((fifos.rx_data.count() << 2) & 0xffff); break; 
	case SCR_TX_FIFO_INF:	data = ((fifos.tx_status.count() & 0xff) << 16) | ((fifos.tx_data.free() << 2) & 0xffff); break;
	case SCR_PMT_CTRL:	data = pmt_ctrl; break;
	case SCR_GPIO_CFG:	data = gpio_cfg; break;
	case SCR_GPT_CFG:	data = gpt_cfg; break;
	case SCR_GPT_CNT:	data = 0; break;
	case SCR_WORD_SWAP:	data = word_swap; break;
	case SCR_FREE_RUN:	data = 0; break;
	case SCR_RX_DROP:	data = 0; break;
	case SCR_MAC_CSR_CMD:	data = mac.cmd; break;
	case SCR_MAC_CSR_DATA:	data = mac.data; break;
	case SCR_AFC_CFG:	data = afc_cfg; break;
	case SCR_E2P_CMD:	data = eeprom.cmd; break;
	case SCR_E2P_DATA:	data = eeprom.data; break;
	}
	
	//fprintf(stderr, "***** net: read: %02x %08x (%d)\n", off, data, len);
	return true;
}

bool LAN9118::write(uint64_t off, uint8_t len, uint64_t data)
{
	if (len != 4) {
		data = 0;
		return true;
	}
	
	off &= 0xff;
	
	if (off >= 0x20 && off < 0x40) {
		fifos.tx_data.push(data);
		handle_tx_data_push(data);
		
		return true;
	}
	
	switch (off) {
	case SCR_IRQ_CFG:
		//fprintf(stderr, "net: lan9118: write: irq-cfg = %08x\n", data);
		irq_cfg = (irq_cfg & IRQ_INT) | (data & (IRQ_EN | IRQ_POL | IRQ_TYPE));
		break;
		
	case SCR_INT_STS:
		//fprintf(stderr, "net: lan9118: write: irq-status = %08x\n", data);
		
		irq_status &= ~data;
		break;
		
	case SCR_INT_EN:
		//fprintf(stderr, "net: lan9118: write: irq-en = %08x\n", data);
		irq_en = data & ~RESERVED_INT;
		irq_status |= data & SW_INT;
		break;	
		
	case SCR_FIFO_INT:
		//fprintf(stderr, "net: lan9118: write: fifo-int = %08x\n", data);
		fifo_int = data;
		break;
		
	case SCR_RX_CFG:
		//fprintf(stderr, "net: lan9118: write: rx-cfg = %08x\n", data);
		if (data & 0x8000) {
			fifos.rx_data.reset();
			fifos.rx_status.reset();
		}
		
		rx_cfg = data & 0xcfff1ff0;
		break;
		
	case SCR_TX_CFG:
		//fprintf(stderr, "net: lan9118: write: tx-cfg = %08x\n", data);
		if (data & 0x8000) {
			fifos.tx_status.reset();
		}
		
		if (data & 0x4000) {
			txp.state = TX_DATA_IDLE;
			txp.cmd_a = 0xffffffff;
			fifos.tx_data.reset();
		}
		
		tx_cfg = data & 6;
		break;
		
	case SCR_HW_CFG:
		//fprintf(stderr, "net: lan9118: write: hw-cfg = %08x\n", data);
		
		if (data & 1) {
			reset();
		} else {
			hw_cfg = (data & 0x301f0000);
		}
		
		reconfigure_fifos();
		break;
		
	case SCR_RX_DP_CTL:
		//fprintf(stderr, "net: lan9118: write: dp-ctl = %08x\n", data);
		break;
	
	case SCR_PMT_CTRL:
		//fprintf(stderr, "net: lan9118: write: pmt-ctl = %08x\n", data);
		
		if (data & 0x400) {
			phy_reset();
		}
		
		pmt_ctrl &= ~0x34e;
		pmt_ctrl |= (data & 0x34e);
		break;
		
	case SCR_GPIO_CFG:
		//fprintf(stderr, "net: lan9118: write: gpio = %08x\n", data);
		gpio_cfg = data & 0x7777071f;
		break;
	
	case SCR_GPT_CFG:		
		//fprintf(stderr, "net: lan9118: write: gpt = %08x\n", data);
		gpt_cfg = data & (GPT_TIMER_EN | 0xffff);
		break;
	
	case SCR_WORD_SWAP:
		//fprintf(stderr, "net: lan9118: write: word-swap = %08x\n", data);
		word_swap = data;
		break;
	
	case SCR_MAC_CSR_CMD:
		mac.cmd = data & 0x4000000f;
		if (data & 0x80000000) {
			if (data & 0x40000000) {
				mac.data = mac_read(data & 0xf);
			} else {
				mac_write(data & 0xf, mac.data);
			}
		}
		break;
	
	case SCR_MAC_CSR_DATA:
		mac.data = data;
		break;
	
	case SCR_AFC_CFG:
		//fprintf(stderr, "net: lan9118: write: afc-cfg = %08x\n", data);
		afc_cfg = data & 0x00ffffff;
		break;
		
	case SCR_E2P_CMD:		fprintf(stderr, "net: lan9118: eemprom cmd\n"); break;
	case SCR_E2P_DATA:		fprintf(stderr, "net: lan9118: eemprom data\n"); break;
	default:
		fprintf(stderr, "net: lan9117: write: UNKNOWN %02x %08x (%d)\n", off, data, len);
		break;
	}
	
	update();	
	return true;
}

void LAN9118::phy_reset()
{
	//fprintf(stderr, "net: lan9118: phy reset!\n");
	
	phy.status = 0x7809;
	phy.control = 0x3000;
	phy.advertise = 0x01e1;
	phy.irq_mask = 0;
	phy.irq = 0;
	phy.mode = 2;
	phy.special = 0x00e1;
	
	phy_update();
}

void LAN9118::phy_update()
{
	// IF LINK DOWN
	//phy.status &= ~0x0024;
	//phy.irq |= PHY_INT_DOWN;
	// ELSE
	phy.status |= 0x0024;
	phy.irq |= PHY_INT_ENERGYON;
	phy.irq |= PHY_INT_AUTONEG_COMPLETE;
	
	phy_update_irq();
}

void LAN9118::phy_update_irq()
{
	if (phy.irq & phy.irq_mask) {
		irq_status |= PHY_INT;
	} else {
		irq_status &= ~PHY_INT;
	}
	
	update();
}

void LAN9118::reset()
{
	//fprintf(stderr, "net: lan9118: reset!\n");
	
	irq_cfg &= (IRQ_TYPE | IRQ_POL);
	irq_status = 0;
	irq_en = 0;
	fifo_int = 0x48000000;
	rx_cfg = 0;
	tx_cfg = 0;
	hw_cfg = 0x00050000;
	pmt_ctrl &= 0x45;
	gpio_cfg = 0;
	
	reconfigure_fifos();
	
	mac.cmd = 0;
	mac.data = 0;
	
	mac.cr = MAC_CR_PRMS;
	mac.hashh = 0;
	mac.hashl = 0;
	mac.mii_acc = 0;
	mac.mii_data = 0;
	mac.flow = 0;
	
	fifos.rx_data.reset();
	fifos.tx_data.reset();
	fifos.rx_status.reset();
	fifos.tx_status.reset();
	
	txp.state = TX_DATA_IDLE;
	txp.buffer_size = 0;
	txp.cmd_a = 0;
	txp.cmd_b = 0;
	bzero(txp.data, sizeof(txp.data));
	txp.len = 0;
	txp.offset = 0;
	txp.pad = 0;
	
	afc_cfg = 0;
	
	eeprom.cmd = 0;
	eeprom.data = 0;
	
	gpt_cfg = 0xffff;
	
	phy_reset();
}

uint32_t LAN9118::mac_read(uint8_t reg)
{
	uint32_t rc;
	
	switch (reg) {
	case MAC_CR:
		rc = mac.cr;
		break;
	case MAC_ADDRH:
		rc = (mac.addr[4] | mac.addr[5] << 8);
		break;
	case MAC_ADDRL:
		rc = (mac.addr[0] | mac.addr[1] << 8 | mac.addr[2] << 16 | mac.addr[3] << 24);
		break;
	case MAC_HASHH:
		rc = mac.hashh;
		break;
	case MAC_HASHL:
		rc = mac.hashl;
		break;
	case MAC_MII_ACC:
		rc = mac.mii_acc;
		break;
	case MAC_MII_DATA:
		rc = mac.mii_data;
		break;
	case MAC_FLOW:
		rc = mac.flow;
		break;
	}
	
	//fprintf(stderr, "net: lan9118: mac-read %d = %08x\n", reg, rc);
	return rc;
}

void LAN9118::mac_write(uint8_t reg, uint32_t data)
{
	//fprintf(stderr, "net: lan9118: mac-write %d = %08x\n", reg, data);
	
	switch (reg) {
	case MAC_CR:
		if ((mac.cr & MAC_CR_RXEN) != 0 && (data & MAC_CR_RXEN) == 0) {
			irq_status |= RXSTOP_INT;
		}
		
		mac.cr = data & ~MAC_CR_RESERVED;
		break;
		
	case MAC_ADDRH:
		mac.addr[4] = data & 0xff;
		mac.addr[5] = (data >> 8) & 0xff;
		break;
		
	case MAC_ADDRL:
		mac.addr[0] = data & 0xff;
		mac.addr[1] = (data >> 8) & 0xff;
		mac.addr[2] = (data >> 16) & 0xff;
		mac.addr[3] = (data >> 24) & 0xff;
		break;
		
	case MAC_HASHH:
		mac.hashh = data;
		break;
		
	case MAC_HASHL:
		mac.hashl = data;
		break;
		
	case MAC_MII_ACC:
		mac.mii_acc = data & 0xffc2;
		if (data & 2) {
			phy_write((data >> 6) & 0x1f, mac.mii_data);
		} else {
			mac.mii_data = phy_read((data >> 6) & 0x1f);
		}
		break;
		
	case MAC_MII_DATA:
		mac.mii_data = data & 0xffff;
		break;
		
	case MAC_FLOW:
		mac.flow = data & 0xffff0000;
		break;
		
	case MAC_VLAN1:
		break;
	}
}

uint32_t LAN9118::phy_read(uint8_t reg)
{
	uint32_t rc;
	switch (reg) {
	case 0:
		rc = phy.control;
		break;
		
	case 1:
		rc = phy.status;
		break;
		
	case 2:
		rc = 0x0007;
		break;
		
	case 3:
		rc = 0xc0c3;
		break;
		
	case 4:
		rc = phy.advertise;
		break;
		
	case 5:
		rc = 0x0f71;
		break;
		
	case 6:
		rc = 1;
		break;
		
	case 17:
		rc = phy.mode;
		break;
		
	case 18:
		rc = phy.special;
		break;
		
	case 29:
		rc = phy.irq;
		phy.irq = 0;
		phy_update_irq();
		break;
		
	case 30:
		rc = phy.irq_mask;
		break;
	}
	
	//fprintf(stderr, "net: lan9118: phy-read %d = %08x\n", reg, rc);
	return rc;
}

void LAN9118::phy_write(uint8_t reg, uint32_t data)
{
	//fprintf(stderr, "net: lan9118: phy-write %d = %08x\n", reg, data);
	
	switch (reg) {
	case 0:
		if (data & 0x8000) {
			phy_reset();
			break;
		}
		
		phy.control = data & 0x7980;
		
		if (data & 0x1000) {
			phy.status |= 0x0020;
		}
		break;
		
	case 4:
		phy.advertise = (data & 0x2d7f) | 0x80;
		break;
		
	case 17:
		phy.mode = data & 0xfffb;
		break;
		
	case 18:
		phy.special = data & 0xffff;
		break;
		
	case 30:
		phy.irq_mask = data & 0xff;
		phy_update_irq();
		break;
	}
}

void LAN9118::update()
{
	uint32_t level = (irq_status & irq_en) != 0;
	if (level) {
		irq_cfg |= IRQ_INT;
	} else {
		irq_cfg &= ~IRQ_INT;
	}
	
	if ((irq_cfg & IRQ_EN) == 0) {
		level = 0;
	}
	
	if ((irq_cfg & (IRQ_TYPE | IRQ_POL)) != (IRQ_TYPE | IRQ_POL)) {
		level = !level;
	}
	
	//fprintf(stderr, "net: lan9118: update level = %d\n", level);
	
	if (level) {
		irq.raise();
	} else {
		irq.rescind();
	}
}

void LAN9118::handle_tx_data_push(uint32_t data)
{
	if (fifos.tx_data.full()) {
		irq_status |= TDFO_INT;
		return;
	}
	
	switch (txp.state)
	{
	case TX_DATA_IDLE:
		txp.cmd_a = data & 0x831f37ff;
		txp.state = TX_DATA_B;
		txp.buffer_size = (txp.cmd_a & 0x7ff);
		txp.offset = ((txp.cmd_a >> 16) & 0x1f);
		
		//fprintf(stderr, "net: lan9118: idle: a=%08x, size=%08x, offset=%08x\n", txp.cmd_a, txp.buffer_size, txp.offset);
		break;
		
	case TX_DATA_B:
		txp.state = TX_DATA_DATA;
		
		if (txp.cmd_a & 0x2000) {
			txp.cmd_b = data;
			
			int32_t n = (txp.buffer_size + txp.offset + 3) >> 2;
			switch ((n >> 24) & 3) {
			case 1:
				n = (-n) & 3;
				break;
			case 2:
				n = (-n) & 7;
				break;
			default:
				n = 0;
				break;
			}
			
			txp.pad = n;
			txp.len = 0;
			
			//fprintf(stderr, "net: lan9118: b: b=%08x, pad=%08x\n", txp.cmd_b, txp.pad);
			break;
		}
		
	case TX_DATA_DATA:
		if (txp.offset >= 4) {
			txp.offset -= 4;
			break;
		}
		
		if (txp.buffer_size <= 0 && txp.pad != 0) {
			txp.pad--;
		} else {
			int32_t n = (4 < (txp.buffer_size + txp.offset)) ? 4 : (txp.buffer_size + txp.offset);
			while (txp.offset) {
				data >>= 8;
				n--;
				txp.offset--;
			}
			
			while (n--) {
				txp.data[txp.len] = data & 0xff;
				txp.len++;
				data >>= 8;
				txp.buffer_size--;
			}
		}
		
		if (txp.buffer_size <= 0 && txp.pad == 0) {
			if (txp.cmd_a & 0x1000) {
				tx_packet(txp.data, txp.len);
				
				if (!fifos.tx_status.full()) {
					fifos.tx_status.push(txp.cmd_b & 0xffff0000);
					if (fifos.tx_status.full()) {
						irq_status |= TSFF_INT;
					}
				}
				
				fifos.tx_data.reset();
			}
			
			if (txp.cmd_a & 0x80000000) {
				irq_status |= TX_IOC_INT;
			}
			
			txp.state = TX_DATA_IDLE;
		}
		
		break;
	}
}

void LAN9118::tx_packet(const uint8_t *buffer, uint32_t length)
{
	//fprintf(stderr, "net: lan9118: transmitting packet\n");

	//fprintf(stderr, "data: ");
	//for (int i = 0; i < length; i++) {
		//fprintf(stderr, "%02x ", buffer[i]);
	//}
	//fprintf(stderr, "\n");
	
	if (phy.control & 0x4000) {
		//fprintf(stderr, "net: lan9118: test packet\n");
		rx_packet(buffer, length);
	} else {
		interface()->transmit_packet(buffer, length);
	}
}

void LAN9118::rx_packet(const uint8_t* buffer, uint32_t length)
{
	uint32_t status = (length + 4) << 16;
	if (buffer[0] == 0xff && buffer[1] == 0xff && buffer[2] == 0xff && buffer[3] == 0xff && buffer[4] == 0xff && buffer[5] == 0xff) {
		status |= 0x2000;
	} else if (buffer[0] & 1) {
		status |= 0x400;
	}
	
	uint32_t v = 0, n = 0;
	for (int i = 0; i < length; i++) {
		v = (buffer[i] << 24) | ((v >> 8) & 0xffffff);
		
		if (++n == 4) {
			fifos.rx_data.push(v);	
			n = 0; v = 0;
		}
	}
	
	fifos.rx_status.push(status);
	
	if (fifos.rx_status.count() > fifo_int & 0xff) {
		irq_status |= RSFL_INT;
	}
	
	update();
}

#define MAX_FIFO_SIZE		16384
#define TX_STATUS_FIFO_SIZE	512
#define RX_STATUS_FIFO_SIZE 512

void LAN9118::reconfigure_fifos()
{
	uint32_t requested_tx_fifo_size = ((hw_cfg >> 16) & 0xf) * 1024;
	
	//fprintf(stderr, "net: lan9118: requested tx fifo size: %08x\n", requested_tx_fifo_size);
	
	fifos.tx_status.configure(TX_STATUS_FIFO_SIZE >> 2);
	fifos.rx_status.configure(RX_STATUS_FIFO_SIZE >> 2);

	fifos.tx_data.configure((requested_tx_fifo_size - TX_STATUS_FIFO_SIZE) >> 2);
	fifos.rx_data.configure((MAX_FIFO_SIZE - RX_STATUS_FIFO_SIZE - requested_tx_fifo_size) >> 2);
	
	/*fprintf(stderr, "net: lan9118: txs=%x, rxs=%x, txd=%x, rxd=%x, total=%x\n", 
			fifos.tx_status.fifo_size(), 
			fifos.rx_status.fifo_size(), 
			fifos.tx_data.fifo_size(), 
			fifos.rx_data.fifo_size(),
			fifos.tx_status.fifo_size()+fifos.rx_status.fifo_size()+fifos.tx_data.fifo_size()+fifos.rx_data.fifo_size());*/
}

bool LAN9118::receive_packet(const uint8_t* buffer, uint32_t length)
{
	return true;
}
