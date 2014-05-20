#!/bin/sh

VIS="-fvisibility=hidden"
CF="-ggdb3 -Wall -Wextra -O0 -D_DEBUG -D_GLIBCXX_DEBUG -fstack-protector"
CXF="-D_GLIBCXX_CONCEPT_CHECK -fvisibility-inlines-hidden"
qmake \
  CONFIG+="debug" \
  QMAKE_CXX="mpic++" \
  QMAKE_CFLAGS+="${CF}" \
  QMAKE_CXXFLAGS+="${VIS} ${CF} ${CXF}" \
  -recursive \
  Tuvok.pro || exit 1
nice make -j2