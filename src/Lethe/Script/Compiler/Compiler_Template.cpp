#include "Compiler.h"

#include "AstIncludes.h"

#include <Lethe/Core/Collect/Pair.h>
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

bool Compiler::GenerateTemplateName(ErrorHandler &eh, const String &qname, StringBuilder &sb, AstNode *instanceNode, bool &nestedTemplate)
{
	LETHE_ASSERT(!qname.IsEmpty());

	auto *insideTemplate = instanceNode;

	while (insideTemplate && !(insideTemplate->qualifiers & AST_Q_TEMPLATE))
		insideTemplate = insideTemplate->parent;

	sb.AppendFormat("%s<", qname.Ansi());

	for (Int i=0; i<instanceNode->nodes.GetSize(); i++)
	{
		auto *itype = instanceNode->nodes[i];
		itype->AppendTypeQualifiers(sb);

		if (!itype->GetTemplateTypeText(sb))
		{
			if (insideTemplate && itype->type == AST_IDENT)
			{
				if (AstStaticCast<AstTypeStruct *>(insideTemplate)->IsTemplateArg(AstStaticCast<AstText *>(itype)->text))
				{
					nestedTemplate = true;
				}
			}

			// try to resolve...
			UniquePtr<AstNode> resolved = itype->Clone();
			resolved->Resolve(eh);
			resolved->parent = itype->parent;

			const auto *tnode = resolved->GetTypeNode();

			if (tnode && tnode->GetTemplateTypeText(sb))
			{
				// ok, it was a typedef
			}
			else if (resolved->type == AST_IDENT && resolved->flags & AST_F_RESOLVED)
			{
				auto *targScope = resolved->target->scopeRef;

				StringBuilder sbname;
				targScope->GetFullScopeName(sbname);

				LETHE_RET_FALSE(GenerateTemplateName(eh, sbname.Get(), sb, resolved, nestedTemplate));
			}
			else
				return eh.Error(itype, "cannot get template type text (unsupported)");
		}

		if (i+1 < instanceNode->nodes.GetSize())
			sb += ',';
	}

	sb += '>';

	return true;
}

bool Compiler::InstantiateTemplates(ErrorHandler &eh)
{
	// grab all templates and (potential) template instances
	AstIterator ait(progList);

	Array<AstNode *> templates;
	Array<AstNode *> templateInstances;

	while (auto *n = ait.Next())
	{
		if ((n->flags & AST_F_TEMPLATE_INSTANCE))
			templateInstances.Add(n);

		if (n->qualifiers & AST_Q_TEMPLATE)
			templates.Add(n);
	}

	// collect unique template instances (+names) => MUST(!) find matching template first, emulating "resolve"
	HashMap<String, Pair<AstNode *, AstNode *>> uniqueInstances;

	HashMap<AstNode *, String> instanceNames;

	for (auto *it : templateInstances)
	{
		auto *instanceNode = it;

		auto *target = it->ResolveTemplateScope(it);

		if (!target)
			return eh.Error(it, String::Printf("couldn't find template target for %s", it->GetTextRepresentation().Ansi()));

		// now that we have a target, make sure it's really a template
		if (!(target->qualifiers & AST_Q_TEMPLATE))
			return eh.Error(it, String::Printf("target not a template for %s", it->GetTextRepresentation().Ansi()));

		// templates only supported for struct/classes
		LETHE_ASSERT(target->type == AST_STRUCT || target->type == AST_CLASS);

		auto *templ = AstStaticCast<AstTypeStruct *>(target);

		if (instanceNode->nodes.GetSize() != templ->templateArgs.GetSize())
		{
			return eh.Error(it, String::Printf("invalid number of template arguments, expected %d got %d",
				templ->templateArgs.GetSize(),
				instanceNode->nodes.GetSize()
			));
		}

		String qname;

		// okay, we now have to build unique name
		if (templ->nodes[0]->type == AST_IDENT)
			qname = AstStaticCast<const AstText *>(templ->nodes[0])->GetQTextSlow();

		// ... and now, generate names for types
		StringBuilder sb;

		bool nestedTemplate = false;
		LETHE_RET_FALSE(GenerateTemplateName(eh, qname, sb, instanceNode, nestedTemplate));

		if (nestedTemplate)
			continue;

		String instName = sb.Get();
		instName = AddString(instName.Ansi());

		LETHE_ASSERT(it->type == AST_OP_SCOPE_RES || it->type == AST_IDENT);
		AstStaticCast<AstText *>(it)->text = instName;

		instanceNames[instanceNode] = instName;
		uniqueInstances[instName] = Pair<AstNode *, AstNode *>(templ, instanceNode);
	}

	// next step (the most important) is to clone AST subtree and remap instances!
	// there's still a couple of things to do:
	// - modify internal typedefs for instance
	// - remap names for instances

	HashMap<String, AstNode *> templateToInstance;

	for (int idx=0; idx<uniqueInstances.GetSize(); idx++)
	{
		const auto &it = uniqueInstances.GetKey(idx);

		const auto &iname = it.key;

		auto *templ = it.value.first;
		auto *instPoint = it.value.second;

		UniquePtr<AstTypeStruct> instance = AstStaticCast<AstTypeStruct *>(templ->Clone());

		templateToInstance[iname] = instance.Get();

		HashMap<AstNode *, AstNode *> cloneRemap;
		Array<AstNode *> nestedInstances;

		// build clone remap node ptrs, assuming traversal is in the same order
		{
			AstIterator it0(templ);
			AstIterator it1(instance.Get());

			while (auto *oldn = it0.Next())
			{
				auto *newn = it1.Next();
				LETHE_ASSERT(newn);

				if (oldn->flags & AST_F_TEMPLATE_INSTANCE)
				{
					if (instanceNames.FindIndex(oldn) < 0)
					{
						nestedInstances.Add(newn);
					}
					else
					{
						// note: due to the way my hashmaps work, this has to be copied via temp copy of a string...
						auto tmp = instanceNames[oldn];
						instanceNames[newn] = tmp;
						templateInstances.Add(newn);
					}
				}

				cloneRemap[oldn] = newn;
			}

			LETHE_ASSERT(it1.Next() == nullptr);
		}

		instance->overrideName = iname;
		instance->nodes[0]->ClearNodes();
		instance->qualifiers &= ~AST_Q_TEMPLATE;
		instance->qualifiers |= AST_Q_TEMPLATE_INSTANTIATED;

		LETHE_ASSERT(instance->nodes[0]->type == AST_IDENT);
		auto inameShort = AstStaticCast<AstText *>(instance->nodes[0])->text;

		// remap typedefs
		for (auto &iter : instance->templateArgs)
			iter.typedefNode = AstStaticCast<AstTypeDef *>(cloneRemap[iter.typedefNode]);

		for (Int i=0; i<instance->templateArgs.GetSize(); i++)
		{
			const auto &arg = instance->templateArgs[i];

			auto *argtype = instPoint->nodes[i];

			auto *n = argtype;

			if (n->type == AST_OP_SCOPE_RES)
				n->Resolve(eh);

			argtype = n->Clone();
			auto *old = arg.typedefNode->nodes[0];
			arg.typedefNode->ReplaceChild(old, argtype);
			delete old;
			argtype->parent = n->parent;
		}

		// okay, now we have to clone AND remap scopeRefs...

		AstIterator nit(instance.Get());

		HashMap<NamedScope *, NamedScope *> scopeRemap;

		while (auto *n = nit.Next())
		{
			// if it's not child scope of template struct, ignore
			if (!instance->scopeRef->IsParentOf(n->scopeRef))
				continue;

			auto ci = scopeRemap.Find(n->scopeRef);

			if (ci != scopeRemap.End())
				continue;

			// clone scope
			SharedPtr<NamedScope> newscope = new NamedScope(*n->scopeRef);
			newscope->scopes.Clear();
			newscope->namedScopes.Clear();

			if (newscope->node)
			{
				auto ci2 = cloneRemap.Find(newscope->node);

				if (ci2 != cloneRemap.End())
					newscope->node = ci2->value;
			}

			if (n == instance)
				newscope->name = iname;

			auto ci2 = scopeRemap.Find(newscope->parent);

			if (ci2 != scopeRemap.End())
				newscope->parent = ci2->value;

			scopeRemap[n->scopeRef] = newscope;

			LETHE_ASSERT(newscope->parent);

			if (newscope->name.IsEmpty())
				newscope->parent->scopes.Add(newscope);
			else
			{
				if (newscope->parent->namedScopes.FindIndex(newscope->name) >= 0)
					return eh.Error(n, String::Printf("named scope already found (during template instantiation) name=`%s'", newscope->name.Ansi()));

				newscope->parent->namedScopes[newscope->name] = newscope;
			}
		}

		nit = AstIterator(instance.Get());

		auto remapNode = [&](AstNode *&n)
		{
			if (!n)
				return;

			auto ci = cloneRemap.Find(n);

			if (ci != cloneRemap.End())
				n = ci->value;
		};

		while (auto *n = nit.Next())
		{
			if (n->type == AST_CALL)
				remapNode(AstStaticCast<AstCall *>(n)->forceFunc);

			// remap targets; necessary for return
			remapNode(n->target);

			auto ci = scopeRemap.Find(n->scopeRef);

			if (ci != scopeRemap.End())
				n->scopeRef = ci->value;

			ci = scopeRemap.Find(n->symScopeRef);

			if (ci != scopeRemap.End())
				n->symScopeRef = ci->value;
		}

		// remap member nodes, operators and deferred nodes
		for (auto &&it2 : scopeRemap)
		{
			auto *newscope = it2.value;

			for (auto &it3 : newscope->members)
			{
				auto ci = cloneRemap.Find(it3.value);

				if (ci != cloneRemap.End())
					it3.value = ci->value;
			}

			for (auto &it3 : newscope->operators)
			{
				auto ci = cloneRemap.Find(it3);

				if (ci != cloneRemap.End())
					it3 = ci->value;
			}

			for (auto &it3 : newscope->deferred)
			{
				auto ci = cloneRemap.Find(it3);

				if (ci != cloneRemap.End())
					it3 = ci->value;
			}

			// also remap result ptr if needed
			remapNode(newscope->resultPtr);
		}

		// inject short name as virtual scope
		instance->scopeRef->nameAlias = inameShort;

		HashMap<AstNode *, String> nestedNameMap;

		for (Int ni=nestedInstances.GetSize()-1; ni >= 0; ni--)
		{
			auto *nested = nestedInstances[ni];
			LETHE_ASSERT(nested->type == AST_IDENT);
			auto *txt = AstStaticCast<AstText *>(nested);

			const NamedScope *dummy;
			auto *sym = nested->scopeRef->FindSymbolFull(txt->text, dummy);

			if (!sym || !(sym->qualifiers & AST_Q_TEMPLATE))
				return eh.Error(nested, String::Printf("couldn't find outer template `%s'", txt->text.Ansi()));

			LETHE_ASSERT(sym->type == AST_STRUCT || sym->type == AST_CLASS);

			auto qname = AstStaticCast<AstText *>(sym->nodes[0])->GetQTextSlow();

			LETHE_ASSERT(!qname.IsEmpty());

			StringBuilder sb;

			sb.AppendFormat("%s<", qname.Ansi());

			for (Int i=0; i<nested->nodes.GetSize(); i++)
			{
				auto *n = nested->nodes[i];

				auto ci = nestedNameMap.Find(n);

				if (ci != nestedNameMap.End())
				{
					sb += ci->value;
					goto nestedFound;
				}

				if (n->type == AST_IDENT)
				{
					LETHE_ASSERT(n->type == AST_IDENT);
					auto *ntxt = AstStaticCast<AstText *>(n);

					// look up in typedefs
					auto *targ = instance->FindTemplateArg(ntxt->text);

					if (targ)
					{
						// replace
						auto *newnode = targ->typedefNode->nodes[0]->Clone();
						nested->ReplaceChild(n, newnode);
						delete n;
						n = newnode;
					}
				}

				n->AppendTypeQualifiers(sb);

				if (!n->GetTemplateTypeText(sb))
					return eh.Error(n, "couldn't generate template name");

			nestedFound:

				if (i+1 < nested->nodes.GetSize())
					sb += ',';
			}

			sb += '>';

			auto nestedInstName = AddString(sb.Ansi());

			nestedNameMap[nested] = nestedInstName;

			if (uniqueInstances.FindIndex(sb.Get()) < 0)
			{
				// add new unique instance!
				LETHE_ASSERT(sym->qualifiers & AST_Q_TEMPLATE);
				uniqueInstances[nestedInstName] = Pair<AstNode *, AstNode *>(sym, nested);
			}

			instanceNames[nested] = nestedInstName;
			templateInstances.Add(nested);
		}

		// and finally add instance to root scope...
		templ->parent->Add(instance.Detach());
	}

	// one step still remains, we need to map instance points to newly created instances

	for (auto *it : templateInstances)
	{
		auto ci = instanceNames.Find(it);

		// skip nested instances
		if (ci == instanceNames.End())
			continue;

		LETHE_ASSERT(templateToInstance.FindIndex(ci->value) >= 0);

		auto *target = templateToInstance[ci->value];

		while (it->parent && it->parent->type == AST_OP_SCOPE_RES)
		{
			it->target = target;
			it = it->parent;
		}

		it->target = target;
	}

	// skip codegen and typegen for original templates
	for (auto *it : templates)
		it->flags |= AST_F_SKIP_CGEN | AST_F_TYPE_GEN;

	return true;
}


}
