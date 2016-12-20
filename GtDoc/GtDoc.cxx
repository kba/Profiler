#include <algorithm>
#include <stdexcept>
#include <iterator>
#include <unicode/uchar.h>
#include <fstream>
#include "Utils/Utils.h"
#include "GtDoc.h"

using namespace OCRCorrection;

////////////////////////////////////////////////////////////////////////////////
EditOperation::EditOperation(wchar_t c)
	: type_(fromChar(c))
{
}

////////////////////////////////////////////////////////////////////////////////
EditOperation::Type
EditOperation::fromChar(wchar_t c)
{
	switch (c) {
	case L'|': return Type::None;
	case L'#': return Type::Substitution;
	case L'-': return Type::Deletion;
	case L'+': return Type::Insertion;
	default:
		   throw std::runtime_error("(EditOperation) Invalid character");
	}
}

////////////////////////////////////////////////////////////////////////////////
GtLine::GtLine(const std::string& file, const std::wstring& gt,
		const std::wstring& ops, const std::wstring& ocr)
	: file_(file)
	, chars_(gt.size())
{
	if (gt.size() != ops.size() or gt.size() != ocr.size()) {
		throw std::runtime_error("(GtLine) gt, ops and ocr do "
				"not all have the same length");
	}
	chars_.reserve(gt.size());
	for (size_t i = 0; i < gt.size(); ++i) {
		chars_[i] = GtChar(gt[i], ocr[i], ops[i]);
	}
}

////////////////////////////////////////////////////////////////////////////////
void
GtLine::parse(Document& doc) const
{
	static const wchar_t* boolean[] = {L"false", L"true"};
	static std::wstring gt, ocr;
	each_token_ocr([&](range r) {
		ocr.clear();
		std::for_each(r.b, r.e, [&](const GtChar& c) {
			if (c.copy_ocr())
				ocr.push_back(c.ocr);
		});

		gt.clear();
		auto bb = bot_gt(begin(), r.b);
		auto ee = eot_gt(r.b, end());
		std::for_each(bb, ee, [&](const GtChar& c) {
			if (c.copy_gt())
				gt.push_back(c.gt);
		});

		const auto idx = doc.getNrOfTokens();
		doc.pushBackToken(ocr, r.normal());
		doc.at(idx).metadata()["groundtruth"] = gt;
		doc.at(idx).metadata()["groundtruth-lc"] = Utils::tolower(gt);

		// std::wcerr << "Adding token: " << doc.at(idx).getWOCR_lc() << "\n"
		// 	   << "normal:       " << doc.at(idx).isNormal() << "\n"
		// 	   << "eval:         " << doc.at(idx).metadata()["eval"] << "\n"
		// 	   << "gt:           " << doc.at(idx).metadata()["groundtruth-lc"] << "\n";
		// if (doc.at(idx).has_metadata("correction")) {
		// 	std::wcerr << "cor:          "
		// 		   << doc.at(idx).metadata()["correction-lc"] << "\n";
		// }
	});
}

////////////////////////////////////////////////////////////////////////////////
bool
GtToken::normal() const noexcept
{
	return std::all_of(b, e, [](const GtChar& c) {return c.is_normal();});
}

////////////////////////////////////////////////////////////////////////////////
void
GtDoc::load(const std::string& file)
{
	std::wifstream is(file);
	if (not is.good())
		throw std::system_error(errno, std::system_category(), file);
	is >> *this;
	is.close();
}

////////////////////////////////////////////////////////////////////////////////
void
GtDoc::parse(Document& document) const
{
	document.clear();
	for (const auto& line: lines_) {
		line.parse(document);
	}
}

////////////////////////////////////////////////////////////////////////////////
void
GtDoc::parse(const std::string& file, Document& document)
{
	load(file);
	parse(document);
}

////////////////////////////////////////////////////////////////////////////////
static std::wstring&
remove_dottet_circles(std::wstring& str)
{
	auto e = std::remove(begin(str), end(str), 0x25cc /* dottet circle '◌' */);
	str.erase(e, end(str));
	return str;
}

////////////////////////////////////////////////////////////////////////////////
std::wistream&
OCRCorrection::operator>>(std::wistream& is, GtLine& line)
{
	std::wstring file;
	if (not std::getline(is, file))
		return is;
	std::wstring gt;
	if (not std::getline(is, gt))
		return is;
	std::wstring trace(gt.size(), 0);
	if (not std::getline(is, trace))
		return is;
	std::wstring ocr(gt.size(), 0);
	if (not std::getline(is, ocr))
		return is;
	line = GtLine(Utils::utf8(file), remove_dottet_circles(gt),
			trace, remove_dottet_circles(ocr));
	return std::getline(is, file); // read empty line;
}

////////////////////////////////////////////////////////////////////////////////
std::wistream&
OCRCorrection::operator>>(std::wistream& is, GtDoc& doc)
{
	doc.clear();
	GtLine line;
	while (is >> line)
		doc.lines().push_back(line);
	return is;
}
