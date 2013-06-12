#include "importer.hpp"
#include <stdint.h>
#include <limits>
#include <vector>
#include <assert.h>
#include <sstream>
using namespace std;
using namespace tightdb;

const size_t print_width = 25;

string set_width(string s, size_t w) {
	if(s.size() > w)
		s = s.substr(0, w - 3) + "...";
	else
		s = s + string(w - s.size(), ' ');
	return s;
}

const char* DataTypeToText(DataType t) {
	if(t == type_Int)
		return "Int";
	else if(t == type_Bool)
		return "Bool";
	else if(t == type_Float)
		return "Float";
	else if(t == type_Double)
		return "Double";
	else if(t == type_String)
		return "String";
	else if(t == type_Binary)
		return "Binary";
	else if(t == type_Date)
		return "Date";
	else if(t == type_Table)
		return "Table";
	else if(t == type_Mixed)
		return "Mixed";
	else {
		TIGHTDB_ASSERT(true);
		return "";
	}
}

void print_col_names(Table& table) {
	cout << "\n";
	for(size_t t = 0; t < table.get_column_count(); t++) {
		string s = string(table.get_column_name(t).data()) + " (" + string(DataTypeToText(table.get_column_type(t))) + ")";
		s = set_width(s, print_width);
		cout << s.c_str() << " ";
	}
	cout << "\n" << string(table.get_column_count() * (print_width + 1), '-').c_str() << "\n";
}

void print_row(Table& table, size_t r) {
	for(size_t c = 0; c < table.get_column_count(); c++) {
		char buf[print_width];

		if(table.get_column_type(c) == type_Bool)
			sprintf(buf, "%d", table.get_bool(c, r));
		if(table.get_column_type(c) == type_Double)
			sprintf(buf, "%f", table.get_double(c, r));
		if(table.get_column_type(c) == type_Float)
			sprintf(buf, "%f", table.get_float(c, r));
		if(table.get_column_type(c) == type_Int)
			sprintf(buf, "%d", table.get_int(c, r));
		if(table.get_column_type(c) == type_String) {
#if _MSC_VER
			_snprintf(buf, sizeof(buf), "%s", table.get_string(c, r).data());
#else							
			snprintf(buf, sizeof(buf), "%s", table.get_string(c, r).data());
#endif
		}
		string s = string(buf);
		s = set_width(s, print_width);
		cout << s.c_str() << " ";
	}
	cout << "\n";					
}


bool is_null(const char* v)
{
	if (v[0] == 0)
		return true;

	if(v[1] != 'u' && v[1] != 'U')
		return false;

	if(strcmp(v, "NULL") == 0 || strcmp(v, "Null") == 0 || strcmp(v, "null") == 0)
		return true;

	return false;
}

Importer::Importer(void) : Quiet(false), Separator(',') 
{

}

// Set can_fail = true to detect if a string is an integer. In this case, provide a success argument
// Set can_fail = false to do the conversion. In this case, omit success argument. 
template <bool can_fail> int64_t Importer::parse_integer(const char* col, bool* success)
{
	int64_t	x = 0;

	if(can_fail && is_null(col)) {
		if(!m_empty_as_string_flag)
			*success = true;
		else
			*success = false;
		return 0;
	}

	if (*col == '-'){
		++col;
		x = 0;
		while (*col != '\0') {
			if (can_fail && ('0' > *col || *col > '9')){
				*success = false;
				return 0;
			}
			int64_t y = *col - '0';
			if (can_fail && x < (std::numeric_limits<int64_t>::min()+y)/10){
				*success = false;
				return 0;
			}

			x = 10*x-y;
			++col;
		}
		if(can_fail)
			*success = true;
		return x;
	}
	else if (*col == '+')
		++col;

	while (*col != '\0'){
		if(can_fail && ('0' > *col || *col > '9')){
			*success = false;
			return 0;
		}
		int64_t y = *col - '0';
		x = 10*x+y;
		++col;
	}

	if(can_fail)
		*success = true;

	return x;
}


template <bool can_fail> bool Importer::parse_bool(const char*col, bool* success)
{
	// Must be tuples of {true value, false value}
	static const char* a[] = {"True", "False", "true", "false", "TRUE", "FALSE", "1", "0", "Yes", "No", "yes", "no", "YES", "NO"};

	char c = *col;	

	// Perform quick check in order to terminate fast if non-bool. Unfortunatly VC / gcc does NOT optimize a loop 
	// that iterates through a[n][0] to remove redundant letters, even though 'a' is static const. So we need to do 
	// it manually.
	if(can_fail) {
		if (is_null(col)) {
			if(!m_empty_as_string_flag)
				*success = true;
			else
				*success = false;
			return false;
		}

		char lower = c | 32;
		if(c != '1' && c != '0' && lower != 't' && lower != 'f' && lower != 'y' && lower != 'n') {
			*success = false;
			return false;
		}

		for(size_t t = 0; t < sizeof(a) / sizeof(a[0]); t++)
		{
			if(strcmp(col, a[t]) == 0)
			{
				*success = true;
//				m_bool_true = a[t >> 1 << 1][0];
				return !((t & 0x1) == 0);
			}
		}
		*success = false;
		return false;
	}
	
	char lower = c | 32;
	if(c == '1' || lower == 't' || lower == 'y')
		return true;
	else
		return false;

//  This is a super fast way that only requires 1 compare because we have already detected the true/false values
//  earlier, so we just need to compare with that. However, it's not complately safe, so we avoid it
//	return (c == m_bool_true);

}

// Set can_fail = true to detect if a string is a float. In this case, provide a 'success' argument. It will return false
// if the number of significant decimal digits in the input string is greater than 6, in which case you should use 
// doubles instead.
//
// Set can_fail = false to do the conversion. In this case, omit success argument. 
template <bool can_fail> float Importer::parse_float(const char*col, bool* success)
{
	bool s;
	size_t significants = 0;	
	double d = parse_double<can_fail>(col, &s, &significants);

	// Fail if plain text input has more than 6 significant digits (a float can have ~7.2 correct digits)
	if(can_fail && (!s || significants > 6)) {
		*success = false;
		return 0.0;	
	}

	if(can_fail && success != NULL)
		*success = true;

	return static_cast<float>(d);
}

// Set can_fail = true to detect if a string is a double. In this case, provide 'success' and 'significants' arguments.
// It will return the number of significant digits in the input string in 'significants', that is, 4 for 1.234 and
// 4 for -2.531e9 and 5 for 1.0000 (this is helper functionality for parse_float()).
//
// Set can_fail = false to do the conversion. In this case, omit success argument. Empty string ('') converts to 0.0 
// and integers with no radix point ('5000') converts to nearest double.
template <bool can_fail> double Importer::parse_double(const char* col, bool* success, size_t* significants)
{
	const char* orig_col = col;
	double x;
	bool is_neg = false;
	size_t dummy;
	if(can_fail && significants == NULL)
		significants = &dummy;

	if(can_fail && is_null(col)) {
		if(!m_empty_as_string_flag)
			*success = true;
		else
			*success = false;
		return 0;
	}

	if (*col == '-') {
		is_neg = true;
		++col;
	}
	else if (*col == '+')
		++col;

	x = 0;
	while('0' <= *col && *col <= '9'){
		int y = *col - '0';
		x *= 10;
		x += y;
		++col;
		++*significants;
	}
			
	if(*col == '.'|| *col == Separator){
		++col;
		double pos = 1;
		while ('0' <= *col && *col <= '9'){
			pos /= 10;
			int y = *col - '0';
			++col;
			x += y*pos;
			++*significants;
		}
	}

	if(*col == 'e' || *col == 'E'){
		if(can_fail && col == orig_col) {
			*success = false;
			return 0;
		}

		++col;
		int64_t e;
		e = parse_integer<false>(col);
				
		if (e != 0) {
			double base;	
			if (e < 0) {
				base = 0.1;
				e = -e;
			}
			else {
				base = 10;
			}
	
			while (e != 1) {
				if ((e & 1) == 0) {
					base = base*base;
					e >>= 1;
				}
				else {
					x *= base;
					--e;
				}
			}
			x *= base;
		}
	}
	else if (can_fail && *col != '\0') {
		*success = false;
		return 0;
	}
	if(is_neg)
		x = -x;

	if(can_fail)
		*success = true;

	return x;
}

// Takes a row of payload and returns a vector of TightDB types. Prioritizes Bool > Int > Float > Double > String. If 
// m_empty_as_string_flag == false, then empty strings ('') are converted to Integer 0.
vector<DataType> Importer::types (vector<string> v)
{
	vector<DataType> res;

	for(size_t t = 0; t < v.size(); t++) {
		bool i;
		bool d;
		bool f;
		bool b;

		parse_integer<true>(v[t].c_str(), &i);
		parse_double<true>(v[t].c_str(), &d);
		parse_float<true>(v[t].c_str(), &f);
		parse_bool<true>(v[t].c_str(), &b);

		if(is_null(v[t].c_str()) && !m_empty_as_string_flag) {
			// If m_empty_as_string_flag == false, then integer/bool fields containing empty strings may be converted to 0/false 
			i = true;
			d = true;
			f = true;
			b = true;
		}

		res.push_back(b ? type_Bool : i ? type_Int : f ? type_Float : d ? type_Double : type_String);
	}

	return res;
}

// Takes two vectors of TightDB types, and for each field finds best type that can represent both.
vector<DataType> Importer::lowest_common(vector<DataType> types1, vector<DataType> types2)
{
	vector<DataType> res;

	for(size_t t = 0; t < types1.size(); t++) {
		// All choices except for the last must be ||. The last must be &&
		if(types1[t] == type_String || types2[t] == type_String) 
			res.push_back(type_String);
		else if(types1[t] == type_Double || types2[t] == type_Double)
			res.push_back(type_Double);
		else if((types1[t] == type_Float && types2[t] == type_Int) || (types2[t] == type_Float && types1[t] == type_Int)) {
			// This covers the special case where first values are integers and suddenly radix points occur. In this case we must import as double, because a float 
			// may not be precise enough to hold the number of significant digits in the integers. Todo: We could keep track of the significant digits seen in
			// all integers so that we know if we can import as float instead.
			res.push_back(type_Double);
		}
		else if(types1[t] == type_Float || types2[t] == type_Float)
			res.push_back(type_Float);
		else if(types1[t] == type_Int || types2[t] == type_Int)
			res.push_back(type_Int);
		else if(types1[t] == type_Bool && types2[t] == type_Bool)
			res.push_back(type_Bool);
		else
			assert(false);
	}
	return res;
}

// Takes payload vectors, and for each field finds best type that can represent all rows.
vector<DataType> Importer::detect_scheme(vector<vector<string> > payload, size_t begin, size_t end)
{
	vector<DataType> res;
	res = types(payload[begin]);

	for(size_t t = begin + 1; t < end && t < payload.size(); t++) {
		vector<DataType> t2 = types(payload[t]);	
		res = lowest_common(res, t2);
	}
	return res;
}

size_t Importer::import(vector<vector<string> > & payload, size_t records) 
{
	size_t original_size = payload.size();

nextrecord:

    if(payload.size() - original_size >= records)
        goto end;

    if(top - s < chunk_size / 2) {
        memmove(src, src + s, top - s);
        top -= s;
        size_t r = fread(src + top, 1, chunk_size / 2, f);
        top += r;
        s = 0;
        if(r != chunk_size / 2) {
            src[top] = 0;
        }
    }

    if(src[s] == 0)
        goto end;

	payload.push_back(vector<string>());

nextfield:

    if(src[s] == 0)
        goto end;

	payload.back().push_back("");

	while(src[s] == ' ')
		s++;

	if(src[s] == '"') {
		// Field in quotes - can only end with another quote
		s++;
payload:
		while(src[s] != '"') {
			// Payload character
			payload.back().back().push_back(src[s]);
			s++;
		}

		if(src[s + 1] == '"') {
			// Double-quote
			payload.back().back().push_back('"');
			s += 2;
			goto payload;
		}
		else {
			// Done with field
			s += 1;

			// Only whitespace is allowed to occur between end quote and non-comma/non-eof/non-newline
			while(src[s] == ' ')
				s++;
        }

	}
	else {

		// Field not in quotes - cannot contain quotes or commas. So read until quote or comma or eof. Even though it's
		// non-conforming, some CSV files can contain non-quoted line breaks, so we need to test if we can't test for
		// new record by just testing for 0a/0d.
		size_t fields = payload.back().size();
		while(src[s] != Separator && src[s] != 0 && ((src[s] != 0xd && src[s] != 0xa) || (fields < m_fields && m_fields != size_t(-1)) )) {
			payload.back().back().push_back(src[s]);
			s++;
		}
		
	}

	if(src[s] == Separator) {
		s++;
		goto nextfield;
	}

	if(src[s] == 0xd || src[s] == 0xa) {
		s++;
		if(src[s] == 0xd || src[s] == 0xa)
			s++;
		goto nextrecord;
	}

	goto nextfield;

end:

	return payload.size() - original_size;
}

size_t Importer::import_csv(FILE* file, Table& table, vector<DataType> *scheme2, vector<string> *column_names, size_t type_detection_rows, bool empty_as_string_flag, size_t skip_first_rows, size_t import_rows)
{
	m_empty_as_string_flag = empty_as_string_flag;

	vector<vector<string> > payload;
	vector<string> header;
	vector<DataType> scheme;
	bool header_present = false;
	vector<vector<string> > pprint;

	top = 0;
	s = 0;
	d = 0;
	m_fields = static_cast<size_t>(-1);

    f = file;

	if(scheme2 == NULL) {
		// Auto detection of format:
		// 3 scenarios for header: 1) If first line is text-only and second row contains at least 1 non-string field, then
		// header is guaranteed present. 2) If both lines are string-only, we can't tell, and import first line as payload
		// 3) If at least 1 field of the first row is non-string, no header is present.
		import(payload, 2);

		// The flight data files contain "" as last field in header. This is the reason why we temporary disable conversion
		// to 0 during hedaer detection 
		bool original_empty_as_string_flag = empty_as_string_flag;
		m_empty_as_string_flag = true;
		vector<DataType> scheme1 = detect_scheme(payload, 0, 1);

		// First row is best one to detect number of fields since it's less likely to contain embedded line breaks
		m_fields = scheme1.size();

		vector<DataType> scheme2 = detect_scheme(payload, 1, 2);
		bool only_strings1 = true;
		bool only_strings2 = true;
		for(size_t t = 0; t < scheme1.size(); t++) {
			if(scheme1[t] != type_String)
				only_strings1 = false;
			if(scheme2[t] != type_String)
				only_strings2 = false;
		}
		m_empty_as_string_flag = original_empty_as_string_flag;

		if(only_strings1 && !only_strings2)
			header_present = true;


		if(header_present) {
			// Use first row of csv for column names
			header = payload[0];
			payload.erase(payload.begin());

			for(size_t t = 0; t < header.size(); t++) {
				// In flight database, header is present but contains null ("") as last field. We replace such 
				// occurences by a string
				if(header[t] == "") {
					char buf[10];
					sprintf(buf, "Column%d\0", t);
					header[t] = buf;
				}
			}

		}
		else {
			// Use "1", "2", "3", ... for column names
			for(int i = 0; i < scheme1.size(); i++) {
				char buf[10];
				sprintf(buf, "%d\0", i);
				header.push_back(buf);
			}
		}

		// Detect scheme using next N rows. 
		import(payload, type_detection_rows);
		scheme = detect_scheme(payload, 0, type_detection_rows);
	}
	else {
		// Use user provided column names and types
		scheme = *scheme2;
		header = *column_names;
	}

	// Create sheme in TightDB table
	for(size_t t = 0; t < scheme.size(); t++)
		table.add_column(scheme[t], StringData(header[t]).data());
   
	if(!Quiet)
		print_col_names(table);

	size_t imported_rows = 0;

	if(skip_first_rows > 0) {
		import(payload, skip_first_rows);
		payload.clear();
	}

	do {
		for(size_t row = 0; row < payload.size(); row++) {

			if(imported_rows == import_rows)
				return imported_rows;

			if(!Quiet && imported_rows % 1000 == 0)
				printf("%d k rows...\r", imported_rows / 1000);

			table.add_empty_row();
			for(size_t col = 0; col < scheme.size(); col++) {
				bool success = true;

				if(scheme[col] == type_String)
					table.set_string(col, imported_rows, StringData(payload[row][col]));
				else if(scheme[col] == type_Int)
					table.set_int(col, imported_rows, parse_integer<true>(payload[row][col].c_str(), &success));
				else if(scheme[col] == type_Double)
					table.set_double(col, imported_rows, parse_double<true>(payload[row][col].c_str(), &success));
				else if(scheme[col] == type_Float)
					table.set_float(col, imported_rows, parse_float<true>(payload[row][col].c_str(), &success));
				else if(scheme[col] == type_Bool)
					table.set_bool(col, imported_rows, parse_bool<true>(payload[row][col].c_str(), &success));
				else
					assert(false);

				if(!success) {
					// Remove all columns so that user can call csv_import() on it again
					table.clear();
					
					for(size_t t = 0; t < table.get_column_count(); t++)
						table.remove_column(0);

					std::stringstream sstm;

					if(type_detection_rows > 0) {
						if(scheme[col] != type_String && is_null(payload[row][col].c_str()) && empty_as_string_flag)
							sstm << "Column " << col << " was auto detected to be of type " << DataTypeToText(scheme[col]) 
							<< " using the first " << type_detection_rows << " rows of CSV file, but in row " << 
							imported_rows << " of cvs file the field contained the NULL value '" << 
							payload[row][col].c_str() << "'. Please increase the 'type_detection_rows' argument or set "
							"empty_as_string_flag = false/void the -e flag to convert such fields to 0, 0.0 or false";
						else
							sstm << "Column " << col << " was auto detected to be of type " << DataTypeToText(scheme[col]) 
							<< " using the first " << type_detection_rows << " rows of CSV file, but in row " << 
							imported_rows << " of cvs file the field contained '" << payload[row][col].c_str() << 
							"' which is of another type. Please increase the 'type_detection_rows' argument";
					}
					else
						sstm << "Column " << col << " was specified to be of type " << DataTypeToText(scheme[col]) <<
						", but in row " << imported_rows << " of cvs file," << "the field contained '" << 
						payload[row][col].c_str() << "' which is of another type";

					throw runtime_error(sstm.str());
				}
			}


			if(!Quiet) {
				if(imported_rows < 10)
					print_row(table, imported_rows);
				else if(imported_rows == 11)
					cout << "\nOnly showing first few rows...\n";
			}

			imported_rows++;
		}
		payload.clear();
		import(payload, record_chunks);
	}
	while(payload.size() > 0);

	return imported_rows;

}

size_t Importer::import_csv_auto(FILE* file, Table& table, size_t type_detection_rows, bool empty_as_string_flag, size_t import_rows)
{
	return import_csv(file, table, NULL, NULL, type_detection_rows, empty_as_string_flag, 0, import_rows);
}

size_t Importer::import_csv_manual(FILE* file, Table& table, vector<DataType> scheme, vector<string> column_names, size_t skip_first_rows, size_t import_rows)
{
	return import_csv(file, table, &scheme, &column_names, 0, 0, skip_first_rows, import_rows);
}




