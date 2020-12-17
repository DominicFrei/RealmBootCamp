#!/bin/sh
set -e
if test "$CONFIGURATION" = "Debug"; then :
  cd /Users/dominic.frei/Repositories/capi2/build.debug
  make -f /Users/dominic.frei/Repositories/capi2/build.debug/CMakeScripts/ReRunCMake.make
fi
if test "$CONFIGURATION" = "Release"; then :
  cd /Users/dominic.frei/Repositories/capi2/build.debug
  make -f /Users/dominic.frei/Repositories/capi2/build.debug/CMakeScripts/ReRunCMake.make
fi
if test "$CONFIGURATION" = "MinSizeRel"; then :
  cd /Users/dominic.frei/Repositories/capi2/build.debug
  make -f /Users/dominic.frei/Repositories/capi2/build.debug/CMakeScripts/ReRunCMake.make
fi
if test "$CONFIGURATION" = "RelWithDebInfo"; then :
  cd /Users/dominic.frei/Repositories/capi2/build.debug
  make -f /Users/dominic.frei/Repositories/capi2/build.debug/CMakeScripts/ReRunCMake.make
fi

