#include "context.h"
#include "details/ast/ast.h"

using namespace Yuni;





namespace Nany
{
namespace IR
{
namespace Producer
{


	void Context::useNamespace(const AnyString& nmspc)
	{
		if (not nmspc.empty())
		{
			nmspc.words(".", [&](const AnyString& part) -> bool
			{
				sequence.emitNamespace(part);
				return true;
			});
		}
	}


	void Context::generateLineIndexes(const AnyString& content)
	{
		uint line = 1;
		uint length = content.size();
		for (uint i = 0; i != length; ++i)
		{
			if (content[i] == '\n')
				offsetToLine.emplace(i, ++line);
		}
	}


	void Context::prepareReuseForLiterals()
	{
		// new (+2)
		//	 type-decl
		//	 |   identifier: i64
		//	 call (+3)
		//		 call-parameter
		//			 expr
		//				 register: <lvid>

		auto& cache = reuse.literal;

		cache.node = new AST::Node{AST::rgNew};
		AST::Node::Ptr typeDecl = new AST::Node{AST::rgTypeDecl};
		cache.node->children.push_back(typeDecl);
		cache.classname = new AST::Node{AST::rgIdentifier};
		typeDecl->children.push_back(cache.classname);

		AST::Node::Ptr call = new AST::Node{AST::rgCall};
		cache.node->children.push_back(call);
		AST::Node::Ptr callParam = new AST::Node{AST::rgCallParameter};
		call->children.push_back(callParam);
		AST::Node::Ptr expr = new AST::Node{AST::rgExpr};
		callParam->children.push_back(expr);
		cache.lvidnode = new AST::Node{AST::rgRegister};
		expr->children.push_back(cache.lvidnode);
	}


	void Context::prepareReuseForClasses()
	{
		reuse.operatorDefault.node
			= AST::createNodeFunc(reuse.operatorDefault.funcname);

		reuse.operatorClone.node
			= AST::createNodeFuncCrefParam(reuse.operatorClone.funcname, "rhs");
	}


	void Context::prepareReuseForVariableMembers()
	{
		reuse.func.node = AST::createNodeFunc(reuse.func.funcname);

		AST::Node::Ptr funcBody = new AST::Node{AST::rgFuncBody};
		(reuse.func.node)->children.push_back(funcBody);

		AST::Node::Ptr expr = new AST::Node{AST::rgExpr};
		funcBody->children.push_back(expr);

		// intrinsic (+2)
		//	   entity (+3)
		//	   |   identifier: nanyc
		//	   |   identifier: fieldset
		//	   call (+7)
		//		   call-parameter
		//		   |   expr
		//		   |	   <expr A>
		//		   call-parameter
		//		   |   expr
		//		   |	   <expr B>
		AST::Node::Ptr intrinsic = new AST::Node{AST::rgIntrinsic};
		expr->children.push_back(intrinsic);
		intrinsic->children.push_back(AST::createNodeIdentifier("^fieldset"));

		AST::Node::Ptr call = new AST::Node{AST::rgCall};
		intrinsic->children.push_back(call);

		// param 2 - expr
		{
			reuse.func.callparam = new AST::Node{AST::rgCallParameter};
			call->children.push_back(reuse.func.callparam);
		}
		// param text varname
		{
			AST::Node::Ptr callparam = new AST::Node{AST::rgCallParameter};
			call->children.push_back(callparam);
			AST::Node::Ptr pexpr = new AST::Node{AST::rgExpr};
			callparam->children.push_back(pexpr);

			reuse.func.varname = new AST::Node{AST::rgStringLiteral};
			pexpr->children.push_back(reuse.func.varname);
		}
	}


	void Context::prepareReuseForClosures()
	{
		reuse.closure.node = new AST::Node{AST::rgExpr};

		// expr
		//	 expr-value
		//		 new
		//			 type-decl
		//				 class
		//					 class-body
		//						 expr
		//						 |   expr-value
		//						 |	   function (+2)
		//						 |		   function-kind
		//						 |		   |   function-kind-operator (+2)
		//						 |		   |	   function-kind-opname: ()
		//						 |		   func-body
		//						 |			   return-inline (+3)
		//						 |				   expr
		//						 |				   |   ...

		auto& exprValue = reuse.closure.node->append<AST::rgExprValue>();
		auto& nnew	  = exprValue.append<AST::rgNew>();
		auto& typedecl  = nnew.append<AST::rgTypeDecl>();
		auto& nclass	= typedecl.append<AST::rgClass>();
		auto& cbody	 = nclass.append<AST::rgClassBody>();
		auto& bodyExpr  = cbody.append<AST::rgExpr>();
		auto& bodyValue = bodyExpr.append<AST::rgExprValue>();

		auto& func = bodyValue.append<AST::rgFunction>();

		auto& funcKind = func.append<AST::rgFunctionKind>();
		auto& kindOp   = funcKind.append<AST::rgFunctionKindOperator>();
		auto& funcname = kindOp.append<AST::rgFunctionKindOpname>();
		funcname.text = "()";

		auto& params = func.append<AST::rgFuncParam>();
		reuse.closure.params = &params;

		auto& rettype = func.append<AST::rgFuncReturnType>();
		reuse.closure.rettype = &rettype;

		auto& funcBody = func.append<AST::rgFuncBody>();
		reuse.closure.funcbody = &funcBody;
	}


	void Context::prepareReuseForIn()
	{
		// expr
		//   expr-value (+2)
		//       expr-group
		//       |   expr-value
		//       |       identifier: expr
		//       expr-sub-dot
		//           identifier: makeview
		//               call
		//                   call-parameter
		//                       expr
		//                           expr-value
		//                               function (+4)
		//                                   function-kind
		//                                   |   function-kind-function
		//                                   func-params
		//                                   |   func-param (+2)
		//                                   |       cref
		//                                   |       identifier: i
		//                                   func-return-type
		//                                   |   type
		//                                   |       type-qualifier
		//                                   |           ref
		//                                   func-body
		//                                       return
		//                                           expr
		//                                               expr-value
		//                                                   identifier: predicate
		//
		reuse.inset.node = new AST::Node{AST::rgExpr};
		auto& exprValue = reuse.inset.node->append<AST::rgExprValue>();

		auto& exprGroup = exprValue.append<AST::rgExprGroup>();
		reuse.inset.container = &exprGroup;

		auto& subexpr = exprValue.append<AST::rgExprSubDot>();
		auto& viewname = subexpr.append<AST::rgIdentifier>();
		reuse.inset.viewname = &viewname;

		auto& call = viewname.append<AST::rgCall>();
		auto& parameter = call.append<AST::rgCallParameter>();
		auto& paramExpr = parameter.append<AST::rgExpr>();
		auto& paramExprValue = paramExpr.append<AST::rgExprValue>();
		auto& func = paramExprValue.append<AST::rgFunction>();
		auto& kind = func.append<AST::rgFunctionKind>();
		kind.append<AST::rgFunctionKindFunction>();

		auto& funcparams = func.append<AST::rgFuncParams>();
		auto& funcparam = funcparams.append<AST::rgFuncParam>();
		funcparam.append<AST::rgCref>();
		auto& cursorname = funcparam.append<AST::rgIdentifier>();
		reuse.inset.cursorname = &cursorname;

		auto& rettype = func.append<AST::rgFuncReturnType>();
		auto& type = rettype.append<AST::rgType>();
		auto& typequalifier = type.append<AST::rgTypeQualifier>();
		typequalifier.append<AST::rgRef>();

		auto& funcbody = func.append<AST::rgFuncBody>();
		auto& ret = funcbody.append<AST::rgReturn>();
		reuse.inset.predicate = &ret;

		reuse.inset.premadeAlwaysTrue = new AST::Node{AST::rgExpr};
		auto& alwaysTrue = reuse.inset.premadeAlwaysTrue->append<AST::rgExprValue>();
		alwaysTrue.append<AST::rgIdentifier>();
	}



} // namespace Producer
} // namespace IR
} // namespace Nany
