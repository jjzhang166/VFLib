// Copyright (C) 2008-2011 by Vincent Falco, All rights reserved worldwide.
// This file is released under the MIT License:
// http://www.opensource.org/licenses/mit-license.php

#ifndef __VF_MUTEX_VFHEADER__
#define __VF_MUTEX_VFHEADER__

#include "vf/vf_MutexBase.h"
#include "vf/vf_BoostMutex.h"
#include "vf/vf_JuceMutex.h"

// Lift one implementation

#if VF_HAVE_JUCE

using Juce::Mutex;

#elif VF_HAVE_BOOST

using Boost::Mutex;

#else

#error "Missing Mutex"

#endif

typedef detail::ScopedLock <Mutex> ScopedLock;
typedef detail::ScopedUnlock <Mutex> ScopedUnlock;

#endif
