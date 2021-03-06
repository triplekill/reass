/*
 * Copyright 2011 Hylke Vellinga
 */


#ifndef __REASS_FREELIST_H__
#define __REASS_FREELIST_H__

#include <boost/noncopyable.hpp>
#include <assert.h>
#include "reass/config.h"
#include "reass/force_inline.h"

// FIXME: could re-write this to use intrusive's slist,
// and replace the d_delayed multimap with a sorted list
// (could use same storage that way)

template<typename T>
struct free_list_member_t : public boost::noncopyable
{
	free_list_member_t(T *&free_head) :
	 	d_free_head(free_head) // d_free_next does not need to be initialised
 	{
	}

	~free_list_member_t()
	{
#ifdef NO_REUSE
		assert(d_free_head != this);
#endif
		// note, when a member is deleted from halfway through the freelist, the
		// list is broken
		if (d_free_head == this)
			d_free_head = d_free_next;
	}

	void release()
	{
#ifdef NO_REUSE
		delete static_cast<T *>(this);
#else
		d_free_next = d_free_head;
		d_free_head = static_cast<T *>(this);
#endif
	}

private:

	void claim()
	{
		assert(this);
		if (d_free_head == this) // race condition if items got deleted and we are in-place -> never delete
		{ // we are in the free list, remove ourselves
			d_free_head = d_free_next;
		}
	}

	template<typename Q>
	friend struct free_list_container_t;

	T *&d_free_head;
	T *d_free_next; // NULL if we are the last in the chain, undefined if not free
};

template<typename T>
struct free_list_container_t : public boost::noncopyable
{
	free_list_container_t(unsigned pre_claim = 0) :
		d_free_head(NULL)
#if !defined(NO_REUSE) and defined(DEBUG)
		, d_count(pre_claim)
#endif
	{
#if !defined(NO_REUSE)
		for (unsigned n=0; n<pre_claim; ++n)
		{
			T *r = new T(d_free_head);
			r->release();
		}
#endif
	}

	~free_list_container_t()
	{
		while(d_free_head)
			delete d_free_head;
	}


	T *claim()
	{
#ifdef NO_REUSE
		T *r = new T(d_free_head);
#else
		T *r = NULL;
		if (d_free_head)
			r = d_free_head;
		else
		{
			r = new T(d_free_head);
#ifdef DEBUG
			++d_count;
			assert(d_count < 100000);
#endif
		}
		r->claim();
#endif
		return r;
	}

#if !defined(NO_REUSE) and defined(DEBUG)
	// returns number of allocated objects
	unsigned objectcount() const { return d_count; }
#endif

protected:
	T *d_free_head;
#ifdef DEBUG
	uint64_t d_count;
#endif
};

template<typename T>
struct auto_release_t
{
	auto_release_t(T *t) : d_t(t) {}
	FORCE_INLINE ~auto_release_t() { if (d_t) d_t->release(); }

	void do_not_release() { d_t = NULL; }
protected:
	T *d_t;
};

#endif // __REASS_FREELIST_H__
