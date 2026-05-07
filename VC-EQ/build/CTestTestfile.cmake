# CMake generated Testfile for 
# Source directory: /tmp/AudioFX/VC-EQ
# Build directory: /tmp/AudioFX/VC-EQ/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[VC-EQ-DSP-Tests]=] "/tmp/AudioFX/VC-EQ/build/tests/VC-EQ-DSP-Tests")
set_tests_properties([=[VC-EQ-DSP-Tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/tmp/AudioFX/VC-EQ/CMakeLists.txt;36;add_test;/tmp/AudioFX/VC-EQ/CMakeLists.txt;0;")
subdirs("JUCE-build")
