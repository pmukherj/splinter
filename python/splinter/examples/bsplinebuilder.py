# This file is part of the SPLINTER library.
# Copyright (C) 2012 Bjarne Grimstad (bjarne.grimstad@gmail.com).
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Add the SPLINTER directory to the search path, so we can include it
from os import sys, path, remove
sys.path.append(path.dirname(path.dirname(path.dirname(path.abspath(__file__)))))


##### Start of the example #####
import splinter

splinter.load("/home/anders/SPLINTER/build/release/libsplinter-2-0.so")


d = splinter.DataTable()

for x0 in range(10):
    for x1 in range(10):
        d.addSample([x0, x1], x0*x1)

print(d.getNumVariables())
builder = splinter.BSplineBuilder(d)


builder.degree([splinter.BSplineBuilder.Degree.LINEAR, splinter.BSplineBuilder.Degree.QUARTIC])
builder.numBasisFunctions([10 ** 3, 10 ** 2])
import math
builder.knotSpacing(splinter.BSplineBuilder.KnotSpacing.EQUIDISTANT)
builder.smoothing(splinter.BSplineBuilder.Smoothing.NONE)
builder.setLambda(math.pi)

bspline = builder.build()


for x0 in range(10):
    for x1 in range(10):
        pass#print(str(bspline.eval([x0, x1])) + " ?= " + str(x0*x1))