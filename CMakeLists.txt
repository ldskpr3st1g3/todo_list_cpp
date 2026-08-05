cmake_minimum_required(VERSION 3.14)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

project(petty_plusik CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF)


find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)


pkg_check_modules(SODIUM REQUIRED libsodium)

include(FetchContent)
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0  
)
FetchContent_MakeAvailable(jwt-cpp)


add_executable(${PROJECT_NAME} main.cc CpuThreadPool.cc)

aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(models MODEL_SRC)

drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
                    ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/models
                                   ${SODIUM_INCLUDE_DIRS}) 

target_sources(${PROJECT_NAME}
               PRIVATE
               ${CTL_SRC}
               ${FILTER_SRC}
               ${PLUGIN_SRC}
               ${MODEL_SRC})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Drogon::Drogon
    jwt-cpp::jwt-cpp
    OpenSSL::SSL
    OpenSSL::Crypto
    PostgreSQL::PostgreSQL
    ${SODIUM_LIBRARIES} 
)

# add_subdirectory(test)