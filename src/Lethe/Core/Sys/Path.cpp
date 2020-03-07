#include "Path.h"
#include "Platform.h"

/*
	Problems to solve (of course Windows only:)
	ADS vs drive
	c:file.txt is file.txt relative to drive c
	but file.txt:ads.txt is ADS stream
	will use a hack: prefix is anything before : (but not before . and /)
*/

namespace lethe
{

// Path

Path::Path()
{
}

Path::Path(const String &pth)
{
	Make(pth);
}

Path &Path::Clear()
{
	finalPath.Clear();
	return *this;
}

Path &Path::Reset()
{
	finalPath.Reset();
	return *this;
}

Path &Path::Shrink()
{
	finalPath.Shrink();
	return *this;
}

Path &Path::Make(const String &pth)
{
	finalPath = pth;
	return Normalize();
}

// normalize path
void Path::Normalize(String &pth)
{
	// first, convert backslashes to slashes
	pth.Replace('\\', '/');

	// remove useless ./
	while (pth.Replace("/./", "/"));

	while (pth.StartsWith("./"))
		pth.Erase(0, 2);

	// compress more slashes in a row to 1 (except first occurrence where we keep at most 2)
	bool first = 1;
	int pos = 0;

	for (;;)
	{
		int sl = pth.Find('/', pos);

		if (sl < 0)
			break;

		int count = 1;
		int tmp = sl+1;

		while (tmp < pth.GetLength() && pth[tmp] == '/')
		{
			tmp++;
			count++;
		}

		if (first)
		{
			if (count > 2)
			{
				pth.Erase(sl+2, count-2);
				pos = sl+2;
			}
			else
				pos = sl+count;

			first = 0;
			continue;
		}

		if (count > 1)
			pth.Erase(sl+1, count-1);

		pos = sl+1;
	}

	bool changed;

	do
	{
		changed = 0;
		// trim whitespace
		pth.Trim();

		if (pth.IsEmpty())
			return;

		// remove quotes if any
		if (pth.GetLength() >= 2 && pth[0] == pth[pth.GetLength()-1] && (pth[0] == '\'' || pth[0] == '"'))
		{
			pth.Erase(0, 1);
			pth.Erase(pth.GetLength()-1, 1);
			changed = 1;
		}

		// strip trailing slashes except for after : (stupid win!)
		if (pth.GetLength() > 1 && pth[pth.GetLength()-1] == '/'
#if LETHE_OS_WINDOWS
				&& pth[pth.GetLength()-2] != ':'
#endif
		   )
		{
			pth.Erase(pth.GetLength()-1, 1);
			changed = 1;
		}
	}
	while(changed);

	// last step: remove useless /../
	do
	{
		changed = 0;
		pos = 1;

		// note: > 0 is important here!
		while ((pos = pth.Find("/../", pos)) > 0)
		{
			wchar_t ch = pth[pos-1];

			if (ch != '.' &&
#if LETHE_OS_WINDOWS
					ch != ':' &&
#endif
					ch != '/')
			{
				// ok, look for previous
				int tmp = pos-1;

				while (tmp >= 0 && (pth[tmp] != '/'
#if LETHE_OS_WINDOWS
									&& pth[tmp] != ':'
#endif
								   ))
					tmp--;

				pth.Erase(tmp+1, pos+3-tmp);
				pos = tmp;
				changed = 1;
			}

			pos++;
		}
	}
	while(changed);

	// we need to handle special cases here:
	// ends with /.
	while (pth.EndsWith("/."))
		pth.Erase(pth.GetLength()-2, 2);

	// ends with /.. and has previous part
	while (pth.EndsWith("/.."))
	{
		pos = pth.GetLength() - 4;

		if (pos >=0 && pth[pos] != '.' &&
#if LETHE_OS_WINDOWS
				pth[pos] != ':' &&
#endif
				pth[pos] != '/')
		{
			while (pos >= 0 && (pth[pos] != '/'
#if LETHE_OS_WINDOWS
								&& pth[pos] != ':'
#endif
							   ))
				pos--;

			pth.Erase(pos+1);
			continue;
		}

		break;
	}

#if LETHE_OS_WINDOWS
	// lowercase drive letter if present
	// fixes problems with drag and drop from TotalCommander
	if (pth.GetLength() > 2 && pth[1] == ':' && pth[2] == '/')
	{
		Int drvLetter = pth[0] | 32;

		if (drvLetter >= 'a' && drvLetter <= 'z' && pth[0] < 'a')
		{
			pth.Erase(0, 1);
			pth.Insert(0, (char)drvLetter);
		}
	}
#endif
}

// append path (string)
Path &Path::Append(const String &pth)
{
	if (finalPath.IsEmpty())
	{
		finalPath = pth;
		return Normalize();
	}

	String tmp = pth;
	Normalize(tmp);

	if (tmp.IsEmpty())
		return *this;

	if (finalPath[finalPath.GetLength()-1] != '/' && tmp[0] != '/')
		finalPath += '/';

	finalPath += tmp;
	return Normalize();
}

Path &Path::Normalize()
{
	Normalize(finalPath);
	return *this;
}

// get path string
const String &Path::Get() const
{
	return finalPath;
}

bool Path::IsAbsolute() const
{
	return finalPath.StartsWith('/')
#if LETHE_OS_WINDOWS
		   || finalPath.Find(":/") >= 0
#endif
		   ;
}

bool Path::IsRelative() const
{
	return !IsAbsolute();
}

Int Path::FindPrefix() const
{
#if LETHE_OS_WINDOWS

	for (Int i=0; i<finalPath.GetLength(); i++)
	{
		wchar_t ch = finalPath[i];

		if (ch == '.' || ch == '/')
			break;

		if (ch == ':')
			return i;
	}

#endif
	return -1;
}

bool Path::HasPrefix() const
{
	return FindPrefix() >= 0;
}

// get drive prefix, doesn't include :
String Path::GetPrefix() const
{
	int i = FindPrefix();
	return i > 0 ? String(finalPath.Ansi(), i) : String();
}

Path &Path::RemovePrefix()
{
	int i = FindPrefix();

	if (i >= 0)
	{
		finalPath.Erase(0, i+1);
		Normalize();
	}

	return *this;
}

// convert to absolute path
bool Path::MakeAbsolute(const String &refpth)
{
	if (LETHE_UNLIKELY(IsAbsolute()))
	{
		// already absolute path
		return 1;
	}

	Path tmp(refpth);
#if LETHE_OS_WINDOWS

	if (!tmp.HasPrefix() && HasPrefix())
	{
		// if there's no drv prefix in refpth and there is one in rel path, we should prepend it!
		String prefix = GetPrefix();
		prefix += ':';
		tmp.Make(prefix + tmp);
		RemovePrefix();
	}

#endif
	tmp.Append(Get());
	finalPath = tmp.finalPath;
	return 1;
}

// convert to relative path
bool Path::MakeRelative(const String &refpth)
{
	// TODO: handle case-insensitive OSes (i.e. Windows but also OSX which can work in both modes
	// In that case, we should use case-insensitive comparison
	// another problem is that for OSX case-sensitivity is determined by partition, so each drive can
	// be either case-sensitive or not => probably can't solve properly, keeping case sensitivity
	/*if ( LETHE_UNLIKELY(IsRelative())) {
		// already relative path
		return 1;
	}*/
	Path path(refpth);
	/*if ( !path.IsAbsolute() ) {
		// refpath NOT an absolute path => cannot make relative
		return 0;
	}*/
	int minsz = Min(path.finalPath.GetLength(), finalPath.GetLength());
	int i;

	for (i=0; i<minsz; i++)
	{
		if (path.finalPath[i] != finalPath[i])
			break;
	}

	int perfect = 0;

	if (i == path.finalPath.GetLength() && i < finalPath.GetLength() && finalPath[i] == '/')
	{
		// perfect submatch
		perfect = 1;
	}
	else
	{
		// now trace back until / is found
		while (i > 0 && path.finalPath[i-1] != '/')
			i--;
	}

	int pos = i;
	int count = 0;

	while ((pos = path.finalPath.Find('/', pos)) >= 0)
	{
		count++;
		pos++;
	}

	if (path.finalPath.GetLength() > i)
		count++;

	path.finalPath.Clear();
	finalPath.Erase(0, i+perfect);

	for (i=0; i<count; i++)
		path.finalPath += "../";

	path.finalPath += finalPath;
	finalPath = path.finalPath;
	path.finalPath.Clear();
	Normalize();
	return 1;
}

String Path::GetExt() const
{
	Int epos = GetExtPos();
	return epos >= 0 ? finalPath.Mid(epos+1) : String();
}

String Path::GetFilename() const
{
	Path tmp(*this), tmp2(*this);
	tmp.Append("..");
	tmp2.MakeRelative(tmp);
	return tmp2.Get();
}

Int Path::GetExtPos() const
{
	Int epos = -1;

	for (Int i=finalPath.GetLength()-1; i >= 0; i--)
	{
		Int ch = finalPath[i];

		if (ch == '/' || ch == '\\'
#if LETHE_OS_WINDOWS
				|| ch == ':'
#endif
		   )
			break;

		if (ch == '.' && i > 0 && finalPath[i-1] != '/' && finalPath[i-1] != '\\')
		{
			epos = i;
			break;
		}
	}

	return epos;
}

Path &Path::SetExt(const String &ext)
{
	const char *w = ext.Ansi() + ext.StartsWith('.');
	Int epos = GetExtPos();

	if (ext.IsEmpty())
	{
		if (epos >= 0)
			finalPath.Erase(epos);
	}
	else if (epos < 0)
		finalPath += String(".") + String(w);
	else
	{
		finalPath.Erase(epos+1);
		finalPath += w;
	}

	return Normalize();
}

}
