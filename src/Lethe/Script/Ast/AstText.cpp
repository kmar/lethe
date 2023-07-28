#include "AstText.h"
#include "NamedScope.h"
#include <Lethe/Core/String/StringBuilder.h>
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstText)

// AstText

AstNode *AstText::ResolveTemplateScope(AstNode *&ntext) const
{
	const AstNode *it = this;

	while (it->parent && it->parent->type == AST_OP_SCOPE_RES)
		it = it->parent;

	if (it->type == AST_OP_SCOPE_RES)
		return it->ResolveTemplateScope(ntext);

	auto *tmp = AstStaticCast<const AstNode *>(this);
	ntext = const_cast<AstNode *>(tmp);

	const NamedScope *nscope = nullptr;
	return scopeRef->FindSymbolFull(text, nscope);
}

void AstText::GetQTextBuilder(StringBuilder &sb) const
{
	const auto *nscope = scopeRef;

	sb += text;

	while (nscope)
	{
		if (!nscope->name.IsEmpty())
		{
			sb.Prepend("::");
			sb.Prepend(nscope->name);

			// note: this is necessary for instantiated templates because they are fully resolved
			if (nscope->name.Find("::") >= 0)
				break;
		}

		nscope = nscope->parent;
	}
}

String AstText::GetQTextSlow() const
{
	StringBuilder sb;
	GetQTextBuilder(sb);

	if (text == sb.Get())
		return text;

	return sb.Get();
}

String AstText::GetQText(CompiledProgram &p) const
{
	StringBuilder sb;
	GetQTextBuilder(sb);

	if (text == sb.Get())
		return text;

	return p.AddString(sb.Get());
}

String AstText::GetTextRepresentation() const
{
	String res = Super::GetTextRepresentation();
	res += " ";
	res += String(text.Ansi()).Escape();
	return res;
}

bool AstText::GetTemplateTypeText(StringBuilder &sb) const
{
	const NamedScope *nscope = nullptr;
	auto *symTarget = scopeRef->FindSymbolFull(text, nscope);

	// FIXME: the second condition covers recursion on struct name => this should be handled elsewhere...
	if (symTarget && symTarget != parent)
		return symTarget->GetTemplateTypeText(sb);

	LETHE_RET_FALSE(!text.IsEmpty());

	nscope = scopeRef;

	StringBuilder sbqt;
	GetQTextBuilder(sbqt);
	sb += sbqt.Get();

	return true;
}

void AstText::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstText *>(n);
	tmp->text = text;
}


}
