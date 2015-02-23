#include <devices/io/virtio/virtio.h>
#include <devices/io/virtio/virtqueue.h>
#include <devices/irq/irq-line.h>
#include <hypervisor/guest.h>
#include <captive.h>

#include <string.h>

using namespace captive::devices::io::virtio;

#define VIRTIO_CHAR_SHIFT(c, s) (((uint32_t)((uint8_t)c)) << (s))
#define VIRTIO_MAGIC (VIRTIO_CHAR_SHIFT('v', 0) | VIRTIO_CHAR_SHIFT('i', 8) | VIRTIO_CHAR_SHIFT('r', 16) | VIRTIO_CHAR_SHIFT('t', 24))

VirtIO::VirtIO(irq::IRQLine& irq, uint32_t version, uint32_t device_id, uint8_t nr_queues) : _irq(irq), Version(version), DeviceID(device_id), VendorID(0x1af4)
{
	for (uint8_t i = 0; i < nr_queues; i++) {
		queues.push_back(new VirtQueue());
	}
}

VirtIO::~VirtIO()
{
	for (auto queue : queues) {
		delete queue;
	}

	queues.clear();
}

bool VirtIO::read(uint64_t off, uint8_t len, uint64_t& data)
{
	if (off >= 0x100 && off <= (0x100 + config_area_size())) {
		uint32_t config_area_offset = 0x100 + config_area_size();
		if (len > (config_area_size() - config_area_offset))
			return false;

		uint8_t *config_area_data = config_area();
		data = 0;
		memcpy((void *)&data, config_area_data + config_area_offset, len);
		return true;
	} else {
		switch (off) {
		case 0x00:
			data = VIRTIO_MAGIC;
			break;

		case 0x04:
			data = Version;
			break;

		case 0x08:
			data = DeviceID;
			break;

		case 0x0c:
			data = VendorID;
			break;

		case 0x10:
			data = host_features;
			break;

		case 0x34:
			data = (current_queue()->GetPhysAddr() == 0) ? 0x1000 : 0;
			break;

		case 0x40:
			data = current_queue()->GetPhysAddr() >> guest_page_shift;
			break;

		case 0x60:
			data = _irq.raised() ? 1 : 0;
			break;

		case 0x70:
			data = Status;
			break;

		default:
			return false;
		}
	}

	return true;
}

bool VirtIO::write(uint64_t off, uint8_t len, uint64_t data)
{
	switch (off) {
	case 0x14: //host features sel
		HostFeaturesSel = data;
		break;

	case 0x20: //guest feat
		GuestFeatures = data;
		break;

	case 0x24: //guest feat sel
		GuestFeaturesSel = data;
		break;

	case 0x28: // guest page size
		guest_page_shift = 31 - __builtin_clz(data);
		break;

	case 0x30: //queue sel
		QueueSel = data;
		break;

	case 0x38: //queue num
		current_queue()->SetSize(data);
		break;

	case 0x3c: //queue align
		current_queue()->SetAlign(data);
		break;

	case 0x40: //queue pfn
		if (data == 0) {
			HostFeaturesSel = 0;
			GuestFeaturesSel = 0;

			reset();
		} else {
			gpa_t queue_phys_addr = data << guest_page_shift;
			VirtQueue *cq = current_queue();

			void *queue_host_addr;
			if (!guest().resolve_gpa(queue_phys_addr, queue_host_addr)) {
				ERROR << "Unable to resolve GPA: " << std::hex << queue_phys_addr;
				assert(false);
			}

			cq->SetBaseAddress(queue_phys_addr, queue_host_addr);
		}
		break;

	case 0x50: //queue notify
		process_queue(queue(data));
		break;

	case 0x64: // int ack
		InterruptStatus &= ~data;
		break;

	case 0x70: //status
		Status = data;

		if (data == 0) {
			HostFeaturesSel = 0;
			GuestFeaturesSel = 0;

			reset();
		}

		break;

	default:
		return false;
	}

	if (InterruptStatus == 0) {
		_irq.rescind();
	} else {
		_irq.raise();
	}

	return true;
}

void VirtIO::process_queue(VirtQueue* queue)
{
	assert(InterruptStatus == 0);

	// DEBUG << "[" << GetName() << "] Processing Queue";

	uint16_t head_idx;
	const VirtRing::VirtRingDesc *descr;
	VirtIOQueueEvent evt;
	while ((descr = queue->PopDescriptorChainHead(head_idx)) != NULL) {
		//LC_DEBUG1(LogVirtIO) << "[" << GetName() << "] Popped a descriptor chain head " << std::dec << head_idx;

		bool have_next = false;
		evt.clear();

		do {
			void *descr_host_addr;
			if (!guest().resolve_gpa((gpa_t)descr->paddr, descr_host_addr)) {
				assert(false);
			}

			if (descr->flags & 2) {
				//LC_DEBUG1(LogVirtIO) << "[" << GetName() << "] Adding WRITE buffer @ " << descr_host_addr << ", size = " << descr->len;
				evt.add_write_buffer(descr_host_addr, descr->len);
			} else {
				//LC_DEBUG1(LogVirtIO) << "[" << GetName() << "] Adding READ buffer @ " << descr_host_addr << ", size = " << descr->len;
				evt.add_read_buffer(descr_host_addr, descr->len);
			}

			have_next = (descr->flags & 1) == 1;
			if (have_next) {
				//LC_DEBUG1(LogVirtIO) << "[" << GetName() << "] Following descriptor chain to " << (uint32_t)descr->next;
				descr = queue->GetDescriptor(descr->next);
			}
		} while (have_next);

		//LC_DEBUG1(LogVirtIO) << "[" << GetName() << "] Processing event";
		process_event(evt);

		queue->Push(head_idx, evt.response_size);
		//LC_DEBUG1(LogVirtIO) << "[" << GetName() << "] Pushed a descriptor chain head " << std::dec << head_idx << ", length=" << evt.response_size;
	}
}
