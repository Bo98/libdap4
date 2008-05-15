
// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2002 OPeNDAP, Inc.
// Author: James Gallagher <jgallagher@opendap.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

#ifndef _http_cache_table_h
#define _http_cache_table_h

#include <pthread.h>

#ifdef WIN32
#include <io.h>   // stat for win32? 09/05/02 jhrg
#endif

#include <string>
#include <vector>
#include <map>

#ifndef _error_h
#include "Error.h"
#endif

#ifndef _internalerr_h
#include "InternalErr.h"
#endif

#define LOCK(m) pthread_mutex_lock((m))
#define TRYLOCK(m) pthread_mutex_trylock((m))
#define UNLOCK(m) pthread_mutex_unlock((m))

#define INIT(m) pthread_mutex_init((m), 0)
#define DESTROY(m) pthread_mutex_destroy((m))

using namespace std;

namespace libdap
{

int get_hash(const string &url);

/** The table of entires in the client-side cache.
    @todo This class needed to have the locking mechanism in place to work 
    or it must become a singleton. */
class HTTPCacheTable {
public:
	/** A struct used to store information about responses in the
	cache's volatile memory. 

	About entry locking: An entry is locked using both a mutex and a
	counter. The counter keeps track of how many clients are accessing a
	given entry while the mutex provides a guarantee that updates to the
	counter are MT-safe. In addition, the HTTPCache object maintains a
	map which binds the FILE* returned to a client with a given entry.
	This way the client can tell the HTTPCache object that it is done
	with <code>FILE *response</code> and the class can arrange to update
	the lock counter and mutex. */
	struct CacheEntry
	{
	private:
		string url;  // Location
	    int hash;
	    int hits;  // Hit counts
	    string cachename;

	    string etag;
	    time_t lm;  // Last modified
	    time_t expires;
	    time_t date;  // From the response header.
	    time_t age;
	    time_t max_age;  // From Cache-Control

	    unsigned long size; // Size of cached entity body
	    bool range;  // Range is not currently supported. 10/02/02 jhrg

	    time_t freshness_lifetime;
	    time_t response_time;
	    time_t corrected_initial_age;

	    bool must_revalidate;
	    bool no_cache;  // This field is not saved in the index.

	    int locked;
	    pthread_mutex_t lock ;
	    
	    // Allow HTTPCacheTable methods access and the test class, too
	    friend class HTTPCacheTable;
		friend class HTTPCacheTest;
	    
		// Allow access by the fucntors used in HTTPCacheTable
		friend class DeleteCacheEntry;
	    friend class WriteOneCacheEntry;
	    friend class DeleteExpired;
	    friend class DeleteByHits;
	    friend class DeleteBySize;

	public:
		string get_cachename() {
			return cachename;
		}
		string get_etag() {
			return etag;
		}
		time_t get_lm() {
			return lm;
		}
		time_t get_expires() {
			return expires;
		}
		time_t get_max_age() {
			return max_age;
		}
		void set_size(unsigned long sz) {
			size = sz;
		}
		time_t get_freshness_lifetime() {
			return freshness_lifetime;
		}
		time_t get_response_time() {
			return response_time;
		}
		time_t get_corrected_initial_age() {
			return corrected_initial_age;
		}
		bool get_must_revalidate() {
			return must_revalidate;
		}
		void set_no_cache(bool state) {
			no_cache = state;
		}
	    bool is_no_cache() { return no_cache; }
	    pthread_mutex_t &get_lock() { return lock; }

		CacheEntry() :
			url(""), hash(-1), hits(0), cachename(""), etag(""), lm(-1),
					expires(-1), date(-1), age(-1), max_age(-1), size(0),
					range(false), freshness_lifetime(0), response_time(0),
					corrected_initial_age(0), must_revalidate(false),
					no_cache(false), locked(0) {
			INIT(&lock);
		}
		CacheEntry(const string &u) :
			url(u), hash(-1), hits(0), cachename(""), etag(""), lm(-1),
					expires(-1), date(-1), age(-1), max_age(-1), size(0),
					range(false), freshness_lifetime(0), response_time(0),
					corrected_initial_age(0), must_revalidate(false),
					no_cache(false), locked(0) {
			INIT(&lock);
			hash = get_hash(url);
		}
	};

	// Typedefs for CacheTable. A CacheTable is a vector of vectors of
	// CacheEntries. The outer vector is accessed using the hash value.
	// Entries with matching hashes occupy successive positions in the inner
	// vector (that's how hash collisions are resolved). Search the inner
	// vector for a specific match.
	typedef vector<CacheEntry *> CacheEntries;
	typedef CacheEntries::iterator CacheEntriesIter;

	typedef CacheEntries **CacheTable;// Array of pointers to CacheEntries

	friend class HTTPCacheTest;
	
private:
	CacheTable d_cache_table;
    
	string d_cache_root;
    unsigned int d_block_size; // File block size.
    unsigned long d_current_size;

    string d_cache_index;
    int d_new_entries;
    
    map<FILE *, HTTPCacheTable::CacheEntry *> d_locked_entries;
    
	// Make these private to prevent use
	HTTPCacheTable(const HTTPCacheTable &) {
		throw InternalErr(__FILE__, __LINE__, "unimplemented");
	}
	
	HTTPCacheTable &operator=(const HTTPCacheTable &) {
		throw InternalErr(__FILE__, __LINE__, "unimplemented");
	}
	
	HTTPCacheTable() {
		throw InternalErr(__FILE__, __LINE__, "unimplemented");
	}

	CacheTable &get_cache_table() { return d_cache_table; }

public:
	HTTPCacheTable(const string &cache_root, int block_size);
	~HTTPCacheTable();
	
	//@{ @name Accessors/Mutators
	unsigned long get_current_size() const { return d_current_size; }
	void set_current_size(unsigned long sz) { d_current_size = sz; }
	
	unsigned int get_block_size() const { return d_block_size; }
	void set_block_size(unsigned int sz) { d_block_size = sz; }
	
	int get_new_entries() const { return d_new_entries; }
	void increment_new_entries() { ++d_new_entries; }
	
	string get_cache_root() { return d_cache_root; }
	void set_cache_root(const string &cr) { d_cache_root = cr; }
	//@}

	void delete_expired_entries(time_t time = 0);
	void delete_by_hits(int hits);
	void delete_by_size(unsigned int size);
	void delete_all_entries();
	
	bool cache_index_delete();
	bool cache_index_read();
	CacheEntry *cache_index_parse_line(const char *line);
	void cache_index_write();
	
    string create_hash_directory(int hash);
    void create_location(CacheEntry *entry);

	void add_entry_to_cache_table(CacheEntry *entry);
	CacheEntry *get_entry_from_cache_table(int hash, const string &url); /*const*/
	void remove_entry_from_cache_table(const string &url);
	CacheEntry *get_entry_from_cache_table(const string &url);
	void calculate_time(HTTPCacheTable::CacheEntry *entry,
			int default_expiration, time_t request_time);
	void parse_headers(HTTPCacheTable::CacheEntry *entry, 
			unsigned long max_entry_size, const vector<string> &headers);
	
	void lock_entry(CacheEntry *entry, FILE *body);
	void unlock_entry(FILE *body);
	bool is_locked_entries();
	
	void remove_cache_entry(HTTPCacheTable::CacheEntry *entry);
};

} // namespace libdap
#endif
