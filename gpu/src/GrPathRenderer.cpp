
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrPathRenderer.h"

GrPathRenderer::GrPathRenderer()
    : fCurveTolerance (GR_Scalar1)
    , fPath(NULL)
    , fTarget(NULL) {
}

void GrPathRenderer::setPath(GrDrawTarget* target,
                             const SkPath* path,
                             GrPathFill fill,
                             const GrPoint* translate) {
    GrAssert(NULL == fPath);
    GrAssert(NULL == fTarget);
    GrAssert(NULL != target);

    fTarget = target;
    fPath = path;
    fFill = fill;
    if (NULL != translate) {
        fTranslate = *translate;
    } else {
        fTranslate.fX = fTranslate.fY = 0;
    }
    this->pathWasSet();
}

void GrPathRenderer::clearPath() {
    this->pathWillClear();
    fTarget->resetVertexSource();
    fTarget->resetIndexSource();
    fTarget = NULL;
    fPath = NULL;
}
