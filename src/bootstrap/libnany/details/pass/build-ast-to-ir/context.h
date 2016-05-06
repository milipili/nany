#pragma once
#include <yuni/yuni.h>
#include <yuni/core/noncopyable.h>
#include "details/fwd.h"
#include "details/ir/sequence.h"
#include "details/reporting/report.h"
#include "details/grammar/nany.h"
#include "nany/nany.h"
#include <map>
#include <array>
#include <cassert>




namespace Nany
{
namespace IR
{
namespace Producer
{

	// forward declaration
	class Scope;




	/*!
	** \brief Context for IR generation
	*/
	class Context final : public Yuni::NonCopyable<Context>
	{
	public:
		//! \name Constructor & Destructor
		//@{
		//! Default constructor
		explicit Context(nybuild_cf_t&, Sequence&, Logs::Report);
		//@}


		//! \name Utilities: IR generation
		//@{
		/*!
		** \brief Generate opcode for using a namespace
		** \param nmspc namespace (ex: std.nany.example)
		*/
		void useNamespace(const AnyString& nmspc);

		/*!
		** \brief Generate a mapping between input offsets and line numbers
		*/
		void generateLineIndexes(const AnyString& content);
		//@}


		//! \name Reuse nodes
		//@{
		//! re-use objects for classes (ctor)
		void prepareReuseForClasses();
		//! re-use objects for literals
		void prepareReuseForLiterals();
		//! re-use objects for clusores
		void prepareReuseForClosures();
		//! re-use objects for variable members
		void prepareReuseForVariableMembers();
		//! re-use objects for in
		void prepareReuseForIn();
		//! re-use objects for for..do
		void prepareReuseForLoops();
		//@}


	public:
		//! Information about the current build
		nybuild_cf_t& cf;
		//! Linked IR sequence
		Sequence& sequence;
		//! Reporting
		Logs::Report report;
		//! Has debug info ?
		bool debuginfo = true;

		//! Debug source filename
		Yuni::String dbgSourceFilename;
		//! Map contet offset (0-based - bytes) -> lines (1-based, from source input)
		std::map<uint, uint> offsetToLine;

		struct {
			struct {
				AST::Node::Ptr node;
				AST::Node::Ptr classname;
				AST::Node::Ptr lvidnode;
			}
			literal;

			struct {
				AST::Node::Ptr createObject;
				YString text;
			}
			string;

			struct {
				AST::Node::Ptr node;
				AST::Node* funcname = nullptr;
				AST::Node* varname = nullptr;
				AST::Node* callparam = nullptr;
			}
			func;

			struct {
				AST::Node::Ptr node;
				AST::Node* funcname = nullptr;
			}
			operatorDefault;

			struct {
				AST::Node::Ptr node;
				AST::Node* funcname = nullptr;
			}
			operatorClone;

			struct {
				AST::Node::Ptr node;
				AST::Node* funcbody = nullptr;
				AST::Node* params = nullptr;
				AST::Node* rettype = nullptr;
			}
			closure;

			struct {
				AST::Node::Ptr node;
				AST::Node* container = nullptr;
				AST::Node* viewname = nullptr;
				AST::Node* elementname = nullptr;
				AST::Node* predicate = nullptr;
				AST::Node::Ptr premadeAlwaysTrue;
			}
			inset;

			struct {
				AST::Node::Ptr node;
				std::array<AST::Node*,2> viewlvid;
				std::array<AST::Node*,4> cursorname;
				AST::Node* elementname = nullptr;
				AST::Node* scope;
			}
			loops;
		}
		reuse;

	private:
		friend class Scope;

	private:
		uint32_t pPreviousDbgOffset = 0;
		uint32_t pPreviousDbgLine = 0;

	}; // class Producer






} // namespace Producer
} // namespace IR
} // namespace Nany

#include "scope.h"
#include "context.hxx"
