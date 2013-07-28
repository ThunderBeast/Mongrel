# Define target name
set ( TARGET_NAME QuakeToon )

add_definitions( -DC_ONLY=1 )

# Define source files
file ( GLOB APPLICATION_FILES application/*.cpp )

file ( GLOB COMMON_C_FILES qcommon/*.c )
file ( GLOB CLIENT_C_FILES client/*.c )
file ( GLOB SERVER_C_FILES server/*.c )
file ( GLOB RENDER_C_FILES render/*.c )
file ( GLOB GAME_C_FILES   game/*.c )
file ( GLOB SYSTEM_C_FILES system/*.c )

set (SOURCE_FILES ${COMMON_C_FILES} ${CLIENT_C_FILES} ${SERVER_C_FILES} ${GAME_C_FILES} ${RENDER_C_FILES} ${SYSTEM_C_FILES} ${APPLICATION_FILES} )

# Define dependency libs
set (LIBS ../Engine/Container ../Engine/Core ../Engine/Engine ../Engine/IO ../Engine/Math ../Engine/Resource ../Engine/Script)

if (ENABLE_LUA)
    set (LIBS ${LIBS} ../Extras/LuaScript)
endif ()

# Setup target with resource copying
#setup_main_executable ()

setup_executable ()
set (CMAKE_EXE_LINKER_FLAGS "-framework AudioUnit -framework Carbon -framework Cocoa -framework CoreAudio -framework ForceFeedback -framework IOKit -framework OpenGL -framework CoreServices")
