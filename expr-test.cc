
// (c) COPYRIGHT URI/MIT 1995-1996
// Please read the full copyright statement in the file COPYRIGH.  
//
// Authors:
//      jhrg,jimg       James Gallagher (jgallagher@gso.uri.edu)

// Test the CE scanner and parser.
//
// jhrg 9/12/95

// $Log: expr-test.cc,v $
// Revision 1.19  1999/01/21 02:50:08  jimg
// Added code to test the expr scanner using strings and not files.
//
// Revision 1.18  1998/11/10 00:49:19  jimg
// Fixed up the online help.
//
// Revision 1.17  1998/09/17 17:00:02  jimg
// Added include files to get rid of compiler messages about missing
// prototypes.
//
// Revision 1.16  1998/03/26 00:15:53  jimg
// Added keep_temps global for use with debuggers to keep those temp file
// around.
// Added parse_mime() to gobble up the mime header generated by DDS::send()
//
// Revision 1.15  1998/03/19 23:29:20  jimg
// Removed old code (that was surrounded by #if 0 ... #endif).
//
// Revision 1.14  1997/09/22 22:33:14  jimg
// Added data file option. Now -f can be used to specify the name of a file
// from which to read data. This currently only works with Sequences, but
// in the future all test data could be read from a file.
// Added use of the DataDDS class (which is required by the new core
// software).
//
// Revision 1.13  1997/06/05 22:51:25  jimg
// Changed so that compression is not used.
//
// Revision 1.12  1996/08/13 18:55:20  jimg
// Added __unused__ to definition of char rcsid[].
// Uses the parser_arg object to communicate with the parser.
//
// Revision 1.11  1996/06/11 17:30:36  jimg
// Fixed -k (constraint expression) option when used with -p (parser) option.
//
// Revision 1.10  1996/06/04 21:34:00  jimg
// Multiple connections are now possible. It is now possible to open several
// URLs at the same time and read from them in a round-robin fashion. To do
// this I added data source and sink parameters to the serialize and
// deserialize mfuncs. Connect was also modified so that it manages the data
// source `object' (which is just an XDR pointer).
//
// Revision 1.9  1996/05/31 23:30:58  jimg
// Updated copyright notice.
//
// Revision 1.8  1996/05/29 22:04:13  jimg
// Removed old, useless, code.
//
// Revision 1.7  1996/05/22 18:05:35  jimg
// Merged files from the old netio directory into the dap directory.
// Removed the errmsg library from the software.
//
// Revision 1.6  1996/05/14 15:38:57  jimg
// These changes have already been checked in once before. However, I
// corrupted the source repository and restored it from a 5/9/96 backup
// tape. The previous version's log entry should cover the changes.
//
// Revision 1.5  1996/03/05 00:57:19  jimg
// Fixed tests of constrained tranmission so CEs with spaces will be read
// properly.
// Added new option so that a CE may be given on the command line.
//
// Revision 1.4  1995/12/09  01:07:37  jimg
// Added changes so that relational operators will work properly for all the
// datatypes (including Sequences). The relational ops are evaluated in
// DDS::eval_constraint() after being parsed by DDS::parse_constraint().
//
// Revision 1.3  1995/12/06  19:43:09  jimg
// Added options for testing the constraint evaluator software.
// Added functions which test the constraint evaluator.
// Added function that simulates te complete client-server conversation which
// causes a variable to be sent after the evaluation of a CE. This manages
// multiple DDSs just as a real client would. This code is different than the
// simpler code run by evaluate_dds().
//
// Revision 1.2  1995/10/23  23:08:17  jimg
// Fixed scanner display code to match current scanner.
// Added code to test simple evaluator.
// Fixed type declarations (YYSTYPE, ...).
//
// Revision 1.1  1995/10/13  03:02:26  jimg
// First version. Runs scanner and parser.
//

#include "config_dap.h"

static char rcsid[] __unused__ = {"$Id: expr-test.cc,v 1.19 1999/01/21 02:50:08 jimg Exp $"};

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <rpc/types.h>
#include <netinet/in.h>
#include <rpc/xdr.h>

#include <streambuf.h>
#include <iostream.h>
#include <stdiostream.h>
#include <GetOpt.h>

#include <String.h>
#include <SLList.h>

#include "DDS.h"
#include "DataDDS.h"
#include "BaseType.h"

#include "parser.h"
#include "expr.h"
#include "expr.tab.h"
#include "util.h"
#include "debug.h"

#define DODS_DDS_PRX "dods_dds"
#define YY_BUFFER_STATE (void *)

void test_scanner(const char *str);
void test_scanner(bool show_prompt);
void test_parser(DDS &table, const String &dds_name, const String &constraint);
bool read_table(DDS &table, const String &name, bool print);
void evaluate_dds(DDS &table, bool print_constrained);
bool transmit(DDS &write, bool verb);
bool loopback_pipe(FILE **pout, FILE **pin);
bool constrained_trans(const String &dds_name, String dataset,
		       const String &ce);

int exprlex();			// exprlex() uses the global exprlval
int exprparse(void *arg);
int exprrestart(FILE *in);

// Glue routines declared in expr.lex
void expr_switch_to_buffer(void *new_buffer);
void expr_delete_buffer(void * buffer);
void *expr_string(const char *yy_str);

extern int exprdebug;

static int keep_temps = 0;

const String version = "version 1.12";
const String prompt = "expr-test: ";
const String options = "sS:detcvp:w:f:k:";
const String usage = "expr-test [-s [-S string] -d -c -t -v [-p dds-file] [-e expr]\
[-w dds-file] [-f data-file] [-k expr]]\n\
Test the expression evaluation software.\n\
Options:\n\
	-s: Feed the input stream directly into the expression scanner, does\n\
	    not parse.\n\
        -S: <string> Scan the string as if it was standard input.\n\
	-d: Turn on expression parser debugging.\n\
	-c: Print the constrained DDS (the one that will be returned\n\
	    prepended to a data transmission. Must also supply -p and -e \n\
	-t: Test transmission of data. This uses the Test*classes.\n\
	    Transmission is done using a single process that writes and then\n\
	    reads from a pipe. Must also suppply -p.\n\
        -v: Print the version of expr-test\n\
  	-p: DDS-file: Read the DDS from DDS-file and create a DDS object,\n\
	    then prompt for an expression and parse that expression, given\n\
	    the DDS object.\n\
	-e: Evaluate the constraint expression. Must be used with -p.\n\
	-w: Do the whole enchilada. You don't need to supply -p, -e, ...\n\
	     This prompts for the constraint expression and the optional\n\
             data file name. NOTE: The CE parser Error objects do not print\n\
             with this option.\n\
        -f: A file to use for data. Currently only used by -w for sequences.\n\
	-k: A constraint expression to use with the data. Works with -p,\n\
	    -e, -t and -w\n";

int
main(int argc, char *argv[])
{
    GetOpt getopt(argc, argv, options);
    int option_char;
    bool scanner_test = false, parser_test = false, evaluate_test = false;
    bool trans_test = false, print_constrained = false;
    bool whole_enchalada = false, constraint_expr = false;
    bool scan_string = false;
    String dds_file_name;
    String dataset = "";
    String constraint = "";
    DDS table;

    // process options

    while ((option_char = getopt()) != EOF)
	switch (option_char) {
	  case 'd': 
	    exprdebug = true;
	    break;
	  case 's':
	    scanner_test = true;
	    break;
	  case 'S':
	    scanner_test = true;
	    scan_string = true;
	    constraint = getopt.optarg;
	    break;
	  case 'p':
	    parser_test = true;
	    dds_file_name = getopt.optarg;
	    break;
	  case 'e':
	    evaluate_test = true;
	    break;
	  case 't':
	    trans_test = true;
	    break;
	  case 'c':
	    print_constrained = true;
	    break;
	  case 'w':
	    whole_enchalada = true;
	    dds_file_name = getopt.optarg;
	    break;
	  case 'k':
	    constraint_expr = true;
	    constraint = getopt.optarg;
	    break;
	  case 'f':
	    dataset = getopt.optarg;
	    break;
	  case 'v':
	    cerr << argv[0] << ": " << version << endl;
	    exit(0);
	  case '?': 
	  default:
	    cerr << usage << endl; 
	    exit(1);
	    break;
	}

    if (!scanner_test && !parser_test && !evaluate_test && !trans_test 
	&& !whole_enchalada) {
	cerr << usage << endl;
	exit(1);
    }

    // run selected tests

    if (scanner_test) {
	if (scan_string)
	    test_scanner(constraint);
	else
	    test_scanner(true);
	exit(0);
    }

    if (parser_test) {
	test_parser(table, dds_file_name, constraint);
    }

    if (evaluate_test) {
	evaluate_dds(table, print_constrained);
    }

    if (trans_test) {
	transmit(table, exprdebug);
    }

    if (whole_enchalada) {
	constrained_trans(dds_file_name, dataset, constraint);
    }
}

// Instead of reading the tokens from srdin, read them from a string.


void
test_scanner(const char *str)
{
    exprrestart(0);
    void *buffer = expr_string(str);
    expr_switch_to_buffer(buffer);

    test_scanner(false);

    expr_delete_buffer(buffer);
}

void
test_scanner(bool show_prompt)
{
    if (show_prompt) 
	cout << prompt;		// first prompt

    int tok;
    while ((tok = exprlex())) {
	switch (tok) {
	  case ID:
	    cout << "ID: " << exprlval.id << endl;
	    break;
	  case STR:
	    cout << "STR: " << *exprlval.val.v.s << endl;
	    break;
	  case FIELD:
	    cout << "FIELD: " << exprlval.id << endl;
	    break;
	  case INT:
	    cout << "INT: " << exprlval.val.v.i << endl;
	    break;
	  case FLOAT:
	    cout << "FLOAT: " << exprlval.val.v.f << endl;
	    break;
	  case EQUAL:
	    cout << "EQUAL: " << exprlval.op << endl;
	    break;
	  case NOT_EQUAL:
	    cout << "NOT_EQUAL: " << exprlval.op << endl;
	    break;
	  case GREATER:
	    cout << "GREATER: " << exprlval.op << endl;
	    break;
	  case GREATER_EQL:
	    cout << "GREATER_EQL: " << exprlval.op << endl;
	    break;
	  case LESS:
	    cout << "LESS: " << exprlval.op << endl;
	    break;
	  case LESS_EQL:
	    cout << "LESS_EQL: " << exprlval.op << endl;
	    break;
	  case REGEXP:
	    cout << "REGEXP: " << exprlval.op << endl;
	    break;
	  case '*':
	    cout << "Dereference" << endl;
	    break;
	  case '.':
	    cout << "Field Selector" << endl;
	    break;
	  case ',':
	    cout << "List Element Separator" << endl;
	    break;
	  case '[':
	    cout << "Left Bracket" << endl;
	    break;
	  case ']':
	    cout << "Right Bracket" << endl;
	    break;
	  case '(':
	    cout << "Left Paren" << endl;
	    break;
	  case ')':
	    cout << "Right Paren" << endl;
	    break;
	  case '{':
	    cout << "Left Brace" << endl;
	    break;
	  case '}':
	    cout << "Right Brace" << endl;
	    break;
	  case ':':
	    cout << "Colon" << endl;
	    break;
	  case '&':
	    cout << "Ampersand" << endl;
	    break;
	  default:
	    cout << "Error: Unrecognized input" << endl;
	}
	cout << prompt;		// print prompt after output
    }
}

// NB: The DDS is read in via a file because reading from stdin must be
// terminated by EOF. However, the EOF used to terminate the DDS also closes
// stdin and thus the expr scanner exits immediately.

void
test_parser(DDS &table, const String &dds_name, const String &constraint)
{
    read_table(table, dds_name, true);

    bool status;

    if (constraint != "") 
	status = table.parse_constraint(constraint);
    else {
	exprrestart(stdin);

	cout << prompt;

	parser_arg arg(&table);

	status = exprparse((void *)&arg) == 0;

	//  STATUS is the result of the parser function; if a recoverable error
	//  was found it will be true but arg.status() will be false.
	if (!status || !arg.status()) {// Check parse result
	    if (arg.error())
		arg.error()->display_message();
	    status = false;
	}
	else
	    status = true;
    }

    if (status)
	cout << "Input parsed" << endl;
    else
	cout << "Input did not parse" << endl;
}

// Read a DDS from stdin and build the cooresponding DDS. IF PRINT is true,
// print the text reprsentation of that DDS on the stdout. The DDS TABLE is
// modified as a side effect.
//
// Returns: true iff that DDS pasted the semantic_check() mfunc, otherwise
// false.

bool
read_table(DDS &table, const String &name, bool print)
{
    int parse = table.parse(name);
    
    if (!parse) {
	cout << "Input did not parse" << endl;
	return false;
    }
    
    if (print)
	table.print();

    if (table.check_semantics(true))
	return true;
    else {
	cout << "Input did not pass semantic checks" << endl;
	return false;
    }
}

void
evaluate_dds(DDS &table, bool print_constrained)
{
    if (print_constrained)
	table.print_constrained();
    else
	for (Pix p = table.first_var(); p; table.next_var(p))
	    table.var(p)->print_decl(cout, "", true, true);
}

// Given that a DDS has been created (nominally via read_table() above and
// that a constraint expression has been entered, send data from the DDS to a
// second DDS instance via the serialize/deserialize mfuncs. 

bool
transmit(DDS &write, bool verb)
{
    bool status;
    FILE *pin, *pout;

    status = loopback_pipe(&pin, &pout);
    if (!status) {
	cerr << "expr-test: Could not create the loopback streams" << endl;
	return false;
    }

    XDR *sink = new_xdrstdio(pout, XDR_ENCODE);
    XDR *source = new_xdrstdio(pin, XDR_DECODE);

    // duplicate the DDS (create the variables for reading)

    DDS read = write;

    // for each variable in the write DDS, read it (loading it with dummy
    // values from the mfuncs supplied by the Test classes) and send it. Then
    // read the variable values back into the read DDS and print the received
    // values. 

    String dummy = "dummy";
    Pix wp, rp;
    for (wp = write.first_var(), rp = read.first_var(); 
	 wp && rp; 
	 write.next_var(wp), read.next_var(rp)) {

	// This code only works for scalar variables at the top level of the
	// DDS. It also ignores the read_p() mfunc.
	if (write.var(wp)->send_p()) { // only works for scalars
	    int error = 0;
	    status = write.var(wp)->read(dummy, error);
	    if (error != -1)
		status = false;

	    if (verb) {
		cout << "Variable to be written:" << endl;
		write.var(wp)->print_val(cout);
		cout << endl;
		cout.flush();
	    }

	    status = write.var(wp)->serialize(dummy, write, sink, true);
	    if (!status) {
		cerr << "Could not write";
		write.var(wp)->print_decl(cerr);
		exit(1);
	    }

	    // The following line passes a pointer to a DDS into a mfunc that
	    // expects a pointer to a DataDDS (which is accessed via a cast
	    // for now). This works here only because this function -
	    // transmit() - can only be used with scalar types which don't
	    // use the DataDDS. jhrg 9/19/97.
	    status = read.var(rp)->deserialize(source, &read);
	    if (!status) {
		cerr << "Could not read";
		read.var(wp)->print_decl(cerr);
		exit(1);
	    }

	    if (verb)
		cout << "Variable read:" << endl;

	    read.var(rp)->print_val(cout);

	    if (verb)
		cout << endl;
	}

	cout.flush();
    }

    delete_xdrstdio(sink);
    delete_xdrstdio(source);

    return true;
}

// create a pipe for the caller's process which can be used by the DODS
// software to write to ad read from itself.

bool
loopback_pipe(FILE **pout, FILE **pin)
{
    // make a pipe

    int fd[2];
    if (pipe(fd) < 0) {
	cerr << "Could not open pipe" << endl;
	return false;
    }

    *pout = fdopen(fd[1], "w");
    *pin = fdopen(fd[0], "r");

    return true;
}


// Originally in netexec.c (part of the netio library).
// Read the DDS from the data stream. Leave the binary information behind. The
// DDS is moved, without parsing it, into a file and a pointer to that FILE is
// returned. The argument IN (the input FILE stream) is positioned so that the
// next byte is the binary data.
//
// The binary data follows the text `Data:', which itself starts a line.
//
// Returns: a FILE * which contains the DDS describing the binary information
// in IF.

FILE *
move_dds(FILE *in)
{
    char *c = tempnam(NULL, "dods");
    if (!c) {
	cerr << "Could not create temporary file name" << strerror(errno)
	    << endl;
	return NULL;
    }

    FILE *fp = fopen(c, "w+");
    if (!keep_temps)
	unlink(c);
    if (!fp) {
	cerr << "Could not open anonymous temporary file: " 
	     << strerror(errno) << endl;
	return NULL;
    }
	    
    int data = FALSE;
    char s[256], *sp;
    
    sp = &s[0];
    while (!feof(in) && !data) {
	sp = fgets(s, 255, in);
	if (strcmp(s, "Data:\n") == 0)
	    data = TRUE;
	else
	    fputs(s, fp);
    }

    fflush(fp);
    if (fseek(fp, 0L, 0) < 0) {
	cerr << "Could not rewind data DDS stream: " << strerror(errno)
	    << endl;
	return NULL;
    }
    
    free(c);			// tempnam uses malloc
    return fp;
}

// Gobble up the mime header. At one time the MIME Headers were output from
// the server filter programs (not the core software) so we could call
// DDS::send() from this test code and not have to parse the MIME header. But
// in order to get errors to work more reliably the header generation was
// moved `closer to the send'. That is, we put off determining whether to
// send a DDS or an Error object until later. That trade off is that the
// header generation is not buried in the core software. This code simply
// reads until the end of the header is found. 3/25/98 jhrg

void
parse_mime(FILE *data_source)
{
    istdiostream is(data_source);
    
    char line[256];

    is.getline(line, 256);
    
    while ((String)line != "")
	is.getline(line, 256);
}

// Test the transmission of constrained datasets. Use read_table() to read
// the DDS from a file. Once done, prompt for the variable name and
// constraint expression. In a real client-server system the server would
// read the DDS for the entire dataset and send it to the client. The client
// would then respond to the server by asking for a variable given a
// constraint.
// 
// Once the constraint has been entered, it is evaluated in the context of
// the DDS using DDS:eval_constraint() (this would happen on the server-side
// in a real system). Once the evaluation is complete,
// DDS::print_constrained() is used to create a DDS describing only those
// parts of the dataset that are to be sent to the client process and written
// to the output stream. After that, the marker `Data:' is written to the
// output stream, followed by the binary data.

bool
constrained_trans(const String &dds_name, String dataset, 
		  const String &constraint) 
{
    bool status;
    FILE *pin, *pout;
    DDS server;			// could use DataDDS, but no need when
				// sending. 

    cout << "The complete DDS:" << endl;
    read_table(server, dds_name, true);

    status = loopback_pipe(&pout, &pin);
    if (!status) {
	cerr << "Could not create the loopback streams" << endl;
	return false;
    }

    // If the CE was not passed in, read it from the command line.
    String ce;
    if (constraint == "") {
	cout << "Constraint:";
	char c[256];
	cin.getline(c, 256);
	if (!cin) {
	    cerr << "Could nore read the constraint expression" << endl;
	    exit(1);
	}
	ce = c;
    }
    else
	ce = constraint;

    if (dataset == "") {
	cout << "Data file:";
	char c[256];
	cin.getline(c, 255);
	if (!cin) {
	    cerr << "Could nore read the data file name" << endl;
	    exit(1);
	}
	dataset = c;
    }

    // send the variable given the constraint; TRUE flushes the I/O channel.
    // Currently only Sequence uses the `dataset' parameter.
    if (!server.send(dataset, ce, pout, false)) {
	cerr << "Could not send the DDS" << endl;
	return false;
    }

    fclose(pout);		// close pout to read from pin. Why?
    
    // Now do what Connect::request_data() does:

    // First read the DDS into a new object (using a file to store the DDS
    // temporarily - the parser/scanner won't stop reading until an EOF is
    // found, this fixes that problem).

    DataDDS dds("Test_data", "DODS/2.15"); // Must use DataDDS on receving end
    FILE *dds_fp = move_dds(pin);
    DBG(cerr << "Moved the DDS to a temp file" << endl);
    parse_mime(dds_fp);
    if (!dds.parse(dds_fp)) {
	cerr << "Could not parse return data description" << endl;
	return false;
    }
    fclose(dds_fp);

    XDR *source = new_xdrstdio(pin, XDR_DECODE);

    // Back on the client side; deserialize the data *using the newly
    // generated DDS* (the one sent with the data).

    cout << "The data:" << endl;
    for (Pix q = dds.first_var(); q; dds.next_var(q)) {
	// Currently the return status of deserialize can mean two things;
	// and error (false) or no data for a variable matched the query
	// (also false). This should be fixed in an upcomming release. jhrg
	// 9/22/97. 
	if (!dds.var(q)->deserialize(source, &dds))
	    return false;
	switch (dds.var(q)->type()) {
	    // Sequences present a special case because I let
	    // their semantics get out of hand... jhrg 9/12/96
	  case dods_sequence_c:
	    ((Sequence *)dds.var(q))->print_all_vals(cout, source, &dds);
	    break;
	  default:
	    dds.var(q)->print_val(cout);
	    break;
	}
    }

    delete_xdrstdio(source);

    return true;
}






