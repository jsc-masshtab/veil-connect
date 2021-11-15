CMAKE_MINIMUM_REQUIRED(VERSION 3.4)

SET(CMAKE_C_STANDARD 99)

############################SOURCE CODE################################
INCLUDE_DIRECTORIES(
        .
)

# breakpad_wrapper.o создается вручную вызовом в терминале компилятора C++ с указанием пути
# к хедерам google breakpad в msys2 (breakpad есть в репозитории msys2)
# g++ breakpad_wrapper.cpp -o win64/breakpad_wrapper.o -c -std=c++1z  -I C:/msys64/mingw64/include/breakpad
FILE(GLOB SRC_FILES
        breakpad_wrapper_data.c
        win64/breakpad_wrapper.o
)

###############################LIBRARIES###############################
# breakpad
INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}breakpad)
SET(LIBS ${LIBS} -lbreakpad_client -lstdc++)

#############################LINKING###################################
ADD_LIBRARY(breakpad_wrapper SHARED ${SRC_FILES})
target_link_libraries(
    breakpad_wrapper
    ${LIBS}
)
