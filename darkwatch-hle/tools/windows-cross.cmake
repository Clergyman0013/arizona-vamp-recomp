# Cross build for Windows x64 from macOS or Linux: Homebrew/upstream LLVM (clang, lld-link, llvm-rc) and the
# MSVC CRT + Windows SDK that `xwin splat` writes. Used by tools/build_win_cross.sh.
#   XWIN = the splat folder (crt/ and sdk/ in it), default ~/.xwin-cache/sdk
#   LLVM = LLVM's bin folder, default /opt/homebrew/opt/llvm/bin
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT DEFINED ENV{XWIN})
    set(ENV{XWIN} "$ENV{HOME}/.xwin-cache/sdk")
endif()
if(NOT DEFINED ENV{LLVM})
    set(ENV{LLVM} "/opt/homebrew/opt/llvm/bin")
endif()
set(XWIN $ENV{XWIN})
set(LLVM $ENV{LLVM})

set(triple x86_64-pc-windows-msvc)
set(CMAKE_C_COMPILER ${LLVM}/clang)
set(CMAKE_CXX_COMPILER ${LLVM}/clang++)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})
set(CMAKE_RC_COMPILER ${LLVM}/llvm-rc)
set(CMAKE_AR ${LLVM}/llvm-ar)
set(CMAKE_RANLIB ${LLVM}/llvm-ranlib)
# lld-link from Homebrew's lld when LLVM's bin has none
find_program(LLD_LINK lld-link PATHS ${LLVM} /opt/homebrew/opt/lld/bin REQUIRED)
get_filename_component(LLD_DIR ${LLD_LINK} DIRECTORY)
set(CMAKE_LINKER ${LLD_LINK})

# after clang's own headers, so its intrinsics win over the MSVC declarations
set(sys "-idirafter ${XWIN}/crt/include -idirafter ${XWIN}/sdk/include/ucrt -idirafter ${XWIN}/sdk/include/um -idirafter ${XWIN}/sdk/include/shared -idirafter ${XWIN}/sdk/include/winrt")
set(CMAKE_C_FLAGS_INIT "-march=x86-64-v2 ${sys}")
set(CMAKE_CXX_FLAGS_INIT "-march=x86-64-v2 ${sys}")
set(CMAKE_RC_FLAGS_INIT "-I ${XWIN}/sdk/include/um -I ${XWIN}/sdk/include/shared")
set(libs "-B${LLD_DIR} -L${XWIN}/crt/lib/x86_64 -L${XWIN}/sdk/lib/um/x86_64 -L${XWIN}/sdk/lib/ucrt/x86_64")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${libs}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${libs}")
# static CRT: the exe needs no Visual C++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)

set(CMAKE_FIND_ROOT_PATH ${XWIN})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
