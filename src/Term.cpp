#include "Term.hpp"
#include <iostream>
#include <sstream>
#include <memory.h>
#include <assert.h>
#include "Stack.hpp"

namespace PROJECT {

class LocationTracker {
public:
    LocationTracker() : thisLine(0), thisColumn(0) { }

    void advance(const char ch)
    {
	if (ch == '\n') {
	    newLine();
	} else {
	    nextColumn();
	}
    }

    void newLine() { thisLine++; thisColumn = 0; }
    void nextColumn() { thisColumn++; }

    size_t getLine() const { return thisLine; }
    size_t getColumn() const { return thisColumn; }

private:
    size_t thisLine;
    size_t thisColumn;
};

class PrintState {
public:
    static const size_t MAX_INDENT_DEPTH = 100;

    PrintState(const PrintParam &param)
        : thisParam(param),
	  thisNeedNewLine(false),
	  thisColumn(param.getStartColumn()),
	  thisIndent(0)
    {
	for (size_t i = 0; i < MAX_INDENT_DEPTH; i++) {
	    thisIndentTable[i] = 0;
	}
    }

    const PrintParam & getParam() const { return thisParam; }

    size_t getColumn() const { return thisColumn; }

    void markColumn() {
	thisIndentTable[thisIndent] = thisColumn;
    }

    bool willWrap(size_t len) const {
	bool r = thisColumn + len >= thisParam.getEndColumn();
	return r;
    }

    size_t willWrapOnLength() const {
	if (thisColumn < thisParam.getEndColumn()) {
	    return thisParam.getEndColumn() - thisColumn;
	} else {
	    return 0;
	}
    }

    size_t getIndent() const { return thisIndent; }
    void incrementIndent() { thisIndent++; }
    void decrementIndent() { thisIndent--; }

    bool needNewLine() const {
	return thisNeedNewLine;
    }

    PrintState & addToColumn(size_t len)
    { thisColumn += len;
      if (thisColumn > thisParam.getEndColumn()) {
	  thisNeedNewLine = true;
      }
      return *this;
    }

    void resetToColumn(size_t col)
    {
	thisNeedNewLine = false;
	thisColumn = col;
    }

    void newLine(std::ostream &out)
    {
	out << "\n";
	resetToColumn(0);
	printIndent(out);
    }

    void printIndent(std::ostream &out)
    {
	size_t col = thisColumn;
	size_t start = thisParam.getStartColumn();
	while (col < start) {
	    col++;
	    out << " ";
	}

	size_t iw = thisParam.getIndentWidth();
	for (size_t i = 0; i < thisIndent; i++) {
	    size_t p = thisIndentTable[i] != 0 ? thisIndentTable[i] : col+iw;
	    for (size_t j = col; j < p; j++) {
		out << " ";
	    }
	    col = p;
	}
	thisColumn = col;
    }

private:
    const PrintParam &thisParam;
    bool thisNeedNewLine;
    size_t thisColumn;
    size_t thisIndent;
    size_t thisIndentTable[MAX_INDENT_DEPTH];
};

std::ostream & operator << (std::ostream &out, const ConstRef &ref)
{
    if (ref.getIndex() == 0) {
	out << "ConstRef(NULL)";
    } else {
	out << "ConstRef(" << ref.getIndex() << ")";
    }
    return out;
}

std::ostream & operator << (std::ostream &out, const ConstString &str)
{
    const Char *ch = str.getString();
    size_t n = str.getLength();
    size_t arity = str.getArity();
    for (size_t i = 0; i < n; i++) {
	out << (char)ch[i];
    }
    if (arity > 0) {
	out << "/";
	out << arity;
    }
    
    return out;
}

std::string ConstString::asStdString() const
{
    std::stringstream ss;
    const Char *ch = getString();
    for (size_t i = 0; i < thisLength; i++) {
	ss << (char)ch[i];
    }
    if (thisArity != 0) {
	ss << "/";
	ss << thisArity;
    }
    return ss.str();
}

const char ConstTable::RESERVED[] = { '[', ']', '(', ')', ',', '.', '\\', '\'', '\0' };
bool ConstTable::RESERVED_MAP[256];
bool ConstTable::theInitialized = false;

void ConstTable::escapeName(const char *name, char *escapedName)
{
    size_t i = 0, j = 0;
    bool useQuotes = false;
    if (name[0] >= 'A' && name[0] <= 'Z') {
	useQuotes = true;
    }
    for (i = 0; name[i] != '\0'; i++) {
	if (isReserved(name[i])) {
	    useQuotes = true;
	    break;
	}
    }
    if (useQuotes) {
	escapedName[j++] = '\'';
    }
    i = 0;
    while (name[i] != '\0') {
	if (name[i] == '\\' || name[i] == '\'') {
	    escapedName[j++] = '\\';
	}
	escapedName[j++] = name[i];
	i++;
    }
    if (useQuotes) {
	escapedName[j++] = '\'';
    }
    escapedName[j] = '\0';
}

ConstRef ConstTable::getConst(const char *name, size_t arity) const
{
    assert(strlen(name) < MAX_CONST_LENGTH);

    ConstRef cref = findConst(name, arity);
    if (cref == ConstRef()) {
	char escapedName[MAX_CONST_LENGTH];
	escapeName(name, escapedName);
	return const_cast<ConstTable *>(this)->addConst(escapedName, arity);
    } else {
	return cref;
    }
}

ConstRef ConstTable::getConstNoEscape(const char *name, size_t arity) const
{
    assert(strlen(name) < MAX_CONST_LENGTH);

    ConstRef cref = findConstNoEscape(name, arity);
    if (cref == ConstRef()) {
	return const_cast<ConstTable *>(this)->addConst(name, arity);
    } else {
	return cref;
    }
}

void ConstTable::getConstName(char *name, size_t ordinal) const
{
    static char ALPHABET1 [26] = 
	{ 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
	  'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
	  'U', 'V', 'W', 'X', 'Y', 'Z' };

    if (ordinal == 0) {
	name[0] = 'A';
	name[1] = 0;
	return;
    }

    size_t i = 0;

    size_t index;
    while (ordinal > 0) {
	index = ordinal % sizeof(ALPHABET1);
	name[i] = ALPHABET1[index];
	ordinal /= sizeof(ALPHABET1);
	i++;
    }

    size_t i2 = i / 2;
    for (size_t j = 0; j < i2; j++) {
	char ch = name[j];
	name[j] = name[i - j - 1];
	name[i - j - 1] = ch;
    }
    if (i > 1) {
	name[0]--; 
    }
    name[i] = '\0';
}

ConstRef ConstTable::getConst(size_t ordinal) const
{
    char str[16];
    getConstName(str, ordinal);
    return getConstNoEscape(str, 0);
}

ConstRef ConstTable::addConst(const char *name, size_t arity)
{
    ConstString str = thisPool.addString(name, arity);
    NativeType *cr = allocate(1);
    NativeType rel = static_cast<NativeType>(thisPool.toRelativePointer(str));
    *cr = rel;
    ConstRef constRef(toRelative(cr));
    thisIndexing.add(str, constRef);
    return constRef;
}

ConstRef ConstTable::findConst(const char *name, size_t arity) const
{
    assert(strlen(name) < MAX_CONST_LENGTH);

    char escapedName[MAX_CONST_LENGTH];
    escapeName(name, escapedName);

    size_t n = strlen(escapedName);
    Char chs[MAX_CONST_LENGTH];
    for (size_t i = 0; i <= n; i++) {
	chs[i] = (Char)escapedName[i];
    }
    ConstString str(chs, n, arity);
    return thisIndexing.find(str);
}

ConstRef ConstTable::findConstNoEscape(const char *name, size_t arity) const
{
    assert(strlen(name) < MAX_CONST_LENGTH);

    size_t n = strlen(name);
    Char chs[MAX_CONST_LENGTH];
    for (size_t i = 0; i <= n; i++) {
	chs[i] = (Char)name[i];
    }
    ConstString str(chs, n, arity);
    return thisIndexing.find(str);
}

void ConstTable::print(std::ostream &out) const
{
    size_t n = getSize();
    for (size_t i = 1; i <= n; i++) {
	ConstRef cr(i);
	NativeType *c = toAbsolute(cr.getIndex());
	NativeType name = c[0];
	Char *data = thisPool.toAbsolute(name);
	ConstString str(&data[2], (size_t)data[0], (size_t)data[1]);
	out << "[" << i << "]: " << str;
	out << "\n";
    }
}

void ConstTable::printConst(std::ostream &out, const ConstRef &ref) const
{
    NativeType *c = toAbsolute(ref.getIndex());
    NativeType name = c[0];
    Char *data = thisPool.toAbsolute(name);
    ConstString str(&data[2], (size_t)data[0], (size_t)data[1]);
    out << str;
}

size_t ConstTable::getConstArity(const ConstRef &ref) const
{
    NativeType *c = toAbsolute(ref.getIndex());
    NativeType name = c[0];
    Char *data = thisPool.toAbsolute(name);
    return (size_t)data[1];
}

void ConstTable::printConstNoArity(std::ostream &out, const ConstRef &ref) const
{
    NativeType *c = toAbsolute(ref.getIndex());
    NativeType name = c[0];
    Char *data = thisPool.toAbsolute(name);
    ConstString str(&data[2], (size_t)data[0], 0);
    out << str;
}

size_t ConstTable::getConstLength(const ConstRef &ref) const
{
    NativeType *c = toAbsolute(ref.getIndex());
    NativeType name = c[0];
    Char *data = thisPool.toAbsolute(name);
    return static_cast<size_t>(data[0]);
}

ConstString ConstTable::getConstNameNoArity(const ConstRef &ref) const
{
    NativeType *c = toAbsolute(ref.getIndex());
    NativeType name = c[0];
    Char *data = thisPool.toAbsolute(name);
    ConstString str(&data[2], (size_t)data[0], 0);
    return str;
}

ConstRef Heap::getConst(const char *name, size_t arity) const
{
    return thisConstTable.getConst(name, arity);
}

ConstRef Heap::getConst(size_t ordinal) const
{
    return thisConstTable.getConst(ordinal);
}

void Heap::printTag(std::ostream &out, Cell cell) const
{
    switch (cell.getTag()) {
    case Cell::REF: out << "REF"; break;
    case Cell::CON: out << "CON"; break;
    case Cell::STR: out << "STR"; break;
    case Cell::EXT:
	switch (cell.getExtTag()) {
	case Cell::EXT_INT32: out << "INT32"; break;
	case Cell::EXT_INT64: out << "INT64"; break;
	case Cell::EXT_FLOAT: out << "FLOAT"; break;
	case Cell::EXT_DOUBLE: out << "DOUBLE"; break;
	case Cell::EXT_INT128: out << "INT128"; break;
	case Cell::EXT_ARRAY: out << "ARRAY"; break;
	default: out << "???"; break;
	}
    }
}

void Heap::printConst(std::ostream &out, Cell cell) const
{
    ConstRef cref = cell.toConstRef();
    thisConstTable.printConst(out, cref);
}

void Heap::printCell(std::ostream &out, Cell cell) const
{
    printTag(out, cell);
    out << ":";
    switch (cell.getTag()) {
    case Cell::REF: out << cell.getValue(); break;
    case Cell::CON: printConst(out, cell); break;
    case Cell::STR: out << cell.getValue(); break;
    case Cell::EXT: out << "???"; break;
    }
}

void Heap::printRaw(std::ostream &out) const
{
    printRaw(out, first(), top()-1);
}

void Heap::printRaw(std::ostream &out, HeapRef from, HeapRef to) const
{
    for (HeapRef i = from; i <= to; i++) {
	out << "[" << i.getIndex() << "]: ";
	Cell cell = getCell(i);
	printCell(out, cell);
	out << "\n";
    }
}

std::string Heap::toRawString() const
{
    return toRawString(first(), top()-1);
}

std::string Heap::toRawString(HeapRef from, HeapRef to) const
{
    std::stringstream ss;
    ss << "[";
    for (HeapRef i = from; i <= to; i++) {
	if (i != from) {
	    ss << ", ";
	}
	Cell cell = getCell(i);
	printCell(ss, cell);
    }
    ss << "]";
    return ss.str();
}

size_t Heap::getStringLength(Cell cell, size_t maximum) const
{
    size_t current = thisStack.getSize();

    thisStack.push(cell);

    size_t len = 0;

    while (current != thisStack.getSize()) {
	Cell cell = deref(thisStack.pop());

	if (len >= maximum) {
	    thisStack.trim(current);
	    return maximum;
	}

	switch (cell.getTag()) {
	case Cell::CON:
	    len += thisConstTable.getConstLength(cell.toConstRef()); break;
	case Cell::STR:
	    len += getStringLengthForStruct(cell); break;
	case Cell::REF:
	    len += getStringLengthForRef(cell); break;
        case Cell::EXT:
	    switch (cell.getExtTag()) {
	    case Cell::EXT_END:
		len++;
		break;
	    case Cell::EXT_COMMA:
		len += 2;
		break;
	    default:
		break;
	    }
	    break;
	}
    }
    return len;
}

size_t Heap::getStringLengthForStruct(Cell cell) const
{
    size_t len = 0;
    HeapRef href = toHeapRef(cell);
    ConstRef cref = const_cast<Heap *>(this)->pushArgs(href);
    len += thisConstTable.getConstLength(cref);
    size_t arity = thisConstTable.getConstArity(cref);
    if (arity > 0) {
	len++;
    }
    return len;
}

size_t Heap::getStringLengthForRef(Cell cell) const
{
    const ConstRef *pcref = thisNameMap.get(cell);
    if (pcref == NULL) {
	ConstRef cref = thisConstTable.getConst(thisNameMap.numEntries());
	thisNameMap.put(cell, cref);
	return thisConstTable.getConstLength(cref);
    } else {
	return thisConstTable.getConstLength(*pcref);
    }
}

std::string Heap::toString(HeapRef href) const
{
    std::stringstream ss;
    print(ss, href);
    return ss.str();
}

ConstRef Heap::pushArgs(HeapRef ref)
{
    Cell cell = getCell(ref);
    ConstRef cref = cell.toConstRef();
    size_t arity = thisConstTable.getConstArity(cref);
    if (arity > 0) {
	thisStack.push(Cell(Cell::EXT_END,0));
	Cell listComma(Cell::EXT_COMMA,0);
	for (size_t i = 0; i < arity; i++) {
	    if (i > 0) {
		thisStack.push(listComma);
	    }
	    Cell arg = getCell(ref+arity-i);
	    thisStack.push(arg);
	}
    }
    return cref;
}

ConstRef Heap::getRefName(Cell cell) const
{
    const ConstRef *pcref = thisNameMap.get(cell);
    if (pcref == NULL) {
	ConstRef cref = thisConstTable.getConst(thisNameMap.numEntries());
	thisNameMap.put(cell, cref);
	return cref;
    }
    return *pcref;
}

void Heap::printIndent(std::ostream &out, PrintState &state) const
{
    if (state.needNewLine()) {
	out << "\n";
	state.resetToColumn(0);
	state.printIndent(out);
    }
}

void Heap::print(std::ostream &out, HeapRef href, const PrintParam &param) const
{
    PrintState state(param);

    size_t current = thisStack.getSize();

    Cell cell = getCell(href);

    thisStack.push(cell);

    while (current != thisStack.getSize()) {
	Cell cell = deref(thisStack.pop());

	switch (cell.getTag()) {
	case Cell::CON: {
	    ConstRef cref = cell.toConstRef();
	    printIndent(out, state.addToColumn(thisConstTable.getConstLength(cref)));
	    thisConstTable.printConstNoArity(out, cref);
	    break;
  	    }
	case Cell::STR:
	    {
	    if (state.getIndent() > 0 &&
		state.willWrap(getStringLength(cell,
					       state.willWrapOnLength()))) {
		
		state.newLine(out);
	    }
 	    HeapRef href = toHeapRef(cell);
	    ConstRef cref = const_cast<Heap *>(this)->pushArgs(href);
	    size_t arity = thisConstTable.getConstArity(cref);
	    printIndent(out, state.addToColumn(
	       thisConstTable.getConstLength(cref)
	       + ((arity > 0) ? 1 : 0)));
	    thisConstTable.printConstNoArity(out, cref);
	    if (arity > 0) {
		out << "(";
		state.markColumn();
		state.incrementIndent();
	    }
	    break;
	    }
	case Cell::REF: 
	    {
	     ConstRef cref = getRefName(cell);
	     printIndent(out, state.addToColumn(
		 thisConstTable.getConstLength(cref)));
	     thisConstTable.printConstNoArity(out, cref);
             break;
    	    }
        case Cell::EXT:
	    switch (cell.getExtTag()) {
	    case Cell::EXT_END:
		printIndent(out, state.addToColumn(1));
		out << ")";
		state.decrementIndent();
		break;
	    case Cell::EXT_COMMA:
		printIndent(out, state.addToColumn(2));
		out << ", ";
		break;
	    default:
		printIndent(out, state.addToColumn(3));
		out << "???";
		break;
	    }
	    break;
	}
    }
}

void Heap::printStatus(std::ostream &out) const
{
    out << "Heap{Size=" << getSize() << ",StackSize=" << thisStack.getSize() << ",RootsSize=" << thisRoots.numEntries() << ",MaxRootsSize=" << thisMaxNumRoots << "}";
}

void Heap::printRoots(std::ostream &out) const
{
    thisRoots.print(out);
}

char Heap::parseSkipWhite(std::istream &in, LocationTracker &loc)
{
    char ch;
    do {
	in >> ch;
	loc.advance(ch);
    } while (isspace(ch));
    return ch;
}

HeapRef Heap::parseConst(std::istream &in, char &ch, LocationTracker &loc)
{
    char constName[ConstTable::MAX_CONST_LENGTH];
    size_t i = 0;

    bool useQuotes = false;
    if (ch == '\'') {
	useQuotes = true;
    } else {
	if (thisConstTable.isReserved(ch)) {
	    return parseError(loc);
	}
    }
    constName[i++] = ch;
    bool cont = true;
    
    do {
	in >> ch;
	constName[i++] = ch;
	if (ch == '\\') {
	    if (useQuotes) {
		in >> ch;
		constName[i++] = ch;
	    } else {
		cont = false;
	    }
	} else {
	    if (useQuotes) {
		if (ch == '\'') {
		    cont = false;
		}
		constName[i++] = ch;
	    } else {
		if (thisConstTable.isReserved(ch)) {
		    cont = false;
		}
	    }
	}
    } while (cont);
    constName[i] = '\0';

    ConstRef cref = getConst(constName, 0);
    return newCon(cref);
}

HeapRef Heap::parseError(LocationTracker &loc)
{
    (void)loc;
    ConstRef cref = thisConstTable.getConstNoEscape("$parseError", 0);
    return newCon(cref);
}

HeapRef Heap::parse(std::istream &in, LocationTracker &loc)
{
    char ch = parseSkipWhite(in, loc);
    HeapRef href = parseConst(in, ch, loc);

    return href;
}

}
