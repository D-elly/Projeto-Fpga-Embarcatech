add_library(cartao_sd STATIC
    CartaoSD.cpp
    ControladorSpiCartao.cpp
    DriverCartaoSd.cpp
    FatFsPort.cpp
    FatFsTempo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffunicode.c
)

target_include_directories(cartao_sd PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source
)

target_link_libraries(cartao_sd PUBLIC
    pico_stdlib
    hardware_spi
)
