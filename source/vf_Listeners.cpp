// Copyright (C) 2008-2011 by One Guy Group, Inc., All rights reserved worldwide.

#include "vf/vf_StandardHeader.h"

BEGIN_VF_NAMESPACE

#include "vf/vf_Listeners.h"

//------------------------------------------------------------------------------
//
// ReadWriteMutex
//
//------------------------------------------------------------------------------

namespace detail {

//
// ReadWriteMutex:
//
// - Multiple reader, single writer
// - Write preferenced (writers starve readers)
// - Owner of the write lock can nest unlimited balanced
//   calls to read and write
// - A read lock may be promoted to a write lock
// - A write lock may be demoted to a read lock
//
// TODO:
// THIS CAN BE OPTIMIZED FOR HEAVY READ LOAD by
// doing atomic compare and set tests first, and
// then doing more expensive locking depending on the result
//
#if 0
Listeners::ReadWriteMutex::ReadWriteMutex ()
  : m_readers (0)
  , m_event (true)            // manual reset
{
  m_event.signal ();          // initally signaled
}

Listeners::ReadWriteMutex::~ReadWriteMutex ()
{
  m_event.wait ();
  vfassert (m_readers == 0);
}

void Listeners::ReadWriteMutex::enter_read ()
{
  m_write_mutex.enter ();       // block other threads
  m_mutex.enter ();             // protect state
  if (m_readers > 0)            // there's already readers
    ++m_readers;                // join them
  else if (m_readers == 0) {    // we're the first reader
    m_readers = 1;              // set us as first
    m_event.reset ();           // block writers
  } else                        // we are a writer nesting read
    --m_readers;                // add a nesting level
  m_mutex.exit ();
  m_write_mutex.exit ();
}

void Listeners::ReadWriteMutex::enter_write ()
{
  m_write_mutex.enter ();       // block other threads
  m_event.wait ();              // wait for last reader if any
  vfassert (m_readers <= 0);
  --m_readers;                  // add a nesting
}

void Listeners::ReadWriteMutex::promote_to_write ()
{
  m_write_mutex.enter ();       // block other threads
  m_mutex.enter ();             // protect state
  vfassert (m_readers != 0);     // unbalanced call
  if (m_readers > 0) {          // we are a true reader
    if (--m_readers == 0) {     // we are the last reader
      m_event.signal ();        // unblock future writers
      m_mutex.exit ();
    } else {
      m_mutex.exit (); 
      m_event.wait (); } }      // wait for last reader
  vfassert (m_readers <= 0);
  --m_readers;                  // add a nesting
}

void Listeners::ReadWriteMutex::exit_read ()
{
  m_mutex.enter ();             // protect state
  vfassert (m_readers != 0);     // unbalanced call
  if (m_readers > 0) {          // we are a true reader
    if (--m_readers == 0)       // we are the last reader
      m_event.signal ();        // unblock writers
  } else {                      // we are a nested writer
    ++m_readers;                // remove a nesting
    vfassert (m_readers != 0); }
  m_mutex.exit ();
}

void Listeners::ReadWriteMutex::exit_write ()
{
  vfassert (m_readers < 0);      // unbalanced call
  ++m_readers;                  // remove a nesting
  m_write_mutex.exit ();
}

void Listeners::ReadWriteMutex::demote_to_read ()
{
  m_mutex.enter ();             // protect state
  vfassert (m_readers < 0);      // unbalanced call
  if (m_readers == -1) {        // last nested write
    m_readers = 1;              // become a reader
    m_event.reset ();           // block writers
  } else                        // we are a nested writer
    ++m_readers;                // remove a nesting
  m_mutex.exit ();
  m_write_mutex.exit ();
}
#else
Listeners::ReadWriteMutex::ReadWriteMutex ()
{
}

Listeners::ReadWriteMutex::~ReadWriteMutex ()
{
}

void Listeners::ReadWriteMutex::enter_read ()
{
  m_mutex.enter ();
}

void Listeners::ReadWriteMutex::enter_write ()
{
  m_mutex.enter ();
}

void Listeners::ReadWriteMutex::exit_read ()
{
  m_mutex.exit ();
}

void Listeners::ReadWriteMutex::exit_write ()
{
  m_mutex.exit ();
}

void Listeners::ReadWriteMutex::promote_to_write ()
{
}

void Listeners::ReadWriteMutex::demote_to_read ()
{
}

#endif

//------------------------------------------------------------------------------
//
// Group
//
//------------------------------------------------------------------------------

//
// Group
//
// Maintains a protected list of listeners called on the same thread queue.
//
// The list of groups is not evaluated until the Group receives its call
// on the thread queue.
//
// It is possible for the group to become empty by the time it executes
// its call. In this case it will release its last reference and get deleted.
//

Listeners::Group::Group (Worker* worker)
  : m_listener (0)
  , m_worker (worker)
{
}

Listeners::Group::~Group ()
{
  // If this goes off it means a Listener forgot to remove.
  vfassert (m_list.empty());

  // shouldn't be deleting group during a call
  vfassert (m_listener == 0);
}

// Adds the listener to the group.
// The caller must prevent duplicates.
void Listeners::Group::add (void* listener,
                            const timestamp_t timestamp)
{
  m_mutex.enter ();

  vfassert (!contains (listener));

  // Should never be able to get here while in do_call()
  vfassert (m_listener == 0);

  // Add the listener and remember the time stamp so we don't
  // send it calls that were queued earlier than the add().
  Entry entry;
  entry.listener = listener;
  entry.timestamp = timestamp;
  m_list.push_back (entry);

  m_mutex.exit ();
}

// Removes the listener from the group if it exists.
// Returns true if the listener was removed.
bool Listeners::Group::remove (void* listener)
{
  bool found = false;

  m_mutex.enter ();

  // Should never be able to get here while in do_call()
  vfassert (m_listener == 0);

  for (list_t::iterator iter = m_list.begin(); iter != m_list.end(); ++iter)
  {
    if (iter->listener == listener)
    {
      m_list.erase (iter);
      found = true;
      break;
    }
  }

  m_mutex.exit ();

  return found;
}

// Used for assertions.
// The caller must synchronize.
bool Listeners::Group::contains (void const* listener) const
{
  bool found = false;

  for (list_t::const_iterator iter = m_list.begin(); iter != m_list.end(); iter++)
  {
    if (iter->listener == listener)
    {
      found = true;
      break;
    }
  }

  return found;
}

// Add the group to its thread queue with the given Call.
void Listeners::Group::queue_call (Call::Ptr c)
{
  // Caller shouldn't know about us if we're empty.
  vfassert (!empty ());

  // The Group::Ptr cast is required to maintain the reference count,
  // because boost::bind peforms the conversion from Group* to
  // to Group::Ptr at the point of call instead of bind time.
  //
  // NOTE: do_call() may execute synchronously.
  //
  m_worker->call (&Group::do_call, this, c, Group::Ptr (this));
}

// Queues a reference to the Call on the thread queue of each listener
// that is currently in our list. The thread queue must be in the
// stack's call chain, either directly from Worker::process(),
// or from Proxy::do_call() called from Worker::process().
//
// The unnamed parameter exists to maintain a reference to the group
// while it travels through the thread queue.
//
void Listeners::Group::do_call (Call::Ptr c, Group::Ptr)
{
  if (!empty ())
  {
    m_mutex.enter ();

    // Recursion not allowed.
    vfassert (m_listener == 0);

    // The body of the loop MUST NOT cause listeners to get called.
    // Therefore, we don't have to worry about listeners removing
    // themselves while iterating the list.
    //
    for (list_t::const_iterator iter = m_list.begin(); iter != m_list.end();)
    {
      Entry const& entry = *iter++;

      // Since it is possible for a listener to be added after a
      // Call gets queued but before it executes, this prevents listeners
      // from seeing Calls created before they were added.
      //
      if (entry.came_before (c))
      {
        m_listener = entry.listener;

        // The thread queue's process() function MUST be in our call
        // stack to guarantee that these calls will not execute immediately.
        // They will be handled by the tail recusion unrolling in the
        // thread queue.
        vfassert (m_worker->in_process ());

        m_worker->call (&Call::do_call, c, m_listener);

        m_listener = 0;
      }
    }

    m_mutex.exit ();
  }
  else
  {
    // last listener was removed before we got here,
    // and the parent listener list may have been deleted.
  }
}

//------------------------------------------------------------------------------
//
// Proxy
//
//------------------------------------------------------------------------------

//
// Proxy
//
// This object acts as a proxy for a contained Call and gets put on Group
// thread queues. The proxy is kept in a list so if new calls are made
// for the same member before the existing calls execute, the existing
// calls can be replaced.
//
// Once a Proxy is created it lives for the lifetime of the listeners.
//
Listeners::Proxy::Proxy ()
{
}

Listeners::Proxy::~Proxy ()
{
  // If the proxy is getting destroyed it means:
  // - the listeners object is getting destroyed
  // - all listeners must have removed themselves
  // - all thread queues have been fully processed
  // Therefore, our entries should be gone.

  // NO it is possible for an empty Group, for which
  // the parent listeners object has been destroyed,
  // to still exist in a thread queue!!!

  // But all listeners should have removed themselves
  // so our list of groups should still be empty.
  vfassert (m_entries.empty ());
}

// Adds the group to the Proxy.
// Caller must have the proxies mutex.
// Caller is responsible for preventing duplicates.
void Listeners::Proxy::add (Group::Ptr group)
{
  Entry::Ptr entry (new Entry (group));

  // Manual addref and put raw pointer in list
  entry.getObject()->incReferenceCount ();
  m_entries.push_back (entry);
}

// Removes the group from the Proxy.
// Caller must have the proxies mutex.
// Caller is responsible for making sure the group exists.
void Listeners::Proxy::remove (Group::Ptr group)
{
  for (Entries::iterator iter = m_entries.begin(); iter != m_entries.end(); ++iter)
  {
    Entry::Ptr entry = *iter++;

    if (entry->group == group)
    {
      // remove from list and manual release
      m_entries.remove (entry.getObject());
      entry.getObject()->decReferenceCount();

      // Entry might still be in the empty group's thread queue
      break;
    }
  }
}

// Updates m_call and queues do_call for each Group
// in our list that isn't already in the queue.
// Caller must acquire the read lock.
void Listeners::Proxy::do_calls (Call::Ptr c)
{
  // why would we even want to be called?
  vfassert (!m_entries.empty());

  // With the read lock, this list can't change on us unless someone
  // adds a listener to a new thread queue in response to a call.
  for (Entries::iterator iter = m_entries.begin(); iter != m_entries.end();)
  {
    Entry::Ptr entry = *iter++;

    // Manually add a reference since we use a raw pointer
    c.getObject()->incReferenceCount ();

    // Atomically exchange the new call for the old one
    Call* old = entry->call.exchange (c.getObject());

    // If no old call then they need to be requeued
    if (!old)
      entry->group->getWorker()->call (&Proxy::do_call, this, entry);
    else
      old->decReferenceCount ();
  }
}

void Listeners::Proxy::do_call (Entry::Ptr entry)
{
  // Atomically acquire the call, which also serves as the queued flag
  Call::Ptr c = entry->call.exchange (0);

  vfassert (c);

  Group::Ptr group = entry->group;

  if (!group->empty ())
  {
    group->do_call (c, group);
  }
  else
  {
    // Entry will get a final release
  }

  // Remove the manual reference that we added because of the raw pointer
  c.getObject()->decReferenceCount ();
}

//------------------------------------------------------------------------------
//
// Listeners
//
//------------------------------------------------------------------------------

Listeners::Listeners ()
  : m_timestamp (0)
{
}

Listeners::~Listeners ()
{
  for (Groups::iterator iter = m_groups.begin (); iter != m_groups.end ();)
  {
    Group* group = *iter++;

    // If this goes off it means a Listener forgot to remove.
    vfassert (group->empty ());

#ifdef JUCE_DEBUG
    const bool final = group->decReferenceCount ();
    vfassert (final);
#else
    group->decReferenceCount ();
#endif
  }

  // Proxies are never deleted until here.
  for (Proxies::iterator iter = m_proxies.begin(); iter != m_proxies.end ();)
  {
    Proxy* proxy = *iter++;
    delete proxy;
  }
}

// Searches for a proxy that matches the pointer to member.
// Caller synchronizes.
Listeners::Proxy*
  Listeners::find_proxy (const void* member, int bytes)
{
  Proxy* proxy = 0;

  for (Proxies::iterator iter = m_proxies.begin (); iter != m_proxies.end (); ++iter)
  {
    if ((*iter)->match (member, bytes))
    {
      proxy = *iter;
      break;
    }
  }

  return proxy;
}

// Puts the Call on each existing group's queue.
// The caller must acquire the read mutex, but we release it.
// It is possible that the call will execute immediately.
void Listeners::queue_call (Call::Ptr c)
{
  // can't be const iterator because queue_call might cause called functors
  // to modify the list.
  for (Groups::iterator iter = m_groups.begin(); iter != m_groups.end();)
  {
    Group::Ptr group = *iter++;
    group->queue_call (c);
  }

  m_groups_mutex.exit_read ();
}

void Listeners::add_void (void* const listener, Worker* worker)
{
  // Take the write lock for groups.
  m_groups_mutex.enter_write ();

#ifdef JUCE_DEBUG
  // Make sure the listener has not already been added
  // SHOULD USE const_iterator!
  for (Groups::iterator iter = m_groups.begin(); iter != m_groups.end();)
  {
    Group* group = *iter++;

    // We can be in do_call() on another thread now, but it
    // doesn't modify the list, and we have the write lock.
    vfassert (!group->contains (listener));
  }
#endif

  // See if we already have a Group for this thread queue.
  Group::Ptr group;

  // SHOULD USE const_iterator
  for (Groups::iterator iter = m_groups.begin(); iter != m_groups.end();)
  {
    Group::Ptr cur = *iter++;
    if (cur->getWorker() == worker)
    {
      group = cur;
      break;
    }
  }

  if (!group)
  {
    group = new Group (worker);

    // Add it to the list, and give it a manual ref
    // since the list currently uses raw pointers.
    group->incReferenceCount ();
    m_groups.push_back (group);

    // Tell existing proxies to add the group
    m_proxies_mutex.enter_read ();
    for (Proxies::iterator iter = m_proxies.begin (); iter != m_proxies.end ();)
    {
      Proxy* proxy = *iter++;
      proxy->add (group);
    }
    m_proxies_mutex.exit_read ();
  }

  // Add the listener to the group with the current timestamp
  group->add (listener, m_timestamp);

  // Increment the timestamp within the mutex so
  // future calls will be newer than this listener.
  ++m_timestamp;

  m_groups_mutex.exit_write ();
}

void Listeners::remove_void (void* const listener)
{
  // Take the write lock for groups.
  m_groups_mutex.enter_write ();

  // Make sure the listener exists
#ifdef JUCE_DEBUG
  {
    bool exists = false;

    for (Groups::iterator iter = m_groups.begin(); iter != m_groups.end();)
    {
      Group* group = *iter++;

      // this should never happen while we hold the mutex
      vfassert (!group->empty ());

      if (group->contains (listener))
      {
        vfassert (!exists); // added twice?

        exists = true;
        // keep going to make sure there are no empty groups
      }
    }

    vfassert (exists);
  }
#endif

  // Find the group and remove
  for (Groups::iterator iter = m_groups.begin(); iter != m_groups.end();)
  {
    Group::Ptr group = *iter++;

    // If the listener is in there, take it out.
    if (group->remove (listener))
    {
      // Are we the last listener?
      if (group->empty ())
      {
        // Tell proxies to remove the group
        m_proxies_mutex.enter_read ();
        for (Proxies::iterator iter = m_proxies.begin (); iter != m_proxies.end ();)
        {
          Proxy* proxy = *iter++;
          proxy->remove (group);
        }
        m_proxies_mutex.exit_read ();

        // Remove it from the list and manually release
        // the reference since the list uses raw pointers.
        m_groups.remove (group.getObject());
        group->decReferenceCount();

        // It is still possible for the group to exist at this
        // point in a thread queue but it will get processed,
        // do nothing, and release its own final reference.
      }

      break;
    }
  }

  m_groups_mutex.exit_write ();
}

}

END_VF_NAMESPACE

