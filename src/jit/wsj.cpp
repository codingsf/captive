#include <jit/wsj.h>
#include <jit/wsj-x86.h>
#include <jit/allocator.h>

using namespace captive::jit;
using namespace captive::jit::x86;

WSJ::WSJ(engine::Engine& engine) : BlockJIT((JIT&)*this), RegionJIT((JIT&)*this), _engine(engine), _allocator(NULL)
{
}

WSJ::~WSJ()
{
}

bool WSJ::init()
{
	return true;
}

void* WSJ::internal_compile_block(const RawBytecodeDescriptor* bcd)
{
	X86Builder builder;

	if (!build(builder, bcd))
		return NULL;

	if (_allocator == NULL) {
		_allocator = new Allocator(_code_arena, _code_arena_size);
	}

	auto region = _allocator->allocate(4096);
	void *buffer = region->base_address();
	uint64_t buffer_size = region->size();

	builder.generate(buffer, buffer_size);
	return buffer;
}

void* WSJ::internal_compile_region(const RawBytecodeDescriptor* bcd)
{
	return NULL;
}

bool WSJ::build(x86::X86Builder& builder, const RawBytecodeDescriptor* bcd)
{
	return false;
}
