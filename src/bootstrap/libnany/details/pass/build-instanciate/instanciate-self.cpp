#include "instanciate.h"

using namespace Yuni;




namespace Nany
{
namespace Pass
{
namespace Instanciate
{


	void SequenceBuilder::visit(const IR::ISA::Operand<IR::ISA::Op::self>& operands)
	{
		// we can have at least 2 patterns:
		//
		//  * the most frequent, called from a method contained within a class
		//  * from the class itself, most likely a variable

		auto& cdef = cdeftable.substitute(operands.self);
		cdef.qualifiers.ref = true;
		cdef.qualifiers.constant = false;
		cdef.qualifiers.nullable = false;

		auto& atom = frame->atom;

		if (atom.isClassMember())
		{
			assert(atom.parent != nullptr and "invalid parent");
			cdef.mutateToAtom(atom.parent);
		}
		else
		{
			if (atom.isClass())
			{
				cdef.mutateToAtom(&atom);
			}
			else
				ICE() << "invalid 'self' opcode";
		}
	}




} // namespace Instanciate
} // namespace Pass
} // namespace Nany
