/* scanner: simple string parsing */

#ifndef SCAN_H_
#define SCAN_H_

#include <exception>
#include <cstdlib>
#include <string>
#include <cerrno>

namespace scn {

struct scan_error : public std::runtime_error {
	scan_error(const std::string& description, const std::string& dump_pos) :
		std::runtime_error(description + '\n' + dump_pos) {}
};

class scanner {
public:
	scanner(const char *str)
	{
		if ((m_fst = m_cur = m_checkpoint = strdup(str)) == nullptr)
			throw std::bad_alloc();
	}
	scanner(const std::string& str) : scanner(str.c_str()) {}
	scanner(const scanner& other) = delete;
	scanner& operator =(const scanner& other) = delete;
	~scanner() { free(m_fst); }

	void restart() { m_cur = m_fst; }

	scanner& operator >>(short& val)
		{ val = scan_integer_and_cast<short>("expected integer (with type=short)", create_out_of_range_msg<short>("short")); return *this; }
	scanner& operator >>(int& val)
		{ val = scan_integer_and_cast<int>("expected integer (with type=int)", create_out_of_range_msg<int>("int")); return *this; }
	scanner& operator >>(long& val)
		{ val = scan_integer<long>(strtol, "expected integer (with type=long)", create_out_of_range_msg<long>("long")); return *this; }
	scanner& operator >>(long long& val)
		{ val = scan_integer<long long>(strtoll, "expected integer (with type=long long", create_out_of_range_msg<long long>("long long")); return *this; }	
	scanner& operator >>(float& val)
		{ val = scan_variable<float>(strtof, "expected floating point (with type=float)", create_out_of_range_msg<float>("float")); return *this; }
	scanner& operator >>(double& val)
		{ val = scan_variable<double>(strtod, "expected floating point (with type=double)", create_out_of_range_msg<double>("double")); return *this; }
	scanner& operator >>(long double& val)
		{ val = scan_variable<long double>(strtold, "expected floating point (with type=long double)", create_out_of_range_msg<long double>("long double")); return *this; }	

	/* scanning regex */
	scanner& operator >>(const char *s)
	{
		while (*s) {
			char c = *s;
			if (c == '\\')
				switch (*++s) {
				case 'n':  c = '\n';
				case 't':  c = '\t';
				case '\\': c = '\\';
				case '0':  c = '\0';
				case '*':  c = '*';
				case '?':  c = '?';
				case '+':  c = '+';
				default:
					m_cur = m_checkpoint;
					throw std::invalid_argument("unknown escape sequence");
				}
			if (*(s + 1) == '*') {
				while (*m_cur == c)			
					++m_cur;
				s += 2;
				continue;
			}
			if (*(s + 1) == '?') {
				if (*m_cur == c)
					++m_cur;
				s += 2;
				continue;
			}
			if (c != *m_cur) {
				std::string description;
				if (isprint(c)) {
					description = "expected symbol '";
					description.push_back(c);
					description.push_back('\'');
				} else if (c == '\n') {
					description = "expected newline";
				} else if (c == '\t') {
					description = "expected tab";
				} else {
					description = "expected symbol with code=";
					description += std::to_string((int) c);
				}
				throw_scan_error(description, m_cur);
			}
			++s, ++m_cur;
		}
		return *this;
	}

	scanner& operator >>(scanner& (*func)(scanner&)) { return func(*this); }

	/* end of text manipulator */
	static scanner& end_of_text(scanner& scan)
	{
		if (*scan.m_cur != '\0')
			scan.throw_scan_error("symbols are not expected here", scan.m_cur);
		return scan;
	}

	/*  Checkpoint manipulator. If error occurs, position will be returned
	 * to the position, when manipulator was inserted */
	static scanner& set_checkpoint(scanner& scan)
		{ scan.m_checkpoint = scan.m_cur; return scan; }
private:
	char *m_fst, *m_cur;
	char *m_checkpoint;

	/*  scans integer/floating variable using strtoT and returns result
	 *  Func signature: T strtoT(const char *, char **) - functions
	 * strtof, strtod, strtold and strtol, strtoll with binded last argument */
	template <class T, class Func>
	T scan_variable(Func strtoT, const std::string& invalid_arg_msg, const std::string& out_of_range_msg)
	{
		char *new_cur;

		if (isspace(*m_cur))
			throw_scan_error(invalid_arg_msg, m_cur);
		errno = 0;
		T val = strtoT(m_cur, &new_cur);
		if (m_cur == new_cur)
			throw_scan_error(invalid_arg_msg, m_cur);
		if (errno)
			throw_scan_error(out_of_range_msg, m_cur);
		m_cur = new_cur;
		return val;	
	}

	/* wrapper for scan_variable for integers */
	template <class T, class Func>
	T scan_integer(Func strtoT, const std::string& invalid_arg_msg, const std::string& out_of_range_msg)
	{
		return scan_variable<T>(
			[&](const char *str, char **str_end) { return strtoT(str, str_end, 0); },
			invalid_arg_msg,
			out_of_range_msg);
	}

	/* trying to convert long to smaller integer type */
	template <class To, class From>
	To narrow_cast(const From& from)
	{
		if (from != static_cast<From>(static_cast<To>(from)))
			throw std::invalid_argument("narrow_cast failure");
		return static_cast<To>(from);
	}

	/* scanning integers, which are less than long - i.e. int, short */
	template <class T>
	T scan_integer_and_cast(const std::string& invalid_arg_msg, const std::string& out_of_range_msg)
	{
		char *prev_cur = m_cur;
		long long_val = scan_integer<long>(strtol, invalid_arg_msg, out_of_range_msg);
		try { return narrow_cast<T>(long_val); }
		catch (std::invalid_argument&) {
			throw_scan_error(out_of_range_msg, prev_cur);
		}
	}

	/* string with out of range error description */
	template <class T>
	std::string create_out_of_range_msg(const char *type)
		{ return std::string("value is too big (expected value type = ") + type + ", sizeof(" + type + ") = " + std::to_string(sizeof(T)) + ")"; }

	std::string dump_pos(const char *pos)
	{
		std::string res;
		const char *line_begin = pos, *line_end = pos;
		while (line_begin > m_fst && *--line_begin != '\n') // seek begin
			;
		while (*line_end != '\0' && *line_end != '\n')
			++line_end;
		res.insert(res.end(), line_begin, line_end);
		res.push_back('\n');
		res.insert(res.end(), pos - line_begin, '~');
		res.push_back('^');
		if (line_end > pos)
			res.insert(res.end(), line_end - pos - 1, '~');
		return res;
	}

	/* Changes current position to checkpoint position and throws error
	 * Position dump is printed for err_pos */
	void throw_scan_error [[ noreturn ]] (const std::string& description, char *err_pos)
	{
		m_cur = m_checkpoint;
		throw scan_error(description, dump_pos(err_pos));
	}
};

/* scanner manipulators */
inline const auto skip_spaces = " *";
inline const auto end_of_text = scanner::end_of_text;
inline const auto set_checkpoint = scanner::set_checkpoint;

}; // scn namespace end

#endif // SCAN_H_