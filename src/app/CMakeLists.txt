# add_executable(BiSearch main.cc)
# target_link_libraries(BiSearch ${FINAL_OBJ})

find_package(Threads REQUIRED)
# target_link_libraries(BiSearch ${CMAKE_THREAD_LIBS_INIT})

add_executable(BiSearch main.cc)
target_link_libraries(BiSearch ${FINAL_OBJ} xdelta3 ${CMAKE_THREAD_LIBS_INIT})

