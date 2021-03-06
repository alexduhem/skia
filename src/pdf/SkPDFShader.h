
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkPDFShader_DEFINED
#define SkPDFShader_DEFINED

#include "SkPDFResourceDict.h"
#include "SkPDFStream.h"
#include "SkPDFTypes.h"

class SkMatrix;
class SkShader;
struct SkIRect;

/** \class SkPDFShader

    In PDF parlance, this is a pattern, used in place of a color when the
    pattern color space is selected.
*/

class SkPDFShader {
public:
    class State;

    /** Get the PDF shader for the passed SkShader. If the SkShader is
     *  invalid in some way, returns NULL. The reference count of
     *  the object is incremented and it is the caller's responsibility to
     *  unreference it when done.  This is needed to accommodate the weak
     *  reference pattern used when the returned object is new and has no
     *  other references.
     *  @param shader      The SkShader to emulate.
     *  @param matrix      The current transform. (PDF shaders are absolutely
     *                     positioned, relative to where the page is drawn.)
     *  @param surfceBBox  The bounding box of the drawing surface (with matrix
     *                     already applied).
     *  @param rasterScale Additional scale to be applied for early
     *                     rasterization.
     */
    static SkPDFObject* GetPDFShader(const SkShader& shader,
                                     const SkMatrix& matrix,
                                     const SkIRect& surfaceBBox,
                                     SkScalar rasterScale);
};

class SkPDFFunctionShader : public SkPDFDict {
    SK_DECLARE_INST_COUNT(SkPDFFunctionShader);

public:
    static SkPDFFunctionShader* Create(SkAutoTDelete<SkPDFShader::State>*);
    virtual ~SkPDFFunctionShader();
    bool equals(const SkPDFShader::State&) const;

private:
    SkAutoTDelete<const SkPDFShader::State> fShaderState;
    SkTDArray<SkPDFObject*> fResources;
    explicit SkPDFFunctionShader(SkPDFShader::State* state);
    typedef SkPDFDict INHERITED;
};

/**
 * A shader for PDF gradients. This encapsulates the function shader
 * inside a tiling pattern while providing a common pattern interface.
 * The encapsulation allows the use of a SMask for transparency gradients.
 */
class SkPDFAlphaFunctionShader : public SkPDFStream {
public:
    static SkPDFAlphaFunctionShader* Create(SkAutoTDelete<SkPDFShader::State>*);
    virtual ~SkPDFAlphaFunctionShader();
    bool equals(const SkPDFShader::State&) const;

private:
    SkAutoTDelete<const SkPDFShader::State> fShaderState;
    SkAutoTUnref<SkPDFObject> fColorShader;
    SkAutoTUnref<SkPDFResourceDict> fResourceDict;
    explicit SkPDFAlphaFunctionShader(SkPDFShader::State* state);
};

class SkPDFImageShader : public SkPDFStream {
public:
    static SkPDFImageShader* Create(SkAutoTDelete<SkPDFShader::State>*);
    virtual ~SkPDFImageShader();
    bool equals(const SkPDFShader::State&) const;

private:
    SkAutoTDelete<const SkPDFShader::State> fShaderState;
    SkTSet<SkPDFObject*> fResources;
    explicit SkPDFImageShader(SkPDFShader::State* state);
};

#endif
