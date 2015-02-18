/*
 * File:   pl190.h
 * Author: spink
 *
 * Created on 18 February 2015, 14:31
 */

#ifndef PL190_H
#define	PL190_H

#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace arm {
			class PL190 : public Primecell
			{
			public:
				PL190();
				virtual ~PL190();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

			private:
				uint32_t irq_status, soft_status, fiq_select;
				uint32_t mask;
				uint32_t default_vector_address;

				uint32_t priority, prev_priority[17], prio_mask[18];
				uint32_t vector_addrs[16];
				uint8_t vector_ctrls[16];

				inline uint32_t get_irq_status() const { return (irq_status | soft_status) & mask & ~fiq_select; }
				inline uint32_t get_fiq_status() const { return (irq_status | soft_status) & mask & fiq_select; }

				uint32_t read_var();
				void write_var();

				void update_vectors();
				void update_lines();
			};
		}
	}
}

#endif	/* PL190_H */

