/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   LAN9118.h
 * Author: s0457958
 *
 * Created on 19 April 2016, 16:42
 */

#ifndef LAN9118_H
#define LAN9118_H

#include <devices/device.h>
#include <devices/net/network-interface.h>
#include <devices/irq/irq-line.h>

namespace captive
{
	namespace devices
	{
		namespace net
		{
			class LAN9118 : public Device, public NetworkInterface
			{
			public:
				LAN9118(irq::IRQLine& irq);
				virtual ~LAN9118();
				
				uint32_t size() const override { return 0x1000; }

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				std::string name() const { return "lan9118"; }
				
			private:
				irq::IRQLine& irq;
				
				uint32_t pmt_ctrl, irq_cfg, int_sts, int_en, fifo_int;
				uint32_t rx_cfg, tx_cfg, hw_cfg, gpio_cfg, gpt_cfg, word_swap, afc_cfg;
				uint32_t mac_cmd, mac_data;
				uint32_t e2p_cmd, e2p_data;
			};
		}
	}
}

#endif /* SMC91C111_H */

