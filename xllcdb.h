// xllcdb.h - column database
// Uncomment the following line to use features for Excel2007 and above.
//#define EXCEL12
#include <algorithm>
#include <iostream>
#include <fstream>
#include <functional>
#include <memory>
#include "xll/xll.h"

#ifndef CATEGORY
#define CATEGORY _T("CDB")
#endif

typedef xll::traits<XLOPERX>::xcstr xcstr; // pointer to const string
typedef xll::traits<XLOPERX>::xword xword; // use for OPER and FP indices
typedef xll::traits<XLOPERX>::xfp xfp;

namespace std {
	template<> // called by ~unique_ptr
	struct default_delete<HANDLE> {
		void operator()(HANDLE* ph) 
		{
			if (ph && *ph)
				CloseHandle(*ph); 
			if (ph)
				delete ph;
		} 
	};
}

class unique_handle {
	std::unique_ptr<HANDLE> h_;
public:
	unique_handle(HANDLE h)
		: h_(new HANDLE)
	{
		ensure (h != INVALID_HANDLE_VALUE);
		*h_ = h;
	}
	operator HANDLE() const
	{
		return *h_.get();
	}
};

namespace cdb {

	// NVI idiom
	template<class T>
	class enumerator {
	public:
		enumerator() { }
		void reset() { _reset(); }
		bool next() { return _next(); }
		const T& current() const { return _current(); }
	protected:
		virtual ~enumerator() { }
	private:
		virtual void _reset() = 0;
		virtual bool _next() = 0;
		virtual const T& _current() const = 0;
	};

	template<class T>
	class enumerable {
	public:
		enumerator<T> get(void) { return _get(); }
	protected:
		virtual ~enumerable() { }
	private:
		virtual enumerable<T> _get() = 0;
	};

	class table {
		size_t upper_, lower_, columns_;
		std::vector<const double*> t_;
	public:
		// index in the range [start, stop)
		table(LPCTSTR index, double start = 0, double stop = 0)
			: columns_(1)
		{
			unique_handle f(CreateFile(index, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
			unique_handle m(CreateFileMapping(f, 0, PAGE_READONLY, 0, 0, 0)); // map entire index for now!!!
			double* v = static_cast<double*>(MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0));
			size_t n = GetFileSize(f, 0)/sizeof(double); // assume high word is 0

			lower_ = start ? std::lower_bound(v, v + n, start) - v : 0;
			upper_ = stop ? std::lower_bound(v, v + n, stop) - v : n;

			t_.push_back(v + lower_);
		}
		~table()
		{
			std::for_each(t_.begin(), t_.end(), UnmapViewOfFile);
		}
		table& select(LPCTSTR column)
		{
			unique_handle f(CreateFile(column, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
			unique_handle m(CreateFileMapping(f, 0, PAGE_READONLY, 0, 0, 0));

			t_.push_back(static_cast<double*>(MapViewOfFile(m, FILE_MAP_READ, 0, lower_*sizeof(double), (upper_ - lower_)*sizeof(double))));
			++columns_;

			return *this;
		}
		size_t rows(void) const
		{
			return upper_ - lower_;
		}
		size_t columns(void) const
		{
			return columns_;
		}
		const double* column(size_t i) const
		{
			return &(t_[i])[0];
		}
	};


/*
	// read only existing file
	class ifile {
		HANDLE f_;
	public:
		ifile()
			: f_(0)
		{ }
		ifile(LPCTSTR name)
			: f_(0)
		{
			f_ = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			ensure (f_ != INVALID_HANDLE_VALUE);
		}
		~ifile()
		{
			if (f_)
				CloseHandle(f_);
		}
		operator HANDLE() const
		{
			return f_;
		}
		size_t size(void) const
		{
			return GetFileSize(f_, 0); // assume high word is 0
		}
	};

	// read only mapped file
	class imap {
		HANDLE h_;
		size_t n_;
	public:
		imap()
			: h_(0)
		{ }
		imap(const ifile& f, DWORD hi = 0, DWORD lo = 0)
			: h_(0)
		{
			h_ = CreateFileMapping(f, 0, PAGE_READONLY, hi, lo, 0);
			ensure (h_ != NULL);
			if (!hi && !lo)
				n_ = f.size();
			else
				n_ = lo; // + hi<<0xFFFF 
		}
		~imap()
		{
			if (h_)
				CloseHandle(h_);
		}
		operator HANDLE() const
		{
			return h_;
		}
		size_t size(void) const
		{
			return n_;
		}
	};

	template<class T>
	class iview {
		const T* p_;
		size_t n_; // size of T
	public:
		iview()
			: p_(0), n_(0)
		{ }
		iview(const imap& m, DWORD hi = 0, DWORD lo = 0, DWORD n = 0)
		{
			p_ = static_cast<T*>(MapViewOfFile(m, FILE_MAP_READ, hi, lo, n*sizeof(T)));
			ensure (p_);

			n_ = n ? n : m.size()/sizeof(T);

			return p_;
		}
		~iview()
		{
			if (p_)
				UnmapViewOfFile(p_);
		}
		size_t size(void) const
		{
			return n_;
		}
		const T& operator[](size_t i)
		{
			return p_[i];
		}
		const T* begin(void) const
		{
			return p_;
		}
		const T* end(void) const
		{
			return p_ + n_;
		}
	};

	template<class T>
	class table {
		size_t ilower_, iupper_;
		std::vector<std::string> name_;
		std::vector<iview<T>> col_;
	public:
		table(LPCTSTR index, T lower = 0, T upper = 0, DWORD hi = 0, DWORD lo = 0, size_t n = 0)
		{
			ifile ii(index);
			imap im(ii);
			iview<T> iv(im, hi, lo, n);

			ilower_ = lower ? std::lower_bound(iv.begin(), iv.end(), lower) - iv.begin() : 0;
			iupper_ = upper ? std::upper_bound(iv.begin(), iv.end(), upper) - iv.begin() : iv.size();

			name_.push_back(index);
			col_.push_back(iv);
		}
		~table()
		{ }
		table& add(LPCTSTR column)
		{
			ifile ic(column);
			imap im(ic);
			iview<T> iv(ic, 0, ilower/sizeof(T), iupper_ - ilower_); // assume hi = 0

			name_.push_back(column);
			col_.push_back(iv);

			return *this;
		}
	};

	// memory mapped file of array of T
	template<class T>
	class array {
		HANDLE f_; // file
		HANDLE m_; // mapped 
		T* p_; // pointer to data
		size_t n_; // size of data
		array(const array& a);
		array& operator=(const array& a);
	public:
		array()
			: f_(0), m_(0), p_(0), n_(0)
		{
		}
		~array()
		{
			close();
		}
		size_t size(void) const
		{
			return n_;
		}
		void open(LPCTSTR name, DWORD access = GENERIC_READ, DWORD share = FILE_SHARE_READ, DWORD create = OPEN_EXISTING)
		{
			f_ = CreateFile(name, access, share, 0, create, FILE_ATTRIBUTE_NORMAL, 0);
			ensure (f_ != INVALID_HANDLE_VALUE);

			m_ = CreateFileMapping(f_, 0, PAGE_READONLY, 0, 0, 0);
			ensure (m_ != NULL);
		}
		void close(void)
		{
			m_ && CloseHandle(m_);
			f_ && CloseHandle(f_);
		}
		T* map(DWORD hi = 0, DWORD lo = 0, DWORD n = 0)
		{
			n_ = n ? n : GetFileSize(f_, 0)/sizeof(T);
			p_ = static_cast<T*>(MapViewOfFile(m_, FILE_MAP_READ, hi, lo, n*sizeof(T)));

			return p_;
		}
		BOOL unmap(void)
		{
			n_ = 0;

			return UnmapViewOfFile(p_);
		}
		const T* begin(void) const
		{
			return p_;
		}
		const T* end(void) const
		{
			return p_ + n_;
		}
		T operator[](size_t i) const
		{
			return p_[i];
		}
	};

	struct directory : private std::basic_string<TCHAR> {
		directory(LPCTSTR dir = _T("."))
			: std::basic_string<TCHAR>(dir)
		{
			if (!dir || *dir == 0)
				this->assign(_T("."));
		}
		directory(const std::basic_string<TCHAR>& dir)
			: std::basic_string<TCHAR>(dir)
		{ }
		~directory()
		{ }
		operator LPCTSTR() const
		{
			return this->c_str();
		}
		std::basic_string<TCHAR> file(LPCTSTR name) const
		{
			return file(std::basic_string<TCHAR>(name));
		}
		std::basic_string<TCHAR> file(const std::basic_string<TCHAR>& name) const
		{
			return std::basic_string<TCHAR>(*this).append(_T("/")).append(name);
		}
	};

	class columns {
		directory dir_;
		std::vector<std::basic_string<TCHAR>> col_;
		array<double>* array_;
	public:
		columns(const directory& dir)
			: dir_(dir), array_(0)
		{
		}
		~columns()
		{
			delete [] array_;
		}
		size_t add(const std::basic_string<TCHAR> col)
		{
			col_.push_back(col);

			return col_.size();
		}
		void execute()
		{
			array_ = new array<double>[col_.size()];
			for (size_t i = 0; i < col_.size(); ++i) {
				array_[i].open(dir_.file(col_[i]).c_str());
			}
		}
		size_t size(void) const
		{
			return col_.size();
		}
		array<double>& operator[](size_t i)
		{
			return array_[i];
		}
	};
	
	class select {
		columns& col_;
		size_t i_; // current index
		DWORD n_; // number of rows
		select(const select&);
		select& operator=(const select&);
	public:
		select(columns& col, double a = 0, double b = 0, size_t c = 0)
			: col_(col), i_(~0u)
		{
			// std::binary_search return a bool :-(
			col_[c].map();
			const double* pa = a ? std::lower_bound(col_[c].begin(), col_[c].end(), a) : col_[c].begin();
			const double* pb = b ? std::upper_bound(col_[c].begin(), col_[c].end(), b) : col_[c].end();
			col_[c].unmap();

			DWORD lo = pa - col_[c].begin();
			n_ = pb - pa;
			for (size_t i = 0; i < col_.size(); ++i) {
				col_[i].map(0, lo, n_);
			}
		}
		~select()
		{ }
		size_t size(void) const
		{
			return col_.size();
		}
		DWORD rows(void) const
		{
			return n_;
		}
		size_t current(void) const
		{
			return i_;
		}
		size_t next(void)
		{
			return ++i_;
		}
		double column(size_t i) const
		{
			return col_[i][i_];
		}
	};
	*/
} // namespace cdb