cmake_minimum_required(VERSION 3.13)

project(user)
enable_language(ASM)
set(CMAKE_OSX_DEPLOYMENT_TARGET "")

# Path
if (${ARCH} STREQUAL riscv32 OR ${ARCH} STREQUAL riscv64)
    set(ARCH_DIR lib/arch/riscv)
else()
    set(ARCH_DIR lib/arch/${ARCH})
endif()

set(ASM_DIR asm)
set(BIN_DIR bin)

# Toolchain
set(PREFIX ${ARCH}-unknown-elf-)
# riscv64-unknown-elf-

if (${ARCH} STREQUAL riscv32)
    set(CMAKE_C_FLAGS       "-march=rv32imac -mabi=ilp32 -mcmodel=medany")
elseif (${ARCH} STREQUAL riscv64)
    set(CMAKE_C_FLAGS       "-march=rv64imac -mabi=lp64 -mcmodel=medany")
else()
    message("Unsupported arch: ${ARCH}")
endif ()

set(CMAKE_ASM_COMPILER      ${PREFIX}gcc)
set(CMAKE_C_COMPILER        ${PREFIX}gcc)
set(CMAKE_OBJCOPY           ${PREFIX}objcopy)
set(CMAKE_OBJDUMP           ${PREFIX}objdump)
set(CMAKE_RANLIB            ${PREFIX}ranlib)
set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} -fno-builtin -nostdinc -fno-stack-protector -ggdb -Wall")
set(CMAKE_ASM_FLAGS         ${CMAKE_C_FLAGS})
set(CMAKE_C_LINK_FLAGS      "${LINK_FLAGS} -nostdlib -T ${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/user.ld")

# Library
aux_source_directory(lib LIBS)
#set(LIBS ${ARCH_DIR}/crt.S ${LIBS})
set(LIBS ${ARCH_DIR}/crt.S src/oscomp/clone.s ${LIBS})
add_library(ulib ${LIBS} syscall_ids)
include_directories(include/)
target_include_directories(ulib PRIVATE ${ARCH_DIR})

# Execuatble
set(ENTRY "0x1000")
#aux_source_directory(src SRCS)
aux_source_directory(src/oscomp SRCS)
set(EXECUTABLE_OUTPUT_PATH ${ARCH})
foreach(PATH ${SRCS})
    get_filename_component(NAME ${PATH} NAME_WE)

    if ("${CHAPTER}" STREQUAL 2)
        math(EXPR ENTRY "0x80400000" OUTPUT_FORMAT HEXADECIMAL)
    endif ()

    if ("${CHAPTER}" MATCHES 3_*)
        if(${NAME} STREQUAL ch4_mmap0)
            break ()
        elseif (${NAME} STREQUAL ch2_exit OR ${NAME} STREQUAL ch3_1_yield0 OR ${NAME} STREQUAL ch3_2_stride0)
            math(EXPR ENTRY "0x80400000" OUTPUT_FORMAT HEXADECIMAL)
        elseif (${NAME} STREQUAL ch3t_deadloop)
            math(EXPR ENTRY "0x80500000" OUTPUT_FORMAT HEXADECIMAL)
        else ()
            math(EXPR ENTRY "${ENTRY} + 0x20000" OUTPUT_FORMAT HEXADECIMAL)
        endif ()
    endif ()

    add_executable(${NAME} ${PATH})
    target_link_libraries(${NAME} ulib)
    target_link_options(${NAME} PRIVATE -Ttext ${ENTRY})

    add_custom_command(
            TARGET ${NAME}
            POST_BUILD
            COMMAND mkdir -p ${ASM_DIR}
            COMMAND ${CMAKE_OBJDUMP} ARGS -d -S $<TARGET_FILE:${NAME}> > ${ASM_DIR}/${NAME}.asm
    )
    add_custom_command(
            TARGET ${NAME}
            POST_BUILD
            COMMAND mkdir -p ${BIN_DIR}
            COMMAND ${CMAKE_OBJCOPY} ARGS -O binary $<TARGET_FILE:${NAME}> ${BIN_DIR}/${NAME}.bin --set-section-flags .bss=alloc,load,contents
    )
endforeach()

add_custom_command(
        OUTPUT syscall_ids.h
        COMMAND sed ARGS -n -e s/__NR_/SYS_/p
        < ${CMAKE_SOURCE_DIR}/${ARCH_DIR}/syscall_ids.h.in
        > ${CMAKE_SOURCE_DIR}/lib/syscall_ids.h
)
