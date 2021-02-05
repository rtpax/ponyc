cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(ponyc VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

add_executable(ponyc main.c)

target_include_directories(ponyc
    PRIVATE ../../src/common
    PRIVATE ../../src/libponyc
    PRIVATE ../../build/libs/include
)

target_link_directories(ponyc
    PRIVATE ../../build/libs/lib
)

if (MSVC)
    target_link_libraries(ponyc
        PRIVATE libponyc
        PRIVATE libponyrt
        PRIVATE blake2
        PRIVATE dbghelp
        PRIVATE ucrt
        PRIVATE Ws2_32
        PRIVATE Shlwapi
    )

    set_source_files_properties(main.c PROPERTIES LANGUAGE CXX)

    llvm_config(ponyc all)

    get_target_property(TARGET_LIBRARIES ponyc LINK_LIBRARIES)
    LIST(REMOVE_ITEM TARGET_LIBRARIES LTO)
    set_property(TARGET ponyc PROPERTY LINK_LIBRARIES ${TARGET_LIBRARIES})
else()
    target_link_libraries(ponyc PRIVATE
        libponyc
        libponyrt
        blake2
    )

    llvm_config(ponyc all)

    if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
        target_link_libraries(ponyc PRIVATE dl z LLVMXRay LLVMWindowsManifest LLVMTableGen LLVMSymbolize LLVMDebugInfoPDB LLVMOrcJIT LLVMOrcError LLVMJITLink LLVMObjectYAML LLVMMIRParser LLVMMCA LLVMLTO LLVMPasses LLVMCoroutines LLVMObjCARCOpts LLVMipo LLVMInstrumentation LLVMVectorize LLVMLinker LLVMIRReader LLVMAsmParser LLVMFrontendOpenMP LLVMExtensions LLVMLineEditor LLVMLibDriver LLVMInterpreter LLVMFuzzMutate LLVMMCJIT LLVMExecutionEngine LLVMRuntimeDyld LLVMDWARFLinker LLVMDlltoolDriver LLVMOption LLVMDebugInfoGSYM LLVMCoverage LLVMAArch64Disassembler LLVMAArch64CodeGen LLVMAArch64AsmParser LLVMAArch64Desc LLVMAArch64Utils LLVMAArch64Info LLVMX86Disassembler LLVMX86CodeGen LLVMX86AsmParser LLVMX86Desc LLVMX86Info LLVMARMDisassembler LLVMARMCodeGen LLVMCFGuard LLVMGlobalISel LLVMSelectionDAG LLVMAsmPrinter LLVMDebugInfoDWARF LLVMCodeGen LLVMTarget LLVMScalarOpts LLVMInstCombine LLVMAggressiveInstCombine LLVMTransformUtils LLVMBitWriter LLVMAnalysis LLVMProfileData LLVMObject LLVMTextAPI LLVMBitReader LLVMCore LLVMRemarks LLVMBitstreamReader LLVMARMAsmParser LLVMMCParser LLVMARMDesc LLVMMCDisassembler LLVMMC LLVMDebugInfoCodeView LLVMDebugInfoMSF LLVMBinaryFormat LLVMARMUtils LLVMARMInfo LLVMSupport LLVMDemangle)
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD|DragonFly")
        target_link_libraries(ponyc PRIVATE execinfo)
    else()
        target_link_libraries(ponyc PRIVATE atomic dl)
        target_link_options(ponyc PRIVATE "-static-libstdc++")
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(ponyc PRIVATE "-static-libgcc")
    endif()
endif (MSVC)
