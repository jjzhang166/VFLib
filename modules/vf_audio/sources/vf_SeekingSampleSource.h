/*============================================================================*/
/*
  Copyright (C) 2008 by Vinnie Falco, this file is part of VFLib.
  See the file GNU_GPL_v2.txt for full licensing terms.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 51
  Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
/*============================================================================*/

#ifndef VF_SEEKINGSAMPLESOURCE_VFHEADER
#define VF_SEEKINGSAMPLESOURCE_VFHEADER

//==============================================================================
/**
  Abstract positionable source of audio samples.

  This interface adds positionable capabilities to a SampleSource. It is
  intended as a facade for @ref PositionableAudioSource, with these features:

  - No thread safety; the caller is responsible for all synchronization.

  - The looping and total length features have been removed.

  @ingroup vf_audio
*/
class SeekingSampleSource : public SampleSource
{
public:
  /**
    Move the read position.

    Calling this indicates that the next call to SampleSource::getNextAudioBlock()
    should return samples from this position.
  */
  virtual void setNextReadPosition (int64 newPosition) = 0;

  /**
    Returns the position from which the next block will be returned.

    @see setNextReadPosition
  */
  virtual int64 getNextReadPosition () const = 0;
};

#endif