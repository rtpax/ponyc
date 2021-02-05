cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(libponyc.benchmarks VERSION ${PONYC_PROJECT_VERSION} LANGUAGES CXX)

add_executable(libponyc.benchmarks
    gbenchmark_main.cc
    common/common.cc
)

target_include_directories(libponyc.benchmarks
    PRIVATE ../../src/common
    PRIVATE ../../src/libponyc
    PRIVATE ../../build/libs/include
)

llvm_config(libponyc.benchmarks all)

target_link_directories(libponyc.benchmarks
    PRIVATE ../../build/libs/lib
)

target_link_libraries(libponyc.benchmarks
    PRIVATE libponyc
    PRIVATE libponyrt
    PRIVATE blake2
    PRIVATE benchmark::benchmark
)

if (MSVC)
    target_link_libraries(libponyc.benchmarks
        PRIVATE dbghelp
        PRIVATE ucrt
        PRIVATE Ws2_32
        PRIVATE Shlwapi
    )

    get_target_property(TARGET_LIBRARIES libponyc.benchmarks LINK_LIBRARIES)
    LIST(REMOVE_ITEM TARGET_LIBRARIES LTO)
    set_property(TARGET libponyc.benchmarks PROPERTY LINK_LIBRARIES ${TARGET_LIBRARIES})
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(libponyc.benchmarks PRIVATE dl z LLVMXRay LLVMWindowsManifest LLVMTableGen LLVMSymbolize LLVMDebugInfoPDB LLVMOrcJIT LLVMOrcError LLVMJITLink LLVMObjectYAML LLVMMIRParser LLVMMCA LLVMLTO LLVMPasses LLVMCoroutines LLVMObjCARCOpts LLVMipo LLVMInstrumentation LLVMVectorize LLVMLinker LLVMIRReader LLVMAsmParser LLVMFrontendOpenMP LLVMExtensions LLVMLineEditor LLVMLibDriver LLVMInterpreter LLVMFuzzMutate LLVMMCJIT LLVMExecutionEngine LLVMRuntimeDyld LLVMDWARFLinker LLVMDlltoolDriver LLVMOption LLVMDebugInfoGSYM LLVMCoverage LLVMAArch64Disassembler LLVMAArch64CodeGen LLVMAArch64AsmParser LLVMAArch64Desc LLVMAArch64Utils LLVMAArch64Info LLVMX86Disassembler LLVMX86CodeGen LLVMX86AsmParser LLVMX86Desc LLVMX86Info LLVMARMDisassembler LLVMARMCodeGen LLVMCFGuard LLVMGlobalISel LLVMSelectionDAG LLVMAsmPrinter LLVMDebugInfoDWARF LLVMCodeGen LLVMTarget LLVMScalarOpts LLVMInstCombine LLVMAggressiveInstCombine LLVMTransformUtils LLVMBitWriter LLVMAnalysis LLVMProfileData LLVMObject LLVMTextAPI LLVMBitReader LLVMCore LLVMRemarks LLVMBitstreamReader LLVMARMAsmParser LLVMMCParser LLVMARMDesc LLVMMCDisassembler LLVMMC LLVMDebugInfoCodeView LLVMDebugInfoMSF LLVMBinaryFormat LLVMARMUtils LLVMARMInfo LLVMSupport LLVMDemangle)
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD|DragonFly")
    target_link_libraries(libponyc.benchmarks PRIVATE execinfo)
else()
    target_link_libraries(libponyc.benchmarks PRIVATE atomic dl)
    target_link_options(libponyc.benchmarks PRIVATE "-static-libstdc++")
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(libponyc.benchmarks PRIVATE "-static-libgcc")
endif()
