import os
import sys
sys.path.append('%s/library' % (Dir('#').abspath))

Import('mainEnv')
buildEnv = mainEnv.Clone()

buildEnv.Append(CCFLAGS = ' -O3 -g -DRALLOC -DDESTROY -fPIC')

buildEnv.Append(CPPPATH = ['src'])

# buildEnv.Append(CCFLAGS='-DPWB_IS_CLFLUSH')

C_SRC = Split("""
              src/SizeClass.cpp
              src/RegionManager.cpp
              src/TCache.cpp
              src/BaseMeta.cpp
              src/ralloc.cpp
              """)

SRC = C_SRC

rallocLibrary = buildEnv.StaticLibrary('ralloc', SRC)
Return('rallocLibrary')
