
/*
 * Copyright 2010 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkPDFGraphicState_DEFINED
#define SkPDFGraphicState_DEFINED

#include "SkPaint.h"
#include "SkPDFTypes.h"
#include "SkTemplates.h"
#include "SkThread.h"

class SkPDFFormXObject;

/** \class SkPDFGraphicState
    SkPaint objects roughly correspond to graphic state dictionaries that can
    be installed. So that a given dictionary is only output to the pdf file
    once, we want to canonicalize them. Static methods in this class manage
    a weakly referenced set of SkPDFGraphicState objects: when the last
    reference to a SkPDFGraphicState is removed, it removes itself from the
    static set of objects.

*/
class SkPDFGraphicState : public SkPDFDict {
    SK_DECLARE_INST_COUNT(SkPDFGraphicState)
public:
    enum SkPDFSMaskMode {
        kAlpha_SMaskMode,
        kLuminosity_SMaskMode
    };

    virtual ~SkPDFGraphicState();

    // Override emitObject so that we can populate the dictionary on
    // demand.
    virtual void emitObject(SkWStream* stream, SkPDFCatalog* catalog);

    /** Get the graphic state for the passed SkPaint. The reference count of
     *  the object is incremented and it is the caller's responsibility to
     *  unreference it when done. This is needed to accommodate the weak
     *  reference pattern used when the returned object is new and has no
     *  other references.
     *  @param paint  The SkPaint to emulate.
     */
    static SkPDFGraphicState* GetGraphicStateForPaint(const SkPaint& paint);

    /** Make a graphic state that only sets the passed soft mask. The
     *  reference count of the object is incremented and it is the caller's
     *  responsibility to unreference it when done.
     *  @param sMask     The form xobject to use as a soft mask.
     *  @param invert    Indicates if the alpha of the sMask should be inverted.
     *  @param sMaskMode Whether to use alpha or luminosity for the sMask.
     */
    static SkPDFGraphicState* GetSMaskGraphicState(SkPDFFormXObject* sMask,
                                                   bool invert,
                                                   SkPDFSMaskMode sMaskMode);

    /** Get a graphic state that only unsets the soft mask. The reference
     *  count of the object is incremented and it is the caller's responsibility
     *  to unreference it when done. This is needed to accommodate the weak
     *  reference pattern used when the returned object is new and has no
     *  other references.
     */
    static SkPDFGraphicState* GetNoSMaskGraphicState();

    bool equals(const SkPaint&) const;

    // Only public for SK_DECLARE_STATIC_LAZY_PTR
    static SkPDFGraphicState* CreateNoSMaskGraphicState();

private:
    const SkPaint fPaint;
    bool fPopulated;
    bool fSMask;

    SkPDFGraphicState();
    explicit SkPDFGraphicState(const SkPaint& paint);

    void populateDict();

    typedef SkPDFDict INHERITED;
};

#endif
