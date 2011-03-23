// Copyright (C) 2008-2011 by Vincent Falco, All rights reserved worldwide.
// This file is released under the MIT License:
// http://www.opensource.org/licenses/mit-license.php

#ifndef __VF_LOCKFREEALLOCATOR_VFHEADER__
#define __VF_LOCKFREEALLOCATOR_VFHEADER__

#include "vf/vf_Atomic.h"
#include "vf/vf_LockFreeReadWriteMutex.h"
#include "vf/vf_LockFreeStack.h"
#include "vf/vf_Type.h"

#define ALLOCATOR_COUNT_SWAPS 0

namespace LockFree {

//
// This number should be as small as possible
// without generating compile time assertions.
//
enum
{
  globalAllocatorBlockSize = 96
};

//
// Mostly lock-free allocator for fixed size memory blocks
//
// - Any thread may allocate and free concurrently.
//
// - Freed blocks are recycled and deleted in the destructor.
//
// - If the free block list is exhausted, the algorithm
//   becomes a locking algorithm since we resort to the
//   standard library to get more memory.
//
// - Every once in a while we will perform garbage collection
//   using a blocking algorithm. This eliminates the ABA problem
//   for anyone using the allocator.
//
// Because the underlying Stack is vulnerable to the ABA problem,
// we use two stacks. One for re-using blocks during allocation,
// and a different one for keeping track of freed blocks.
// Every once in a while, we take a mutex (the write lock), block
// all readers (a reader is someone who is calling alloc() or free()),
// and swap the stacks in O(1).
//

template <size_t BlockSize>
class FixedAllocator
{
public:
  //
  // This allocator will time its garbage collections to
  // try to keep the memory utilization at or below the byteLimit.
  //
  // This is something of a soft limit but we should be close.
  //
  enum
  {
    defaultMegaBytes = 2
  };

  enum
  {
    blockSize = BlockSize
  };

  FixedAllocator (int byteLimit = defaultMegaBytes * 1024 * 1024)
    : m_interval (byteLimit / (2 * (BlockSize + sizeof(Node))))
    , m_count (m_interval)
#if ALLOCATOR_COUNT_SWAPS
    , m_swaps (0)
#endif
  {
  }

  ~FixedAllocator ()
  {
#if VF_CHECK_LEAKS
    vfassert (m_used.is_reset ());
#endif

    free (m_free);
    free (m_junk);

#if VF_CHECK_LEAKS
    vfassert (m_total.is_reset ());
#endif
  }

  void* alloc ()
  {
    m_mutex.enter_read ();

    Node* node = m_free.pop_front ();

    m_mutex.exit_read ();

    void* p;

    if (!node)
    {
      p = ::operator new (BlockSize + sizeof (Node)); // implicit global mutex

#if VF_CHECK_LEAKS
      m_total.addref ();
#endif
    }
    else
    {
      p = fromNode (node);
    }

#if VF_CHECK_LEAKS
    m_used.addref ();
#endif

    return p;
  }

  void free (void* p)
  {
    Node* node = toNode (p);

    m_mutex.enter_read ();

    m_junk.push_front (node);

    m_mutex.exit_read ();

#if VF_CHECK_LEAKS
    m_used.release ();
#endif

    // See if we need to perform garbage collection
    if (m_count.release ())
    {
      m_mutex.enter_write ();

      m_free.swap (m_junk);
  
      m_count.set (m_interval);

#if ALLOCATOR_COUNT_SWAPS
      String s;
      s << "swap " << String (++m_swaps);
      Logger::outputDebugString (s);
#endif

      m_mutex.exit_write ();
    }
  }

private:
  // In order to assure that allocated elements have the
  // same alignment as what we would get from a straight
  // new, the List::Node is placed just after the allocated
  // storage, rather than before. However this might cause
  // alignment issues for the List::Node itslf (which contains
  // an Atomic::Pointer that may need more strict alignment)

  struct Node;
  typedef Stack <Node> List;
  struct Node : List::Node
  {
  };

  static inline void* fromNode (Node* node)
  {
    return reinterpret_cast <char *> (node) - BlockSize;
  }

  static inline Node* toNode (void* p)
  {
    return reinterpret_cast <Node*> (reinterpret_cast <char *> (p) + BlockSize);
  }

  void free (List& list)
  {
    for (;;)
    {
      Node* node = list.pop_front ();

      if (node)
      {
        ::operator delete (fromNode (node)); // implicit global mutex

#if VF_CHECK_LEAKS
        m_total.release ();
#endif
      }
      else
      {
        break;
      }
    }
  }

private:
  List m_free;
  List m_junk;
  int m_interval;
  Atomic::Counter m_count;
  ReadWriteMutex m_mutex;

#if ALLOCATOR_COUNT_SWAPS
  int m_swaps;
#endif

#if VF_CHECK_LEAKS
  Atomic::Counter m_total;
  Atomic::Counter m_used;
#endif
};

typedef FixedAllocator <globalAllocatorBlockSize> GlobalAllocator; 
extern GlobalAllocator globalAllocator;

// Sugar hack since the compiler cannot infer
// template arguments based on the return type.
//
// Usage:
//
//  C* c = globalAlloc<C>::New (arg1, arg2,...);
//
template <class C>
struct globalAlloc
{
  static C* New ()
  {
    static_vfassert (sizeof (C) <= GlobalAllocator::blockSize);
    return new (globalAllocator.alloc()) C;
  }

  template <class T1>
  static C* New (const T1& t1)
  {
    static_vfassert (sizeof (C) <= GlobalAllocator::blockSize);
    return new (globalAllocator.alloc()) C (t1);
  }

  template <class T1, class T2>
  static C* New (const T1& t1, const T2& t2)
  {
    static_vfassert (sizeof (C) <= GlobalAllocator::blockSize);
    return new (globalAllocator.alloc()) C (t1, t2);
  }

  template <class T1, class T2, class T3>
  static C* New (const T1& t1, const T2& t2, const T3& t3)
  {
    static_vfassert (sizeof (C) <= GlobalAllocator::blockSize);
    return new (globalAllocator.alloc()) C (t1, t2, t3);
  }

  template <class T1, class T2, class T3, class T4>
  static C* New (const T1& t1, const T2& t2, const T3& t3, const T4& t4)
  {
    static_vfassert (sizeof (C) <= GlobalAllocator::blockSize);
    return new (globalAllocator.alloc()) C (t1, t2, t3, t4);
  }

  template <class T1, class T2, class T3, class T4, class T5>
  static C* New (const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5)
  {
    static_vfassert (sizeof (C) <= GlobalAllocator::blockSize);
    return new (globalAllocator.alloc()) C (t1, t2, t3, t4, t5);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6>
  static C* New (const T1& t1, const T2& t2, const T3& t3, const T4& t4, 
                 const T5& t5, const T6& t6)
  {
    static_vfassert (sizeof (C) <= GlobalAllocator::blockSize);
    return new (globalAllocator.alloc()) C (t1, t2, t3, t4, t5, t6);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6, class T7>
  static C* New (const T1& t1, const T2& t2, const T3& t3, const T4& t4, 
                 const T5& t5, const T6& t6, const T7& t7)
  {
    static_vfassert (sizeof (C) <= GlobalAllocator::blockSize);
    return new (globalAllocator.alloc()) C (t1, t2, t3, t4, t5, t6, t7);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
  static C* New (const T1& t1, const T2& t2, const T3& t3, const T4& t4, 
                 const T5& t5, const T6& t6, const T7& t7, const T8& t8)
  {
    static_vfassert (sizeof (C) <= GlobalAllocator::blockSize);
    return new (globalAllocator.alloc()) C (t1, t2, t3, t4, t5, t6, t7, t8);
  }
};

template <class C>
void globalDelete (C* c)
{
  c->~C();
  globalAllocator.free (c);
}

}

#endif