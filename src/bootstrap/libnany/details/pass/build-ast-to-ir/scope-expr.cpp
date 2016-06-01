#include <yuni/yuni.h>
#include "details/pass/build-ast-to-ir/scope.h"
#include "details/utils/check-for-valid-identifier-name.h"
#include "libnany-config.h"

using namespace Yuni;




namespace Nany
{
namespace IR
{
namespace Producer
{

	inline bool Scope::visitASTExprIdentifier(const AST::Node& node, LVID& localvar)
	{
		// value fetching
		emitDebugpos(node);
		uint32_t rid = sequence().emitStackalloc(nextvar(), nyt_any);
		sequence().emitIdentify(rid, node.text, localvar);
		localvar = rid;

		if (not node.children.empty())
			return visitASTExprContinuation(node, localvar);
		return true;
	}


	inline bool Scope::visitASTExprRegister(const AST::Node& node, LVID& localvar)
	{
		assert(not node.text.empty());
		localvar = node.text.to<uint32_t>();
		assert(localvar != 0);
		if (node.children.empty())
			return true;
		return visitASTExprContinuation(node, localvar);
	}


	bool Scope::visitASTExprContinuation(const AST::Node& node, LVID& localvar, bool allowScope)
	{
		bool success = true;
		for (auto& childptr: node.children)
		{
			auto& child = *childptr;
			switch (child.rule)
			{
				case AST::rgIdentifier: success &= visitASTExprIdentifier(child, localvar); break;
				case AST::rgExprValue:
				case AST::rgExprGroup:  success &= visitASTExpr(child, localvar); break;
				case AST::rgCall:       success &= visitASTExprCall(&child, localvar, &node); break;
				case AST::rgExprSubDot: success &= visitASTExprSubDot(child, localvar); break;
				case AST::rgNumber:     success &= visitASTExprNumber(child, localvar); break;
				case AST::rgNew:        success &= visitASTExprNew(child, localvar); break;

				// typing - same as std expr
				case AST::rgTypeSubDot: success &= visitASTExprSubDot(child, localvar); break;

				// strings
				case AST::rgString:     success &= visitASTExprString(child, localvar); break;
				// when directly called from an expr, this rule is generated by the compiler itself
				case AST::rgStringLiteral: success &= visitASTExprStringLiteral(child, localvar); break;

				// special stuff
				case AST::rgTypeof:     success &= visitASTExprTypeof(child, localvar); break;
				case AST::rgIntrinsic:  success &= visitASTExprIntrinsic(child, localvar); break;

				case AST::rgIf:         success &= visitASTExprIfExpr(child, localvar); break;
				case AST::rgWhile:      success &= visitASTExprWhile(child); break;

				case AST::rgExprTemplate:
				case AST::rgExprTypeTemplate: success &= visitASTExprTemplate(child, localvar); break;

				case AST::rgIn:         success &= visitASTExprIn(child, localvar); break;
				case AST::rgFunction:   success &= visitASTExprClosure(child, localvar); break;

				case AST::rgAttributes: success &= visitASTAttributes(child); break;

				// special for internal AST manipulation
				case AST::rgRegister:   success &= visitASTExprRegister(child, localvar); break;

				// scope may appear in expr (when expr are actually statements)
				case AST::rgScope: {
					if (allowScope) {
						success &= visitASTExprScope(child);
						break;
					}
				}
				// [[fallthru]]
				default:
					success = ICEUnexpectedNode(child, "[expr/continuation]");
			}
		}
		return success;
	}


	void Scope::emitExprAttributes(uint32_t& localvar)
	{
		assert(!!pAttributes);
		auto& attrs = *pAttributes;

		// allow to push a synthetic object (type)
		if (unlikely(attrs.flags(Attributes::Flag::pushSynthetic)))
		{
			// do not report errors
			attrs.flags -= Attributes::Flag::pushSynthetic;
			if (debugmode)
				sequence().emitComment(String("#[__synthetic: %") << localvar << ']');
			sequence().emitPragmaSynthetic(localvar, false);
		}
	}


	bool Scope::visitASTExpr(const AST::Node& orignode, LVID& localvar, bool allowScope)
	{
		assert(not orignode.children.empty());

		// reset the value of the localvar, result of the expr
		localvar = 0;

		// expr
		// |   expr-value
		const AST::Node* nodeptr = &orignode;
		const AST::Node* attrnode = nullptr;
		if (orignode.rule == AST::rgExpr)
		{
			switch (orignode.children.size())
			{
				case 1:
				{
					// do not take into consideration this node
					// (it will generate useless scopes)
					auto& firstchild = *(orignode.children[0]);
					if (firstchild.rule == AST::rgExprValue)
						nodeptr = &firstchild;
					break;
				}
				case 2:
				{
					auto& firstchild = *(orignode.children[0]);
					if (firstchild.rule == AST::rgAttributes)
					{
						auto& sndchild = *(orignode.children[1]);
						if (sndchild.rule == AST::rgExprValue)
						{
							nodeptr = &sndchild;
							attrnode = &firstchild; // do not forget to visit this node
						}
					}
					break;
				}
			}
		}

		auto& node = *nodeptr;
		assert(node.rule == AST::rgExpr or node.rule == AST::rgExprValue
			or node.rule == AST::rgExprGroup or node.rule == AST::rgTypeDecl);
		assert(not node.children.empty());

		// always creating a new scope for a expr
		IR::Producer::Scope scope{*this};
		scope.emitDebugpos(node);

		if (unlikely(attrnode))
			scope.visitASTAttributes(*attrnode);

		bool r = scope.visitASTExprContinuation(node, localvar, allowScope);
		if (r and localvar != 0 and localvar != (uint32_t) -1)
		{
			scope.emitTmplParametersIfAny();
			scope.sequence().emitEnsureTypeResolved(localvar);

			if (unlikely(!!scope.pAttributes))
				scope.emitExprAttributes(localvar);
		}
		return r;
	}





} // namespace Producer
} // namespace IR
} // namespace Nany
