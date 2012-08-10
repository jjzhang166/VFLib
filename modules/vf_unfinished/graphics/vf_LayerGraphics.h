/*============================================================================*/
/*
  VFLib: https://github.com/vinniefalco/VFLib

  Copyright (C) 2008 by Vinnie Falco <vinnie.falco@gmail.com>

  This library contains portions of other open source products covered by
  separate licenses. Please see the corresponding source files for specific
  terms.
  
  VFLib is provided under the terms of The MIT License (MIT):

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/
/*============================================================================*/

#ifndef VF_LAYERGRAPHICS_VFHEADER
#define VF_LAYERGRAPHICS_VFHEADER

/** LayerGraphics Image container base.

    This holds the image and bounding rectangle used for a LayerGraphics
    context, to resolve the order of construction issues.

    @ingroup vf_gui

    @internal
*/
class LayerGraphicsBase : vf::Uncopyable
{
protected:
  LayerGraphicsBase (Graphics& g, Rectangle <int> const& fillBounds);

protected:
  Image m_base;
  Image m_fill;
  Image m_work;
  Point <int> m_workOrigin;       // relative to m_base
  Rectangle <int> m_fillBounds;   // relative to m_base
  Point <int> m_fillOrigin;       // relative to m_fill
};

/** A @ref Graphics context for a compositing layer.

    This allows you to apply layer styles to anything that can be drawn into
    a @ref Graphics context. The @ref ComponentPeer must use the software
    renderer.

    @ingroup vf_gui
*/
class LayerGraphics : public LayerGraphicsBase, public Graphics
{
public:
  struct Options
  {
    //----

    struct General
    {
      General ()
        : opacity (1)
        , groupInteriorEffects (false)
      {
      }

      double    opacity;                // [0, 1] of fill
      bool      groupInteriorEffects;
    };

    //----

    struct Fill
    {
      Fill ()
        : mode (BlendMode::modeNormal)
        , opacity (1)
      {
      }

      BlendMode::Type mode;
      double opacity;                   // [0, 1] overall
    };

    //----

    struct DropShadow
    {
      DropShadow ()
        : active (false)
        , mode (BlendMode::modeNormal)
        , opacity (1)
        , colour (Colours::black)
        , angle (0)
        , distance (1)
        , spread (0)
        , size (1)
        , knockout (true)
      {
        active = false;
      }

      bool      active;
      BlendMode::Type mode;
      double    opacity;
      Colour    colour;                 // can have alpha
      double    angle;                  // radians
      int       distance;               // [0, 30000]
      double    spread;                 // [0, 1]
      int       size;                   // in pixels
      bool      knockout;               // layer mask knocks out drop shadow
    };

    //----

    struct InnerShadow
    {
      InnerShadow ()
        : active (false)
        , mode (BlendMode::modeNormal)
        , colour (Colours::black)
        , angle (0)
        , distance (1)
        , choke (0)
        , size (1)
      {
      }

      bool      active;
      BlendMode::Type mode;
      double    opacity;
      Colour    colour;                 // can have alpha
      double    angle;                  // radians
      int       distance;               // [0, 30000]
      double    choke;                  // [0, 1]
      int       size;                   // in pixels
    };

    //----

    General       general;
    Fill          fill;
    DropShadow    dropShadow;
    InnerShadow   innerShadow;

    BevelEmbossStyle::Options       bevelEmboss;
    GradientOverlayStyle::Options   gradientOverlay;
  };

public:
  /** Create a layer graphics context.

      @param fillBounds The smallest rectangle which encloses all pixels in
                        the fill which are not fully transparent. The
             coordinates are relative to the origin.
  */
  LayerGraphics (Graphics& g, Rectangle <int> const& fillBounds);

  ~LayerGraphics ();

  Options& getOptions ();

private:
  void applyDropShadow (Image& destImage);

  void applyInnerShadow (Image& destImage);

  void applyFill ();

private:
  Options m_options;
};

#endif