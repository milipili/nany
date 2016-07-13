#include "runtime.h"
#include "details/intrinsic/intrinsic-table.h"
#include <yuni/core/string.h>
#include <yuni/core/system/environment.h>

using namespace Yuni;




static void _nanyc_env_set(nyvm_t*, void* varname, void* content)
{
	if (varname)
	{
		AnyString name = *(reinterpret_cast<String*>(varname));
		AnyString cont;
		if (content)
			cont = *(reinterpret_cast<String*>(content));
		System::Environment::Set(name, cont);
	}
}


static void _nanyc_env_unset(nyvm_t*, void* varname)
{
	if (varname)
	{
		AnyString name = *(reinterpret_cast<String*>(varname));
		System::Environment::Unset(name);
	}
}


static void* _nanyc_env_read(nyvm_t* vm, void* varname, void* defvalue)
{
	auto* string = vm_allocate<String>(vm);
	if (varname)
	{
		AnyString name = *(reinterpret_cast<String*>(varname));
		bool read = System::Environment::Read(name, *string);
		if (not read and defvalue)
			*string = *(reinterpret_cast<String*>(defvalue));
	}
	else
	{
		if (defvalue)
			*string = *(reinterpret_cast<String*>(defvalue));
	}
	return string;
}


static bool _nanyc_env_read_as_bool(nyvm_t*, void* varname, bool defvalue)
{
	if (varname)
	{
		AnyString name = *(reinterpret_cast<String*>(varname));
		return System::Environment::ReadAsBool(name, defvalue);
	}
	return defvalue;
}

static int64_t _nanyc_env_read_as_i64(nyvm_t*, void* varname, int64_t defvalue)
{
	if (varname)
	{
		AnyString name = *(reinterpret_cast<String*>(varname));
		return System::Environment::ReadAsInt64(name, defvalue);
	}
	return defvalue;
}

static uint64_t _nanyc_env_read_as_u64(nyvm_t*, void* varname, uint64_t defvalue)
{
	if (varname)
	{
		AnyString name = *(reinterpret_cast<String*>(varname));
		return System::Environment::ReadAsUInt64(name, defvalue);
	}
	return defvalue;
}

static bool _nanyc_env_exists(nyvm_t*, void* varname)
{
	if (varname)
	{
		AnyString name = *(reinterpret_cast<String*>(varname));
		return System::Environment::Exists(name);
	}
	return false;
}




namespace Nany
{

	void importNSLEnv(IntrinsicTable& intrinsics)
	{
		intrinsics.add("nanyc.env.set",    _nanyc_env_set);
		intrinsics.add("nanyc.env.unset",  _nanyc_env_unset);
		intrinsics.add("nanyc.env.read",   _nanyc_env_read);
		intrinsics.add("nanyc.env.asbool", _nanyc_env_read_as_bool);
		intrinsics.add("nanyc.env.asi64",  _nanyc_env_read_as_i64);
		intrinsics.add("nanyc.env.asu64",  _nanyc_env_read_as_u64);
		intrinsics.add("nanyc.env.exists", _nanyc_env_exists);
	}

} // namespace Nany