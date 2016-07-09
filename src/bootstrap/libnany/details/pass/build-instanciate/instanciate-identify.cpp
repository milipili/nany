#include "instanciate.h"
#include "instanciate-atom.h"
#include "libnany-traces.h"

using namespace Yuni;




namespace Nany
{
namespace Pass
{
namespace Instanciate
{

	namespace // anonymous
	{

		static bool tryFindProperties(std::vector<std::reference_wrapper<Atom>>& multipleResults,
			Atom& atom,
			const Classdef& cdef, const AnyString& name)
		{
			if (not cdef.qualifiers.propset)
			{
				return atom.propertyLookupOnChildren(multipleResults, "^propget^", name);
			}
			else
			{
				bool success = true;
				success &= atom.propertyLookupOnChildren(multipleResults, "^propset^", name);
				//success &= atom.propertyLookupOnChildren(multipleResults, "^prop^+=^", name);
				return success;
			}
		}

	} // anonymous namespace




	Atom& SequenceBuilder::resolveTypeAlias(Atom& original, const Classdef*& resultcdef)
	{
		assert(original.isTypeAlias());

		// trying a direct resolution
		auto cdef = std::cref(cdeftable.classdef(original.returnType.clid));

		std::unordered_set<uint32_t> encountered; // to avoid circular references
		Atom* alias = nullptr;
		do
		{
			if (cdef.get().isBuiltin()) // gotcha !
			{
				resultcdef = &(cdef.get());
				return original;
			}

			// current atom to check
			alias = cdeftable.findClassdefAtom(cdef.get());
			if (unlikely(!alias))
				break;

			if ((alias->parent == original.parent) and alias->atomid > original.atomid)
			{
				// same parent but declared after (the atomid is likely to be greater
				// than the first one since registered after)
				complainTypedefDeclaredAfter(original, *alias);
				break;
			}

			if (not alias->isTypeAlias()) // gotcha !
			{
				resultcdef = &(cdef.get());
				return *alias;
			}

			// checking for circular aliases
			if (not encountered.insert(alias->atomid).second)
			{
				// circular reference
				complainTypealiasCircularRef(original, *alias);
				break;
			}

			cdef = std::cref(cdeftable.classdef(alias->returnType.clid));
		}
		while (alias != nullptr);

		complainTypedefUnresolved(original);
		resultcdef = nullptr;
		return original;
	}


	inline bool SequenceBuilder::emitIdentifyForSingleResult(bool isLocalVar, const Classdef& cdef,
		const IR::ISA::Operand<IR::ISA::Op::identify>& operands, const AnyString& name)
	{
		auto& resultAtom = multipleResults[0].get();
		const Classdef* cdefTypedef = nullptr;
		auto& atom = (not resultAtom.isTypeAlias())
			? resultAtom
			: resolveTypeAlias(resultAtom, cdefTypedef);


		if (unlikely((!cdefTypedef and resultAtom.isTypeAlias()) or atom.flags(Atom::Flags::error)))
			return false;

		if (unlikely(cdefTypedef and cdefTypedef->isBuiltin()))
		{
			auto& spare = cdeftable.substitute(cdef.clid.lvid());
			spare.import(*cdefTypedef);

			if (isLocalVar)
			{
				// disable optimisation to avoid unwanted behavior
				auto& lvidinfo = frame->lvids[operands.lvid];
				lvidinfo.synthetic = false;
				lvidinfo.origin.memalloc = false;
				lvidinfo.origin.returnedValue = false;
			}
			return true;
		}

		// if the resolution is simple (aka only one solution), it is possible that the
		// solution is a member variable (`self.myvar`). In this case, the atom will be the member itself
		// and not its real type
		if (atom.isMemberVariable())
		{
			assert(not isLocalVar and "a member variable cannot be a local variable");
			assert(not atom.returnType.clid.isVoid());

			// member variable - the real type is held by 'returnType'
			auto& cdefvar = cdeftable.classdef(atom.returnType.clid);
			auto* atomvar = (not cdefvar.isBuiltin()) ? cdeftable.findClassdefAtom(cdefvar) : nullptr;
			if (unlikely(!atomvar and not cdefvar.isBuiltin()))
				return (ice() << "invalid variable member type for " << atom.fullname());

			auto& spare = cdeftable.substitute(operands.lvid);
			spare.import(cdefvar);
			if (atomvar)
				spare.mutateToAtom(atomvar);

			uint32_t self = operands.self;
			if (self == 0) // implicit 'self' ?
			{
				if (frame->atom.isClassMember())
				{
					// 'self' is given by the first parameter
					self = 2; // 1: return type, 2: first parameter
				}
				else
				{
					// no 'self' available since it just does not exist, which can be expected
					// for type resolution (the type resolution is done directly from the atom class,
					// where the initialization is done via a proxy function)
					// It's ok for type resolution since we already know we're dealing with a variable member)
					if (frame->atom.isClass() and (not canGenerateCode()))
					{
						// 'self' can stay null
					}
					else
					{
						ice() << "identify: invalid 'self' object for '" << name << "' from '"
							<< frame->atom.caption() << '\'';
						return false;
					}
				}
			}

			auto& lvidinfo = frame->lvids[operands.lvid];
			lvidinfo.synthetic = false;

			auto& origin  = lvidinfo.origin.varMember;
			assert(atom.atomid != 0);
			origin.self   = self;
			origin.atomid = atom.atomid;
			origin.field  = atom.varinfo.effectiveFieldIndex;

			if (canGenerateCode())
			{
				// read the address
				assert(self != 0 and "'self can be null only for type resolution'");
				out.emitFieldget(operands.lvid, self, atom.varinfo.effectiveFieldIndex);
				tryToAcquireObject(operands.lvid, cdefvar);
			}
		}
		else
		{
			// override the typeinfo
			auto& spare = cdeftable.substitute(cdef.clid.lvid());
			spare.import(cdef);
			spare.mutateToAtom(&atom);

			if (isLocalVar)
			{
				// disable optimisation to avoid unwanted behavior
				auto& lvidinfo = frame->lvids[operands.lvid];
				lvidinfo.synthetic = false;
				lvidinfo.origin.memalloc = false;
				lvidinfo.origin.returnedValue = false;

				if (canGenerateCode())
					acquireObject(operands.lvid);
			}
		}
		return true;
	}


	bool SequenceBuilder::emitIdentifyForProperty(const IR::ISA::Operand<IR::ISA::Op::identify>& operands,
		const Classdef& cdef, Atom& propatom)
	{
		// report for instanciation
		Logs::Message::Ptr subreport;
		// all pushed parameters
		decltype(FuncOverloadMatch::result.params) params;
		// all pushed template parameters
		decltype(FuncOverloadMatch::result.params) tmplparams;
		// return value
		uint32_t lvid = operands.lvid;
		// current atom
		uint32_t atomid = frame->atomid;

		// preparing the overload matcher
		overloadMatch.clear();
		overloadMatch.input.rettype.push_back(CLID{atomid, lvid});
		if (operands.self)
			overloadMatch.input.params.indexed.emplace_back(CLID{atomid, operands.self});

		// try to validate the func call
		// (no error reporting, since no overload is present)
		if (unlikely(TypeCheck::Match::none == overloadMatch.validate(propatom)))
			return complainCannotCall(propatom, overloadMatch);

		// get new parameters
		params.swap(overloadMatch.result.params);
		tmplparams.swap(overloadMatch.result.tmplparams);


		InstanciateData info{subreport, propatom, cdeftable, build, params, tmplparams};
		if (not doInstanciateAtomFunc(subreport, info, lvid))
			return false;

		if (canGenerateCode())
		{
			for (auto& param: params)
				out.emitPush(param.clid.lvid());

			out.emitCall(lvid, propatom.atomid, info.instanceid);
		}
		return true;
	}


	bool SequenceBuilder::identify(const IR::ISA::Operand<IR::ISA::Op::identify>& operands,
		const AnyString& name, bool firstChance)
	{
		// target lvid
		uint32_t lvid = operands.lvid;

		// keeping traces of the code logic
		frame->lvids[lvid].resolvedName = name;
		frame->lvids[lvid].referer = operands.self;


		if (name == '=') // it is an assignment, not a real method call
		{
			// remember this special case
			frame->lvids[lvid].pointerAssignment = true;
			// for consistency checks, after transformations on the AST, '=' should be a method call
			// we should have something like: 'foo.=(rhs)'
			if (unlikely(0 == operands.self))
				return complainInvalidSelfRefForVariableAssignment(lvid);

			if (0 != frame->lvids[operands.self].propsetCallSelf)
			{
				auto& cdef  = cdeftable.classdef(CLID{frame->atomid, operands.self});
				auto& spare = cdeftable.substitute(lvid);
				spare.import(cdef);
				frame->lvids[lvid].propsetCallSelf =
					frame->lvids[operands.self].propsetCallSelf;
			}
			return true;
		}

		if (operands.self != 0)
		{
			if (not frame->verify(operands.self))
				return false;

			if (frame->lvids[operands.self].pointerAssignment)
			{
				// since self was marked as an 'assignment', we're trying to resolve here '^()'
				if (unlikely(name != "^()"))
				{
					ice() << "invalid resolve name for assignment (got '" << name << "')";
					return false;
				}

				if (0 != frame->lvids[operands.self].propsetCallSelf)
				{
					auto& cdef  = cdeftable.classdef(CLID{frame->atomid, operands.self});
					auto& spare = cdeftable.substitute(lvid);
					spare.import(cdef);
					frame->lvids[lvid].propsetCallSelf =
						frame->lvids[operands.self].propsetCallSelf;
				}

				// remember this special case
				frame->lvids[lvid].pointerAssignment = true;
				return true;
			}
		}

		auto& cdef = cdeftable.classdef(CLID{frame->atomid, lvid});

		// checking if the lvid does not map to a parameter, which  must
		// have already be resolved when instanciating the function
		if (frame->atom.isFunction())
		{
			assert(cdef.clid.lvid() >= 2 + frame->atom.parameters.size());
			if (unlikely(cdef.clid.lvid() < 2 + frame->atom.parameters.size()))
			{
				String errmsg;
				errmsg << CLID{frame->atomid, lvid} << ": should be alreayd resolved";
				return complainOperand(IR::Instruction::fromOpcode(operands), errmsg);
			}
		}

		// list of all possible atoms when resolving 'name'
		assert(multipleResults.empty());
		// Self, if any
		Atom* selfAtom = nullptr;
		// local variable ?
		bool isLocalVar = false;
		// property call ?
		bool isProperty = false;

		if (0 == operands.self)
		{
			// simple variable, function, namespace...

			// checking first for builtin identifiers (void, any...)
			switch (name[0])
			{
				case 'a':
				{
					if (name == "any") // any - nothing to resolve
					{
						frame->lvids[lvid].markedAsAny = true;
						frame->partiallyResolved.erase(cdef.clid);
						cdeftable.substitute(lvid).mutateToAny();
						return true;
					}
					break;
				}
				case 'n':
				{
					if (name == "null")
					{
						frame->partiallyResolved.erase(cdef.clid); // just in case

						auto& opc = cdeftable.substitute(lvid);
						opc.mutateToBuiltin(nyt_ptr);
						opc.qualifiers.ref = false;
						out.emitStore_u64(lvid, 0);
						frame->lvids[lvid].synthetic = false;
						return true;
					}
					break;
				}
				case 'v':
				{
					if (name == "void")
					{
						frame->partiallyResolved.erase(cdef.clid);
						cdeftable.substitute(lvid).mutateToVoid();
						return true;
					}
					break;
				}
				case '_':
				{
					if (name.size() > 1 and name[1] == '_')
					{
						frame->partiallyResolved.erase(cdef.clid); // just in case

						if (name == "__false")
						{
							auto& opc = cdeftable.substitute(lvid);
							opc.mutateToBuiltin(nyt_bool);
							opc.qualifiers.ref = false;
							out.emitStore_u64(lvid, 0);
							frame->lvids[lvid].synthetic = false;
							return true;
						}
						if (name == "__true")
						{
							auto& opc = cdeftable.substitute(lvid);
							opc.mutateToBuiltin(nyt_bool);
							opc.qualifiers.ref = false;
							out.emitStore_u64(lvid, 1);
							frame->lvids[lvid].synthetic = false;
							return true;
						}

						nytype_t type = nany_cstring_to_type_n(name.c_str(), name.size());
						if (unlikely(type == nyt_void))
							return complainUnknownBuiltinType(name);

						cdeftable.substitute(lvid).mutateToBuiltin(type);
						return true;
					}
					break;
				}
			}

			// trying for local variables first
			LVID lvidVar = frame->findLocalVariable(name);
			if (lvidVar != 0)
			{
				// the variable is used, whatever it is (error or not)
				frame->lvids[lvidVar].hasBeenUsed = true;
				frame->lvids[lvid].alias = lvidVar;
				frame->lvids[lvid].synthetic = false;

				if (not frame->verify(lvidVar)) // suppress spurious errors from previous ones
					return false;

				// acquire the variable
				if (canGenerateCode())
					out.emitStore(lvid, lvidVar);

				auto& varcdef = cdeftable.classdef(CLID{frame->atomid, lvidVar});
				if (not varcdef.isBuiltin())
				{
					auto* varAtom = cdeftable.findClassdefAtom(varcdef);
					if (unlikely(varAtom == nullptr))
					{
						if (varcdef.isVoid())
						{
							cdeftable.substitute(cdef.clid.lvid()).mutateToVoid();
							return true;
						}

						ice() << "invalid atom for local scope variable. clid: " << CLID{frame->atomid, lvidVar}
							<< ", " << (uint32_t) varcdef.kind;
						return false;
					}
					multipleResults.emplace_back(std::ref(*varAtom));
					isLocalVar = true;
				}
				else
				{
					// special case - not an atom
					auto& spare = cdeftable.substitute(cdef.clid.lvid());
					spare.mutateToBuiltin(varcdef.kind);
					spare.qualifiers.ref = false;
					return true;
				}
			}
			else
			{
				if (not frame->atom.nameLookupOnChildren(multipleResults, name))
				{
					if (frame->atom.parent)
					{
						auto& parent = *(frame->atom.parent);
						if (not parent.nameLookupFromParent(multipleResults, name))
							isProperty = tryFindProperties(multipleResults, parent, cdef, name);
					}
				}
			}
		}
		else
		{
			assert(frame->verify(operands.self));
			// self.<something to identify>
			if (unlikely(frame->lvids[lvid].markedAsAny))
				return (ice() << "can not perform member lookup on 'any'");

			auto& self = cdeftable.classdef(CLID{frame->atomid, operands.self});
			if (unlikely(self.isBuiltinOrVoid()))
				return complainInvalidMemberRequestNonClass(name, self.kind);

			bool& singleHop = frame->lvids[operands.self].singleHopForReferer;

			selfAtom = cdeftable.findClassdefAtom(self);
			if (selfAtom != nullptr) // the parent has been fully resolved
			{
				// since the parent has been fully resolved, no multiple
				// solution should be available
				assert(frame->partiallyResolved.count(self.clid) == 0
					   or frame->partiallyResolved[self.clid].empty());

				if (not selfAtom->nameLookupOnChildren(multipleResults, name, &singleHop))
					isProperty = tryFindProperties(multipleResults, *selfAtom, cdef, name);
			}
			else
			{
				auto it = frame->partiallyResolved.find(self.clid);
				if (it != frame->partiallyResolved.end())
				{
					auto& selfSolutions = it->second;
					multipleResults.reserve(selfSolutions.size());
					for (auto& atomE: selfSolutions)
						atomE.get().nameLookupOnChildren(multipleResults, name, &singleHop);
				}
			}
		}


		if (not isProperty)
		{
			switch (multipleResults.size())
			{
				case 1: // unique match count
				{
					return emitIdentifyForSingleResult(isLocalVar, cdef, operands, name);
				}
				default: // multiple solutions
				{
					// multiple solutions are possible (probably for a func call)
					// keeping the solutions for later resolution by the real func call
					// (with parameters to find the most appropriate one)
					frame->partiallyResolved[cdef.clid].swap(multipleResults);
					return true;
				}
				case 0: // no identifier found from 'atom map'
				{
					// nothing has been found
					// trying capturing variable from anonymous classes
					if (firstChance)
					{
						if (frame->atom.canCaptureVariabes() and identifyCapturedVar(operands, name))
							return true;
						return complainUnknownIdentifier(selfAtom, frame->atom, name);
					}
					break;
				}
			}
		}
		else
		{
			switch (multipleResults.size())
			{
				case 1: // unique match count
				{
					auto& propatom = multipleResults[0].get();

					// Generate code only for getter
					// setter will be called later, when enough information will be provided
					// (the 'value' parameter is not available yet)
					if (not cdef.qualifiers.propset)
					{
						if (Config::Traces::properties)
						{
							trace() << "property: resolved '" << name << "' from '"
								<< frame->atom.caption() << "' as getter " << cdef.clid;
						}
						return emitIdentifyForProperty(operands, cdef, propatom);
					}
					else
					{
						if (Config::Traces::properties)
						{
							trace() << "property: resolved '" << name << "' from '"
								<< frame->atom.caption() << "' as setter " << cdef.clid;
						}

						// this lvid is a call to a property setter
						// must adjust the code accordingly
						// -1 'self' does not exist for this property (global property)
						uint32_t propsetself = (operands.self != 0) ? operands.self : (uint32_t) -1;
						frame->lvids[lvid].propsetCallSelf = propsetself;

						auto& spare = cdeftable.substitute(lvid);
						spare.mutateToAtom(&propatom);
						return true;
					}
				}
				default: // multiple solutions
				{
					// multiple solutions are not acceptable for properties
					error() << "ambigous property call for '" << name << '\'';
					break;
				}
				case 0: // no identifier found from 'atom map'
				{
					error() << "no property found for '" << name << '\'';
					break;
				}
			}
		}
		return false;
	}



	void SequenceBuilder::visit(const IR::ISA::Operand<IR::ISA::Op::identify>& operands)
	{
		assert(frame != nullptr);

		AnyString name = currentSequence.stringrefs[operands.text];
		bool ok = identify(operands, name);
		if (unlikely(not ok))
			frame->invalidate(operands.lvid);
		multipleResults.clear();
	}





} // namespace Instanciate
} // namespace Pass
} // namespace Nany
