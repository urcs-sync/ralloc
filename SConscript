import os
import sys
sys.path.append('%s/library' % (Dir('#').abspath))

Import('mainEnv')
buildEnv = mainEnv.Clone()

buildEnv.Append(CCFLAGS = ' -O3 -g -DMEM_CONSUME_TEST -DDESTROY -fPIC')

buildEnv.Append(CPPPATH = ['src'])

buildEnv.Append(CCFLAGS='-DPWB_IS_CLFLUSH')

C_SRC = Split("""
              src/SizeClass.cpp
              src/RegionManager.cpp
              src/TCache.cpp
              src/BaseMeta.cpp
              src/rpmalloc.cpp
              """)

SRC = C_SRC

rpmallocLibrary = buildEnv.StaticLibrary('rpmalloc', SRC)
Return('rpmallocLibrary')
