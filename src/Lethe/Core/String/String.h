#pragma once

#include "../Sys/Assert.h"
#include "../Sys/NoCopy.h"
#include "../Collect/Array.h"
#include "../Thread/Atomic.h"
#include "../Memory/Memory.h"
#include "../Memory/AlignedAlloc.h"
#include "CharConv.h"

namespace lethe
{

class Stream;
class StringRef;
class Path;

struct LETHE_API StringData
{
	int capacity;			// not including trailing zero
	int length;
	AtomicUInt hash;		// cached hash (initially zero)
	AtomicInt refCount;
	// length in characters, not UTF-8 bytes (cached because of editor and long lines)
	// 0 = unknown/empty
	int charLength;
	char data[1];

	static StringData *Alloc(int n);
	StringData *Clone();
	inline void AddRef();
	inline void Release();
};

// wide char buffer for temporary conversions from new UTF8 string
typedef StackArray<wchar_t, 2048> WideCharBuffer;

// copy-on-write string class.
// String itself is NOT thread-safe, but StringData refcounting almost is
// (almost = just like shared ptrs)
class LETHE_API String
{
public:
	// default constructor.
	String();
	// construct from C string.
	String(const char *str);
	// construct from wide-char C string.
	String(const wchar_t *str);
	// copy constructor
	String(const String &str);
	String(const StringRef &sr);
	// allow construction from path!
	String(const Path &pth);
	~String();

	// char iterator
	struct CharIterator
	{
		const Byte *ptr;
		const Byte *top;
		// decoded unicode character
		int ch;
		int charPos;
		int bytePos;
		// char size in bytes
		int byteSize;

		inline bool operator ==(const CharIterator &o) const;
		inline bool operator !=(const CharIterator &o) const;

		inline const CharIterator &operator *() const
		{
			return *this;
		}

		CharIterator &operator ++();
		inline CharIterator &operator ++(int);
	};

	CharIterator Begin() const;
	inline CharIterator begin() const
	{
		return Begin();
	}
	inline CharIterator cbegin() const
	{
		return Begin();
	}

	inline CharIterator End() const
	{
		return endIterator;
	}

	inline CharIterator end() const
	{
		return End();
	}
	inline CharIterator cend() const
	{
		return End();
	}

	// explicit ctors

	// construct from pointer and length (multibyte).
	explicit String(const char *str, int len);
	// construct from start and end pointers (multibyte).
	explicit String(const char *str, const char *strEnd);
	// construct from start and end pointers (wide).
	explicit String(const wchar_t *str, const wchar_t *strEnd);
	// construct from pointer and length (wide).
	explicit String(const wchar_t *str, int len);

	// conversion operators/methods

	// returns ansi (multibyte UTF8) string
	const char *Ansi() const;

	// indexing operator (read only!)
	char operator[](int index) const;

	// convert to wide char buffer
	// returns wbuf.GetData()
	const wchar_t *ToWide(Array<wchar_t> &wbuf) const;

	// relational operators (use case-sensitive comparison, no collate!)

	// alphabetically less than (C string)
	bool operator <(const char *str) const;
	// alphabetically less than.
	bool operator <(const String &str) const;

	// alphabetically less/equal than (C string)
	bool operator <=(const char *str) const;
	// alphabetically less/equal than
	bool operator <=(const String &str) const;

	// alphabetically greater than (C string)
	bool operator >(const char *str) const;
	// alphabetically greater than
	bool operator >(const String &str) const;

	// alphabetically greater/equal than (C string)
	bool operator >=(const char *str) const;
	// alphabetically greater/equal than
	bool operator >=(const String &str) const;

	// alphabetically equal to (C string)
	bool operator ==(const char *str) const;
	// alphabetically equal to
	bool operator ==(const String &str) const;
	bool operator ==(const StringRef &sr) const;

	// alphabetically unequal to (C string)
	bool operator !=(const char *str) const;
	// Alphabetically unequal to
	bool operator !=(const String &str) const;
	bool operator !=(const StringRef &str) const;

	// assignment operators

	// assign character
	String &operator =(int w);
	// assign C string
	String &operator =(const char *c);
	// assign wide C string
	String &operator =(const wchar_t *c);
	// assign string
	String &operator =(const String &str);
	String &operator =(const StringRef &str);

	// other operators

	// add character (in-place)
	String &operator +=(int w);
	// add C string (in-place)
	String &operator +=(const char *c);
	// add string (in-place)
	String &operator +=(const String &str);

	// append ref string
	String &Append(const char *c, const char *cend);

	// add character
	String operator +(int w) const;
	// add C string
	String operator +(const char *str) const;
	// add string
	String operator +(const String &str) const;

	// add character and string
	friend String operator +(wchar_t w, const String &str1);
	// add C string and string
	friend String operator +(const char *str0, const String &str1);

	// methods

	// clear string
	String &Clear();

	// reset string, freeing allocated memory
	String &Reset();

	// make unique copy of string data (if necessary)
	String &CloneData();

	// is unique? (=only a single reference)
	// note: this is not thread-safe!
	bool IsUnique() const;

	// shrink string data
	String &Shrink();

	// returns true if string is empty
	bool IsEmpty() const;

	// returns string length in bytes (not unicode!)
	int GetLength() const;

	// returns string length in unicode chars
	int GetWideLength() const;

	// is multi-byte string?
	// if true, string contains characters with 2+ bytes
	bool IsMultiByte() const;

	// get byte index for char
	int GetByteIndex(int charIndex) const;

	// get char index for byte
	int GetCharIndex(int byteIndex) const;

	// get char size in bytes at byte index
	int GetCharSizeAt(int byteIndex) const;

	// decode char at byte index
	int DecodeCharAt(int byteIndex) const;

	// in-place format (C string)
	String &Format(const char *fmt, ...);

	// static versions of Format
	// FIXME: better name?!
	static String Printf(const char *fmt, ...);

	// compare to ansi/multibyte string
	// character by character, no collation
	inline int Compare(const char *str, const char *strEnd = nullptr) const;

	// compare to string
	// character by character, no collation
	inline int Compare(const String &str) const;

	// case insensitive compare
	inline int CompareNoCase(const char *str, const char *strEnd = nullptr) const;

	// case insensitive compare
	inline int CompareNoCase(const String &str) const;

	// convert to uppercase (inplace)
	String &ToUpper();
	// convert to lowercase (inplace)
	String &ToLower();
	// convert to capital (inplace)
	String &ToCapital();
	// reverse characters in string (inplace)
	String &Reverse();

	// simplified find/reverseFind

	// find character inside string
	// returns character index or -1 if not found
	int Find(wchar_t ch, int pos = 0) const;
	// find C string inside string
	// returns character index (startpos) or -1 if not found
	int Find(const char *str, int pos = 0) const;
	// find string inside string
	// returns character index (startpos) or -1 if not found
	int Find(const String &str, int pos = 0) const;

	// find character inside string (in reverse order)
	// note: ANSI only
	// returns character index or -1 if not found
	int ReverseFind(wchar_t ch) const;

	int ReverseFind(const String &str, int pos = 0) const;

	// find character inside string
	// returns character index or -1 if not found
	int FindOneOf(wchar_t ch, int pos = 0) const;
	// find C character of set inside string
	// returns character index (startpos) or -1 if not found
	int FindOneOf(const char *chset, int pos = 0) const;
	// find character of set inside string
	// returns character index (startpos) or -1 if not found
	int FindOneOf(const String &chset, int pos = 0) const;

	// erase characters.
	// returns new length
	int Erase(int pos, int count = -1);

	// copy substring from the left.
	String Left(int count) const;
	// copy substring from the right.
	String Right(int count) const;
	// copy substring (=middle).
	String Mid(int pos, int count = -1) const;

	// remove character from string.
	// returns number of characters removed
	int Remove(wchar_t ch);

	// insert character at position.
	// returns new length
	int Insert(int pos, wchar_t ch);
	// insert string at position.
	// returns new length
	int Insert(int pos, const char *str);
	// insert string at position.
	// returns new length
	int Insert(int pos, const String &str);

	// replace character.
	// returns number of occurrences
	int Replace(wchar_t oldc, wchar_t newc);
	// replace strings.
	// returns number of occurrences (slow)
	int Replace(const char *oldstr, const char *newstr);
	// replace strings.
	// returns number of occurrences
	int Replace(const String &oldstr, const String &newstr);

	// trim leading/trailing whitespace from an existing string (in-place)
	String &Trim(void);
	// trim leading whitespace from an existing string (in-place)
	String &TrimLeft(void);
	// trim trailing whitespace from an existing string (in-place).
	String &TrimRight(void);

	// starts with/endswith
	bool StartsWith(const wchar_t ch) const;
	bool StartsWith(const char *str) const;
	bool StartsWith(const char *str, int len) const;
	bool StartsWith(const char *str, const char *strEnd) const;
	bool StartsWith(const String &str) const;
	bool EndsWith(const wchar_t ch) const;
	bool EndsWith(const char *str) const;
	bool EndsWith(const char *str, int len) const;
	bool EndsWith(const char *str, const char *strEnd) const;
	bool EndsWith(const String &str) const;

	// tokenize (note: charset ANSI only!)
	// note: behavior has been changed so that empty strings are kept;
	// termination condition is thus to check if pos for <0!
	// ch separator
	// returns new string token
	String Tokenize(const wchar_t ch, int &pos) const;
	// tokenize.
	// chset C string separators
	// returns new string token
	String Tokenize(const char *chset, int &pos) const;
	// tokenize
	// chset string separators
	// returns new string token
	String Tokenize(const String &chset, int &pos) const;

	// split string (note: charset ANSI only!)
	// ch separator
	// returns vector of strings
	Array<String> Split(const int ch, bool keepEmpty = 0) const;
	// split string
	// chset C string separators
	// returns vector of strings
	Array<String> Split(const char *chset, bool keepEmpty = 0) const;
	// split string
	// chset string separators
	// returns vector of strings
	Array<String> Split(const String &chset, bool keepEmpty = 0) const;

	// convert to escaped string
	String Escape() const;
	// unescape escaped string
	String Unescape() const;

	// load/save from binary stream
	bool Load(Stream &s);
	bool Save(Stream &s) const;
	bool Serialize(Stream &s);

	// load/save from text stream
	bool LoadText(Stream &s);
	bool SaveText(Stream &s) const;

	// init from specific multibyte encoded string
	String &FromEncoding(CharConv::Encoding enc, const char *str, const char *strEnd = 0);
	// convert ANSI representation to specific encoding
	// reset when string gets modified!
	String &ToEncoding(CharConv::Encoding enc);

	inline void SwapWith(String &o)
	{
		Swap(data, o.data);
	}
	friend inline int Compare(const String &x, const String &y)
	{
		return x.Compare(y);
	}
	friend UInt LETHE_API Hash(const String &s);

	// returns precomputed hash if available
	// 0 => not computed
	UInt GetStoredHash() const;

	// convert to int, no error checking
	Int AsInt() const;

protected:
	friend class StringRef;

	mutable StringData *data;

	// whenever contents change; invalidates derived data like hash and char length
	String &ContChange();

	static CharIterator endIterator;

	void Init(const char *str, const char *strEnd, CharConv::Encoding encoding = CharConv::encDefault);
	void Init(const wchar_t *str, const wchar_t *strEnd);

	int Comp(const char *str, const char *strEnd = 0) const;
	int Comp(const String &str) const;

	// compare no case
	int CompNC(const char *str, const char *strEnd = 0) const;
	int CompNC(const String &str) const;

	int Find(const char *str, const char *strEnd, int pos) const;
	// note: only supports ansi charset!
	int FindSet(const char *str, const char *strEnd, int pos) const;

	String Tokenize(const char *str, const char *strEnd, int &pos) const;

	int Replace(
		const char *ostr, const char *ostrend,
		const char *nstr, const char *nstrend
	);

	String &Insert(const char *str, const char *strEnd, int pos);

	String &AppendData(int len, int slen, const char *str);
};

#include "Inline/String.inl"

}
