// xllcdb.cpp - Column Database
/*
Use std excel functionality for directories.
dir  = CDB.DIRECTORY("foo/bar")
query = CDB.COLUMNS(dir, "time.t", "price.d", "hi.d", "lo.d", "volume.u")
query  = CDB.INTERVAL(query, start, end, column?)
row = CDB.ROW(query) (volatile)
range = CDB.RANGE(query)
array = CDB.ARRAY(query)
*/
#include "xll/utility/message.h"
#include "xllcdb.h"

using namespace cdb;
using namespace xll;

static AddInX xai_cdb(
	DocumentX(CATEGORY)
	.Documentation(_T("Column database. "))
);

#define MAX_COLS 10
/*
static AddInX xai_cdb_directory(
	FunctionX(XLL_HANDLEX, _T("?xll_cdb_directory"), _T("CDB.DIRECTORY"))
	.Arg(XLL_CSTRINGX, _T("Directory"), _T("is the directory containing column data. "))
	.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp(_T("Returns a handle to the Directory."))
	.Documentation(_T("Documentation."))
	);
HANDLEX WINAPI xll_cdb_directory(xcstr dir)
{
#pragma XLLEXPORT
	static HANDLEX h(0);

	try {
		handle<directory> hdir(new directory(dir));

		h = hdir.get();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return h;
}

static AddInX xai_cdb_columns(
	FunctionX(XLL_HANDLEX, _T("?xll_cdb_columns"), _T("CDB.COLUMNS"))
	.Arg(XLL_HANDLEX, _T("Directory"), _T("is a handle to a Directory."))
	.Arg(XLL_LPOPERX, _T("Columns"), _T("is and array of Column names from Directory. "))
	.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp(_T("Returns a handle to the Columns."))
	.Documentation(_T("Documentation."))
	);
HANDLEX WINAPI xll_cdb_columns(HANDLEX dir, LPOPERX pCols)
{
#pragma XLLEXPORT
	static HANDLEX h(0);

	try {
		handle<directory> hdir(dir);
		ensure (hdir);
		handle<cdb::columns> hcol(new cdb::columns(*hdir));

		const OPERX& cols(*pCols);
		for (xword i = 0; i < cols.size(); ++i)
			hcol->add(to_string<XLOPERX>(cols[i]));

		hcol->execute();

		h = hcol.get();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return h;
}

static AddInX xai_cdb_select(
	FunctionX(XLL_HANDLEX, _T("?xll_cdb_select"), _T("CDB.SELECT"))
	.Arg(XLL_HANDLEX, _T("Columns"), _T("is a handle to Columns."))
	.Arg(XLL_DOUBLEX, _T("Start?"), _T("is the optional lower bound of the selected rows."))
	.Arg(XLL_DOUBLEX, _T("Stop?"), _T("is the optional upper bound of the selected rows."))
	.Arg(XLL_USHORTX, _T("Column?"), _T("is the optional column used to define the selection. Default is 1. "))
	.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp(_T("Returns a handle to the selected rows."))
	.Documentation(_T("Documentation."))
	);
HANDLEX WINAPI xll_cdb_select(HANDLEX cols, double start, double stop, USHORT c)
{
#pragma XLLEXPORT
	static HANDLEX h(0);

	try {
		handle<cdb::columns> hcols(cols);
		ensure (hcols);
		if (c == 0)
			c = 1;
		handle<select> hsel(new cdb::select(*hcols, start, stop, c - 1));

		h = hsel.get();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return h;
}

static AddInX xai_cdb_select_next(
	FunctionX(XLL_LPOPERX, _T("?xll_cdb_select_next"), _T("CDB.SELECT.NEXT"))
	.Arg(XLL_HANDLEX, _T("Selection"), _T("is a handle to a Selection. "))
	.Volatile()
	.Category(CATEGORY)
	.FunctionHelp(_T("Return current row from Selection and advance to next."))
	.Documentation(_T("Documentation."))
);
LPOPERX WINAPI xll_cdb_select_next(HANDLEX sel)
{
#pragma XLLEXPORT
	static OPERX o;

	try {
		handle<select> hsel(sel);
		ensure (hsel);

		if (hsel->next() == hsel->rows()) {
			o = ErrX(xlerrNA);

			return &o;
		}
			
		o.resize(1, static_cast<xword>(hsel->size()));

		for (xword i = 0; i < o.columns(); ++i)
			o[i] = hsel->column(i);

	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		o = ErrX(xlerrNA);
	}

	return &o;
}
*/

#ifdef _DEBUG

#define NROWS 100

class darray : public cdb::enumerator<double> {
	const double* x_;
	size_t i_, n_;
public:
	darray(size_t n, const double* x)
		: i_(~0u), n_(n), x_(x)
	{ }
	~darray()
	{ }
	void _reset(void)
	{
		i_ = ~0u;
	}
	bool _next(void)
	{
		++i_;

		return i_ != n_;
	}
	const double& _current() const
	{
		return x_[i_];
	}
};

int xll_cdb_test(void)
{
	try {
		double x[10];
		for (int i = 0; i < 10; ++i)
			x[i] = i;

		darray da(10, x);
		cdb::enumerator<double>* pe = &da;
		size_t i = 0;
		while (pe->next())
			ensure (pe->current() == i++);
		pe->reset();
		pe->next();
		ensure (pe->current() == 0);
		{
			DWORD n;
			double x;
			unique_handle data(CreateFile(_T("data"), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0));
			for (int i = 0; i < NROWS; ++i) {
				x = i;
				ensure (WriteFile(data, &x, sizeof(double), &n, 0));
				ensure (n == sizeof(double));
			}
		}

		table t("data");
		ensure (t.columns() == 1);
		ensure (t.rows() == NROWS);
		const double* pt = t.column(0);
		for (int i = 0; i < NROWS; ++i)
			ensure (pt[i] == i);

		t.select("data");
		ensure (t.columns() == 2);
		ensure (t.rows() == NROWS);

		table t2("data", 2);
		ensure (t2.columns() == 1);
		ensure (t2.rows() == NROWS - 2);
		pt = t2.column(0);
		for (size_t i = 0; i < t2.rows(); ++i)
			ensure (pt[i] == i + 2);

		t2.select("data");
		ensure (t2.columns() == 2);
		ensure (t2.rows() == NROWS - 2);

		table t3("data", 2, 8);
		ensure (t3.columns() == 1);
		ensure (t3.rows() == 8 - 2);
		ensure (t3.column(0)[t3.rows() - 1] == 7);
		pt = t3.column(0);
		for (size_t i = 0; i < t3.rows(); ++i)
			ensure (pt[i] == i + 2);

		t3.select("data");
		ensure (t3.columns() == 2);
		ensure (t3.rows() == 8 - 2);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		return 0;
	}

	return 1;
}
static Auto<Open> xao_cdb_test(xll_cdb_test);

#endif // _DEBUG