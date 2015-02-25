#include <devices/io/virtio/virtio.h>
#include <devices/io/virtio/virtqueue.h>
#include <devices/irq/irq-line.h>
#include <hypervisor/guest.h>
#include <captive.h>

#include <string.h>

DECLARE_CONTEXT(VirtIO);

using namespace captive::devices::io::virtio;

#define VIRTIO_CHAR_SHIFT(c, s) (((uint32_t)((uint8_t)c)) << (s))
#define VIRTIO_MAGIC (VIRTIO_CHAR_SHIFT('v', 0) | VIRTIO_CHAR_SHIFT('i', 8) | VIRTIO_CHAR_SHIFT('r', 16) | VIRTIO_CHAR_SHIFT('t', 24))

#define VIRTIO_REG_MAGIC		0x00		// RO
#define VIRTIO_REG_VERSION		0x04		// RO
#define VIRTIO_REG_DEVICEID		0x08		// RO
#define VIRTIO_REG_VENDORID		0x0c		// RO
#define VIRTIO_REG_HOST_FEAT		0x10		// RO
#define VIRTIO_REG_HOST_FEAT_SEL	0x14		// WO
#define VIRTIO_REG_GUEST_FEAT		0x20		// WO
#define VIRTIO_REG_GUEST_FEAT_SEL	0x24		// WO
#define VIRTIO_REG_GUEST_PAGE_SIZE	0x28		// WO
#define VIRTIO_REG_QUEUE_SEL		0x30		// WO
#define VIRTIO_REG_QUEUE_NUM_MAX	0x34		// RO
#define VIRTIO_REG_QUEUE_NUM		0x38		// WO
#define VIRTIO_REG_QUEUE_ALIGN		0x3c		// WO
#define VIRTIO_REG_QUEUE_PFN		0x40		// RW
#define VIRTIO_REG_QUEUE_NOTIFY		0x50		// WO
#define VIRTIO_REG_INTERRUPT_STATUS	0x60		// RO
#define VIRTIO_REG_INTERRUPT_ACK	0x64		// WO
#define VIRTIO_REG_STATUS		0x70		// RW

VirtIO::VirtIO(irq::IRQLine& irq, uint32_t version, uint32_t device_id, uint8_t nr_queues)
	: _irq(irq),
	_version(version),
	_device_id(device_id),
	_vendor_id(0x1af4),
	_host_features(0),
	_guest_page_shift(0),
	_guest_features(0),
	_guest_features_sel(0),
	_queue_sel(0),
	_status(0),
	_isr(0)
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
	if (off >= 0x100 && off < (0x100 + config_area_size())) {
		uint32_t config_area_offset = off - 0x100;
		if (len > (config_area_size() - config_area_offset))
			return false;

		const uint8_t *config_area_data = config_area();
		data = 0;
		memcpy((void *)&data, (const void *)(config_area_data + config_area_offset), len);
		return true;
	} else {
		switch (off) {
		case VIRTIO_REG_MAGIC:
			data = VIRTIO_MAGIC;
			break;

		case VIRTIO_REG_VERSION:
			data = _version;
			break;

		case VIRTIO_REG_DEVICEID:
			data = _device_id;
			break;

		case VIRTIO_REG_VENDORID:
			data = _vendor_id;
			break;

		case VIRTIO_REG_HOST_FEAT:
			data = _host_features;
			break;

		case VIRTIO_REG_QUEUE_NUM_MAX:
			data = (current_queue()->GetPhysAddr() == 0) ? 0x1000 : 0;
			break;

		case VIRTIO_REG_QUEUE_PFN:
			data = current_queue()->GetPhysAddr() >> _guest_page_shift;
			break;

		case VIRTIO_REG_INTERRUPT_STATUS:
			data = _isr;
			break;

		case VIRTIO_REG_STATUS:
			data = _status;
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
	case VIRTIO_REG_HOST_FEAT_SEL:
		break;

	case VIRTIO_REG_GUEST_FEAT: //guest feat
		_guest_features = data;
		break;

	case VIRTIO_REG_GUEST_FEAT_SEL:
		_guest_features_sel = data;
		break;

	case VIRTIO_REG_GUEST_PAGE_SIZE:
		_guest_page_shift = 31 - __builtin_clz(data);
		break;

	case VIRTIO_REG_QUEUE_SEL:
		_queue_sel = data;
		break;

	case VIRTIO_REG_QUEUE_NUM:
		current_queue()->SetSize(data);
		break;

	case VIRTIO_REG_QUEUE_ALIGN:
		current_queue()->SetAlign(data);
		break;

	case VIRTIO_REG_QUEUE_PFN:
		if (data == 0) {
			_guest_features_sel = 0;
			_guest_features = 0;

			reset();
		} else {
			gpa_t queue_phys_addr = data << _guest_page_shift;
			VirtQueue *cq = current_queue();

			void *queue_host_addr;
			if (!guest().resolve_gpa(queue_phys_addr, queue_host_addr)) {
				ERROR << CONTEXT(VirtIO) << "Unable to resolve GPA: " << std::hex << queue_phys_addr;
				assert(false);
			}

			DEBUG << CONTEXT(VirtIO) << "Resolved PFN from " << std::hex << queue_phys_addr << " to " << queue_host_addr;
			cq->SetBaseAddress(queue_phys_addr, queue_host_addr);
		}
		break;

	case VIRTIO_REG_QUEUE_NOTIFY:
		process_queue(queue(data));
		break;

	case VIRTIO_REG_INTERRUPT_ACK: // int ack
		_isr &= ~data;
		break;

	case 0x70: //status
		_status = data;

		if (_status == 0) {
			_guest_features_sel = 0;
			_guest_features = 0;

			reset();
		}

		break;

	default:
		return false;
	}

	update_irq();
	return true;
}

void VirtIO::process_queue(VirtQueue* queue)
{
	assert(_isr == 0);

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

void VirtIO::update_irq()
{
	if (_isr) {
		_irq.raise();
	} else {
		_irq.rescind();
	}
}
