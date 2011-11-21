/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/**
  \file    GLTexture1D.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/
#pragma once

#ifndef GLTEXTURE1D_H
#define GLTEXTURE1D_H

#include "../../StdTuvokDefines.h"
#include "GLTexture.h"

namespace tuvok {

class GLTexture1D : public GLTexture {
  public:
    GLTexture1D(UINT32 iSize, GLint internalformat, GLenum format, GLenum type,
          UINT32 iSizePerElement,
          const void *pixels = 0,
          GLint iMagFilter = GL_NEAREST,
          GLint iMinFilter = GL_NEAREST,
          GLint wrap = GL_CLAMP_TO_EDGE);
    virtual ~GLTexture1D() {}

    virtual void Bind(UINT32 iUnit=0) const {
      GLint iPrevUint;
      GL(glGetIntegerv(GL_ACTIVE_TEXTURE, &iPrevUint));

      GL(glActiveTexture(GLenum(GL_TEXTURE0 + iUnit)));
      GL(glBindTexture(GL_TEXTURE_1D, m_iGLID));

      GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, m_iMagFilter));
      GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, m_iMinFilter));

      GL(glActiveTexture(iPrevUint));
    }
    virtual void SetData(const void *pixels, bool bRestoreBinding=true);
    void SetData(UINT32 offset, UINT32 size, const void *pixels, bool bRestoreBinding=true);

    virtual UINT64 GetCPUSize() {return UINT64(m_iSize*m_iSizePerElement);}
    virtual UINT64 GetGPUSize() {return UINT64(m_iSize*m_iSizePerElement);}

    UINT32 GetSize() const {return UINT32(m_iSize);}

  protected:
    GLuint m_iSize;
    GLint  m_internalformat;
    GLenum m_format;
    GLenum m_type;
};
};
#endif // GLTEXTURE1D_H
