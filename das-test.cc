
// Test the DAS class.
// Read attributes from one or more files, printing the resulting table to
// stdout. If a file is named `-' read from stdin for that file. The option
// `-d' causes new/delete run-time debugging to be turned on.
//
// jhrg 7/25/94

// $Log: das-test.cc,v $
// Revision 1.11  1995/07/08 18:34:31  jimg
// Removed old code.
// Removed unnecessary declaration of dasparse().
//
// Revision 1.10  1995/05/10  15:34:07  jimg
// Failed to change `config.h' to `config_dap.h' in these files.
//
// Revision 1.9  1995/05/10  13:45:42  jimg
// Changed the name of the configuration header file from `config.h' to
// `config_dap.h' so that other libraries could have header files which were
// installed in the DODS include directory without overwriting this one. Each
// config header should follow the convention config_<name>.h.
//
// Revision 1.8  1995/03/16  17:36:15  jimg
// Added include config_dap.h to top of includes.
// Added TRACE_NEW switch dbnew debugging.
//
// Revision 1.7  1995/02/10  03:28:55  jimg
// Removed dummy_read.cc
// Updates das-test.cc so that type checking gets tested.
//
// Revision 1.6  1994/12/07  21:21:47  jimg
// Added code to test the scanner - it is selected using the -s option.
//
// Revision 1.5  1994/10/13  16:12:07  jimg
// Added -p to interface: run the parser. This option makes das-test
// behave as it used to, with the following exceptions:
// 1) if `none' is given as a filename, then the associated actions (reading
// or writing) is not performed. This lets you test the ctors and dtors
// w/o anything else getting in the way.
// 2) - means stdin or stdout
// Also added AttrTable test code that gets run if -p is not given.
//
// Revision 1.4  1994/10/05  16:48:52  jimg
// Added code to print the return value of the DAS::parse() mfunc.
//
// Revision 1.3  1994/09/27  22:58:59  jimg
// das-test no longer uses Space.cc for new/delete debugging - that system
// did not work on alphas).
//
// Revision 1.2  1994/09/09  16:13:16  jimg
// Added code to test the stdin, FILE * and file descriptor functions of
// class DAS.
//
// Revision 1.1  1994/08/02  18:08:38  jimg
// Test driver for DAS (and AttrTable) classes.
//

#include "config_dap.h"

#include <iostream.h>
#include <String.h>
#include <Pix.h>
#include <GetOpt.h>

#define YYSTYPE char *

#include "DAS.h"
#include "das.tab.h"

#ifdef TRACE_NEW
#include "trace_new.h"
#endif

void plain_driver(DAS das);
void load_attr_table(AttrTable at);
void load_attr_table_ptr(AttrTable *atp);
void parser_driver(int argc, char *argv[], int i, bool use_fd, DAS das);
void test_scanner();

int daslex();
#ifdef NEVER
int dasparse(DAS &);
#endif

extern int dasdebug;
const char *prompt = "das-test: ";

int
main(int argc, char *argv[])
{

    GetOpt getopt (argc, argv, "sfp");
    int option_char;
    bool use_fd = false;	// true to exercise the fd functions
    bool test_parser = false;
    bool scanner_test = false;
    // process options first so that debugging in on for object instantitaion.
    while ((option_char = getopt ()) != EOF)
	switch (option_char)
	  {
	    case 'p':
	      test_parser = true;
	      break;
	    case 'f':
	      use_fd = true;
	      break;
	    case 's':
	      scanner_test = true;
	      break;
	    case '?': 
	      cerr << "usage: " << argv[0] << " [f] [p in-file1 out-file1 ...]"
		   << endl;
	  }

    DAS das;

    if (test_parser)
	for (int i = getopt.optind; i < argc; i+=2)
	    parser_driver(argc, argv, i, use_fd, das);
    else if (scanner_test)
	test_scanner();
    else
	plain_driver(das);
}

void
test_scanner(void)
{
    int tok;

    cout << prompt;		// first prompt
    while ((tok = daslex())) {
	switch (tok) {
	  case ATTR:
	    cout << "ATTR" << endl;
	    break;

	  case ID:
	    cout << "ID=" << daslval << endl;
	    break;
	  case STR:
	    cout << "STR=" << daslval << endl;
	    break;
	  case INT:
	    cout << "INT=" << daslval << endl;
	    break;
	  case FLOAT:
	    cout << "FLOAT=" << daslval << endl;
	    break;

	  case BYTE:
	    cout << "BYTE" << endl;
	    break;
	  case INT32:
	    cout << "INT32" << endl;
	    break;
	  case FLOAT64:
	    cout << "FLOAT64" << endl;
	    break;
	  case STRING:
	    cout << "STRING" << endl;
	    break;
	  case URL:
	    cout << "URL" << endl;
	    break;

	  case '{':
	    cout << "Left Brace" << endl;
	    break;
	  case '}':
	    cout << "Right Brace" << endl;
	    break;
	  case ';':
	    cout << "Semicolon" << endl;
	    break;
	  case ',':
	    cout << "Comma" << endl;
	    break;

	  default:
	    cout << "Error: Unrecognized input" << endl;
	}
	cout << prompt;		// print prompt after output
    }
}


void
parser_driver(int argc, char *argv[], int i, bool use_fd, DAS das)
{
    // If a file is named "-", assume that the user means stdin or stdout.
    if (strcmp(argv[i], "none") == 0)
	return;
    else if (strcmp(argv[i], "-") == 0) {
	cout << "Enter attributes:\n";
	int status = das.parse();
	if (!status)
	    cerr << "parse() returned: " << status << endl;
    }
    else {
	cout << "Reading from: " << argv[i] << endl;
	if (use_fd) {
	    int fd = open(argv[i], O_RDONLY);
	    int status = das.parse(fd);
	    if (!status)
		cerr << "parse() returned: " << status << endl;
	    close(fd);
	}
	else {
	    int status = das.parse(argv[i]);
	    if (!status)
		cerr << "parse() returned: " << status << endl;
	}
    }

    // filename of "none" means don't print.
    if (argc > i+1 && strcmp(argv[i+1], "-") == 0)
	das.print();
    else
	return;
}

// Given a DAS, add some stuff to it.

void
plain_driver(DAS das)
{
    AttrTable *atp;
    AttrTable *dummy;

    String name = "test";
    atp = new AttrTable;
    load_attr_table_ptr(atp);
    dummy = das.get_table(name);
    das.add_table(name, atp);

    name = "test2";
    atp = new AttrTable;
    load_attr_table_ptr(atp);
    das.add_table(name, atp);

    das.print();
}

// stuff an AttrTable full of values. Also, print it out.

void
load_attr_table(AttrTable at)
{
    at.append_attr("month", "String", "Feb");
    at.append_attr("month", "String", "Feb");

    at.append_attr("month_a", "String", "Jan");
    at.append_attr("month_a", "String", "Feb");
    at.append_attr("month_a", "String", "Mar");

    at.append_attr("Date", "Int32", "12345");
    at.append_attr("day", "Int32", "01");
    at.append_attr("Time", "Float64", "3.1415");

    cout << "Using the Pix:" << endl;
    for (Pix p = at.first_attr(); p; at.next_attr(p)) {
	cout << at.get_name(p) << " " << at.get_type(p) << " ";
	for (int i = 0; i < at.get_attr_num(p); ++i)
	     cout << at.get_attr(p, i) << " ";
	cout << endl;
    }

    String name = "month";
    cout << "Using String: " << at.get_type(name) << " " 
	 << at.get_attr(name, 0) << " " << at.get_attr(name, 1) << endl;
    cout << "Using char *: " << at.get_type("month") << " " 
	 << at.get_attr("month", 0) << " " << at.get_attr("month", 1) << endl;

    at.del_attr("month");

    cout << "After deletion:" << endl;
    for (p = at.first_attr(); p; at.next_attr(p)) {
	cout << at.get_name(p) << " " << at.get_type(p) << " ";
	for (int i = 0; i < at.get_attr_num(p); ++i)
	     cout << at.get_attr(p, i) << " ";
	cout << endl;
    }

    at.print(cout);

    cout << "After print:" << endl;
    for (p = at.first_attr(); p; at.next_attr(p)) {
	cout << at.get_name(p) << " " << at.get_type(p) << " ";
	for (int i = 0; i < at.get_attr_num(p); ++i)
	     cout << at.get_attr(p, i) << " ";
	cout << endl;
    }
}

// OK, now try it with a dymanic AttrTable

void
load_attr_table_ptr(AttrTable *at)
{
    at->append_attr("month", "String", "Feb");
    at->append_attr("month", "String", "Feb");

    at->append_attr("month_a", "String", "Jan");
    at->append_attr("month_a", "String", "Feb");
    at->append_attr("month_a", "String", "Mar");

    at->append_attr("Date", "Int32", "12345");
    at->append_attr("day", "Int32", "01");
    at->append_attr("Time", "Float64", "3.1415");

    cout << "Using the Pix:" << endl;
    for (Pix p = at->first_attr(); p; at->next_attr(p)) {
	cout << at->get_name(p) << " " << at->get_type(p) << " ";
	for (int i = 0; i < at->get_attr_num(p); ++i)
	     cout << at->get_attr(p, i) << " ";
	cout << endl;
    }

    String name = "month";
    cout << "Using String: " << at->get_type(name) << " " 
	 << at->get_attr(name, 0) << " " << at->get_attr(name, 1) << endl;
    cout << "Using char *: " << at->get_type("month") << " " 
	 << at->get_attr("month", 0) << " " << at->get_attr("month", 1) << endl;

    at->del_attr("month");

    cout << "After deletion:" << endl;
    for (p = at->first_attr(); p; at->next_attr(p)) {
	cout << at->get_name(p) << " " << at->get_type(p) << " ";
	for (int i = 0; i < at->get_attr_num(p); ++i)
	     cout << at->get_attr(p, i) << " ";
	cout << endl;
    }

    at->print(cout);

    cout << "After print:" << endl;
    for (p = at->first_attr(); p; at->next_attr(p)) {
	cout << at->get_name(p) << " " << at->get_type(p) << " ";
	for (int i = 0; i < at->get_attr_num(p); ++i)
	     cout << at->get_attr(p, i) << " ";
	cout << endl;
    }
}
