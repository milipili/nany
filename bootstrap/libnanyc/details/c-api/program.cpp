#include <yuni/yuni.h>
#include "nany/nany.h"
#include "details/vm/program.h"

using namespace Yuni;


extern "C" void nyprogram_cf_init(nyprogram_cf_t* cf, const nybuild_cf_t* buildcf) {
	assert(cf != NULL);
	memset(cf, 0x0, sizeof(nyprogram_cf_t));
	if (nullptr == buildcf) {
		nany_memalloc_set_default(&(cf->allocator));
		nyconsole_cf_set_stdcout(&cf->console);
	}
	else {
		nany_memalloc_copy(&(cf->allocator), &(buildcf->allocator));
		nyconsole_cf_copy(&(cf->console), &(buildcf->console));
	}
	cf->entrypoint = buildcf->entrypoint;
}


extern "C" nyprogram_t* nyprogram_prepare(nybuild_t* build, const nyprogram_cf_t* cf) {
	if (build) {
		std::unique_ptr<ny::vm::Program> program;
		try {
			if (cf) {
				program = std::make_unique<ny::vm::Program>(*cf, build);
			}
			else {
				nyprogram_cf_t ncf;
				nyprogram_cf_init(&ncf, nullptr);
				program = std::make_unique<ny::vm::Program>(ncf, build);
			}
			program->addRef();
			return program.release()->self();
		}
		catch (...) {}
	}
	return nullptr;
}


extern "C" int nyprogram_main(nyprogram_t* ptr, uint32_t argc, const char** argv) {
	if (ptr and argc != 0 and argv != nullptr) {
		auto& program = ny::ref(ptr);
		return program.execute(argc, argv);
	}
	return EXIT_FAILURE;
}


extern "C" void nyprogram_ref(nyprogram_t* program) {
	if (program)
		ny::ref(program).addRef();
}


extern "C" void nyprogram_unref(nyprogram_t* ptr) {
	if (ptr) {
		auto& program = ny::ref(ptr);
		if (program.release())
			delete &program;
	}
}
