cmake_minimum_required(VERSION 3.17.5)

project(breaklee VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG=1")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMake)

option(CONFIG_BUILD_TARGET_AUCTION_SVR "Build Auction Server" OFF)
option(CONFIG_BUILD_TARGET_BREAKLEE "Build Breaklee" OFF)
option(CONFIG_BUILD_TARGET_CHAT_SVR "Build Chat Server" OFF)
option(CONFIG_BUILD_TARGET_LOGIN_SVR "Build Login Server" ON)
option(CONFIG_BUILD_TARGET_MASTER_DBAGENT "Build Master DB Agent" ON)
option(CONFIG_BUILD_TARGET_MASTER_SVR "Build Master Server" ON)
option(CONFIG_BUILD_TARGET_PARTY_SVR "Build Party Server" ON)
option(CONFIG_BUILD_TARGET_WORLD_SVR "Build World Server" ON)

if(WIN32)
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /ZI /Od")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /ZI")

    enable_language("RC")
    
    if(CMAKE_CL_64)
		set(PLATFORM_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/Libraries/Win64)
	else()
		set(PLATFORM_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/Libraries/Win32)
	endif()

    set(LUA_INCLUDE_DIR ${PLATFORM_LIBRARY_DIR}/Lua/include)
	set(MARIADB_INCLUDE_DIR ${PLATFORM_LIBRARY_DIR}/MariaDB/include)
	set(OPENSSL_INCLUDE_DIR ${PLATFORM_LIBRARY_DIR}/OpenSSL/include)
	set(ZLIB_INCLUDE_DIR ${PLATFORM_LIBRARY_DIR}/Zlib/include)
    set(LUA_LIBRARY lua54)
	set(MARIADB_LIBRARY libmariadb)
	set(ZLIB_LIBRARY zlib)
	set(OPENSSL_CRYPTO_LIBRARY libcrypto_static)
	set(OPENSSL_SSL_LIBRARY libssl)

	add_definitions(-DUSE_MYSQL_INCLUDE)

	link_directories(
        ${PLATFORM_LIBRARY_DIR}/Lua
		${PLATFORM_LIBRARY_DIR}/MariaDB/lib
		${PLATFORM_LIBRARY_DIR}/OpenSSL/lib
		${PLATFORM_LIBRARY_DIR}/Zlib/lib
	)

	file(GLOB RUNTIME_LIBRARIES ${PLATFORM_LIBRARY_DIR}/MariaDB/lib/*.dll
								${PLATFORM_LIBRARY_DIR}/OpenSSL/lib/*.dll
								${PLATFORM_LIBRARY_DIR}/Zlib/lib/*.dll
                                ${PLATFORM_LIBRARY_DIR}/Lua/*.dll)

	file(COPY ${RUNTIME_LIBRARIES} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/)
else()
    find_package(Lua 5.4.2 EXACT REQUIRED) 
	find_package(ZLIB 1.2.8 EXACT REQUIRED)
	find_package(OpenSSL 1.1.1 EXACT REQUIRED)

	set(MARIADB_INCLUDE_NAME mariadb)

	if(APPLE)
		set(MARIADB_INCLUDE_NAME mysql)
	endif()

    add_definitions(-DUSE_MYSQL_INCLUDE)

	find_path(MARIADB_INCLUDE_DIR NAMES ${MARIADB_INCLUDE_NAME}/mysql.h)
	find_library(MARIADB_LIBRARY NAMES mariadb)
endif()

file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/)

file(COPY ${PROJECT_SOURCE_DIR}/ServerData/ DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/Data/)
file(COPY ${PROJECT_SOURCE_DIR}/Scripts/ DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/Scripts/)

set(CORE_LIB_DIR ${PROJECT_SOURCE_DIR}/CoreLib)
set(NET_LIB_DIR ${PROJECT_SOURCE_DIR}/NetLib)
set(RUNTIME_DATA_LIB_DIR ${PROJECT_SOURCE_DIR}/RuntimeDataLib)
set(RUNTIME_LIB_DIR ${PROJECT_SOURCE_DIR}/RuntimeLib)

file(GLOB CORE_LIB_HEADERS ${CORE_LIB_DIR}/*.h)
file(GLOB CORE_LIB_SOURCES ${CORE_LIB_DIR}/*.c)

file(GLOB NET_LIB_HEADERS ${NET_LIB_DIR}/*.h)
file(GLOB NET_LIB_SOURCES ${NET_LIB_DIR}/*.c)

file(GLOB RUNTIME_DATA_LIB_HEADERS ${RUNTIME_DATA_LIB_DIR}/*.h)
file(GLOB RUNTIME_DATA_LIB_SOURCES ${RUNTIME_DATA_LIB_DIR}/*.c)

file(GLOB RUNTIME_LIB_HEADERS ${RUNTIME_LIB_DIR}/*.h)
file(GLOB RUNTIME_LIB_SOURCES ${RUNTIME_LIB_DIR}/*.c)

set(SHARED_HEADERS_DIR ${PROJECT_SOURCE_DIR}/Shared)
file(GLOB SHARED_HEADERS ${SHARED_HEADERS_DIR}/*.h)

add_library(CoreLib STATIC ${CORE_LIB_HEADERS} ${CORE_LIB_SOURCES})
target_link_libraries(CoreLib ${MARIADB_LIBRARY} ${ZLIB_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} ${OPENSSL_SSL_LIBRARY})
target_include_directories(CoreLib PUBLIC ${MARIADB_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})

add_library(NetLib STATIC ${NET_LIB_HEADERS} ${NET_LIB_SOURCES})
target_include_directories(NetLib PUBLIC ${PROJECT_SOURCE_DIR} ${LUA_INCLUDE_DIR})
target_link_libraries(NetLib CoreLib ${LUA_LIBRARY})

set(LIBUV_DIR ${PROJECT_SOURCE_DIR}/Libraries/libuv)
add_subdirectory(${LIBUV_DIR} ${CMAKE_BINARY_DIR}/libuv)

target_include_directories(CoreLib PUBLIC ${LIBUV_DIR}/include)
target_link_libraries(CoreLib uv_a)

#target_include_directories(NetLib PUBLIC ${LIBUV_DIR}/include)
#target_link_libraries(NetLib uv_a)

if(WIN32)
    target_link_libraries(CoreLib ws2_32 iphlpapi)
#    target_link_libraries(NetLib ws2_32 iphlpapi)
endif()

add_library(RuntimeDataLib STATIC ${RUNTIME_DATA_LIB_HEADERS} ${RUNTIME_DATA_LIB_SOURCES})
target_include_directories(RuntimeDataLib PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(RuntimeDataLib CoreLib)

add_library(RuntimeLib STATIC ${RUNTIME_LIB_HEADERS} ${RUNTIME_LIB_SOURCES})
target_include_directories(RuntimeLib PUBLIC ${PROJECT_SOURCE_DIR} ${LUA_INCLUDE_DIR})
target_link_libraries(RuntimeLib CoreLib RuntimeDataLib ${LUA_LIBRARY})

if(CONFIG_BUILD_TARGET_AUCTION_SVR)
    set(AUCTION_SVR_DIR ${PROJECT_SOURCE_DIR}/AuctionSvr)
    
    file(GLOB AUCTION_SVR_HEADERS ${AUCTION_SVR_DIR}/*.h)
    file(GLOB AUCTION_SVR_SOURCES ${AUCTION_SVR_DIR}/*.c)

    add_executable(AuctionSvr ${AUCTION_SVR_HEADERS} ${AUCTION_SVR_SOURCES} ${SHARED_HEADERS})
    target_include_directories(AuctionSvr PUBLIC ${PROJECT_SOURCE_DIR} ${SHARED_HEADERS_DIR})
    target_link_libraries(AuctionSvr NetLib CoreLib)
    
    if(WIN32)
        target_link_libraries(AuctionSvr ${AUCTION_SVR_DIR}/main.res)
    endif()
endif()

if(CONFIG_BUILD_TARGET_BREAKLEE)
    set(BREAKLEE_DIR ${PROJECT_SOURCE_DIR}/Breaklee)

    file(GLOB BREAKLEE_HEADERS ${BREAKLEE_DIR}/*.h)
    file(GLOB BREAKLEE_SOURCES ${BREAKLEE_DIR}/*.c)
    
    add_executable(breaklee ${BREAKLEE_HEADERS} ${BREAKLEE_SOURCES} ${SHARED_HEADERS})
    target_include_directories(breaklee PUBLIC ${PROJECT_SOURCE_DIR} ${SHARED_HEADERS_DIR})
    target_link_libraries(breaklee NetLib CoreLib)
endif()

if(CONFIG_BUILD_TARGET_CHAT_SVR)
    set(CHAT_SVR_DIR ${PROJECT_SOURCE_DIR}/ChatSvr)
    
    file(GLOB CHAT_SVR_HEADERS ${CHAT_SVR_DIR}/*.h)
    file(GLOB CHAT_SVR_SOURCES ${CHAT_SVR_DIR}/*.c)

    add_executable(ChatSvr ${CHAT_SVR_HEADERS} ${CHAT_SVR_SOURCES} ${SHARED_HEADERS})
    target_include_directories(ChatSvr PUBLIC ${PROJECT_SOURCE_DIR} ${SHARED_HEADERS_DIR})
    target_link_libraries(ChatSvr NetLib CoreLib)
    
    if(WIN32)
        target_link_libraries(ChatSvr ${CHAT_SVR_DIR}/main.res)
    endif()
endif()

if(CONFIG_BUILD_TARGET_LOGIN_SVR)
    set(LOGIN_SVR_DIR ${PROJECT_SOURCE_DIR}/LoginSvr)
    
    file(GLOB LOGIN_SVR_HEADERS ${LOGIN_SVR_DIR}/*.h)
    file(GLOB LOGIN_SVR_SOURCES ${LOGIN_SVR_DIR}/*.c)

    add_executable(LoginSvr ${LOGIN_SVR_HEADERS} ${LOGIN_SVR_SOURCES} ${SHARED_HEADERS})
    target_include_directories(LoginSvr PUBLIC ${PROJECT_SOURCE_DIR} ${OPENSSL_INCLUDE_DIR} ${SHARED_HEADERS_DIR} ${LUA_INCLUDE_DIR})
    target_link_libraries(LoginSvr NetLib CoreLib)

    if(WIN32)
        target_link_libraries(LoginSvr ${LOGIN_SVR_DIR}/main.res)
    endif()
endif()

if(CONFIG_BUILD_TARGET_MASTER_DBAGENT)
    set(MASTER_DBAGENT_DIR ${PROJECT_SOURCE_DIR}/MasterDBAgent)
    
    file(GLOB MASTER_DBAGENT_HEADERS ${MASTER_DBAGENT_DIR}/*.h)
    file(GLOB MASTER_DBAGENT_SOURCES ${MASTER_DBAGENT_DIR}/*.c)

    add_executable(MasterDBAgent ${MASTER_DBAGENT_HEADERS} ${MASTER_DBAGENT_SOURCES} ${SHARED_HEADERS})
    target_include_directories(MasterDBAgent PUBLIC ${PROJECT_SOURCE_DIR} ${SHARED_HEADERS_DIR})
    target_link_libraries(MasterDBAgent NetLib CoreLib RuntimeLib RuntimeDataLib)
    
    if(WIN32)
        target_link_libraries(MasterDBAgent ${MASTER_DBAGENT_DIR}/main.res)
    endif()
endif()

if(CONFIG_BUILD_TARGET_MASTER_SVR)
    set(MASTER_SVR_DIR ${PROJECT_SOURCE_DIR}/MasterSvr)
    
    file(GLOB MASTER_SVR_HEADERS ${MASTER_SVR_DIR}/*.h)
    file(GLOB MASTER_SVR_SOURCES ${MASTER_SVR_DIR}/*.c)

    add_executable(MasterSvr ${MASTER_SVR_HEADERS} ${MASTER_SVR_SOURCES} ${SHARED_HEADERS})
    target_include_directories(MasterSvr PUBLIC ${PROJECT_SOURCE_DIR} ${SHARED_HEADERS_DIR})
    target_link_libraries(MasterSvr NetLib CoreLib RuntimeLib RuntimeDataLib)
    
    if(WIN32)
        target_link_libraries(MasterSvr ${MASTER_SVR_DIR}/main.res)
    endif()
endif()

if(CONFIG_BUILD_TARGET_PARTY_SVR)
    set(PARTY_SVR_DIR ${PROJECT_SOURCE_DIR}/PartySvr)
    
    file(GLOB PARTY_SVR_HEADERS ${PARTY_SVR_DIR}/*.h)
    file(GLOB PARTY_SVR_SOURCES ${PARTY_SVR_DIR}/*.c)

    add_executable(PartySvr ${PARTY_SVR_HEADERS} ${PARTY_SVR_SOURCES} ${SHARED_HEADERS})
    target_include_directories(PartySvr PUBLIC ${PROJECT_SOURCE_DIR} ${SHARED_HEADERS_DIR})
    target_link_libraries(PartySvr NetLib CoreLib RuntimeLib RuntimeDataLib)
    
    if(WIN32)
        target_link_libraries(PartySvr ${PARTY_SVR_DIR}/main.res)
    endif()
endif()

if(CONFIG_BUILD_TARGET_WORLD_SVR)
    set(WORLD_SVR_DIR ${PROJECT_SOURCE_DIR}/WorldSvr)
    
    file(GLOB WORLD_SVR_HEADERS ${WORLD_SVR_DIR}/*.h)
    file(GLOB WORLD_SVR_SOURCES ${WORLD_SVR_DIR}/*.c)
        
    add_executable(WorldSvr ${WORLD_SVR_HEADERS} ${WORLD_SVR_SOURCES} ${SHARED_HEADERS})
    target_include_directories(WorldSvr PUBLIC ${PROJECT_SOURCE_DIR} ${SHARED_HEADERS_DIR})
    target_link_libraries(WorldSvr NetLib CoreLib RuntimeLib RuntimeDataLib)

    if(WIN32)
        target_link_libraries(WorldSvr ${WORLD_SVR_DIR}/main.res)
    endif()
endif()

if(NOT WIN32)
    target_link_libraries(CoreLib m dl)
    target_link_libraries(NetLib m)
    target_link_libraries(RuntimeDataLib m)
    target_link_libraries(RuntimeLib m)

    if(CONFIG_BUILD_TARGET_LOGIN_SVR)
        target_link_libraries(LoginSvr m)
    endif()
endif()