# Define target name
set ( TARGET_NAME QuakeToon )

# Define source files
file ( GLOB APPLICATION_FILES application/*.cpp )
set ( SOURCE_FILES ${APPLICATION_FILES} )

# Define dependency libs
set (LIBS ../Engine/Container ../Engine/Core ../Engine/Engine ../Engine/IO ../Engine/Math ../Engine/Resource ../Engine/Script)

if (ENABLE_LUA)
    set (LIBS ${LIBS} ../Extras/LuaScript)
endif ()

# Setup target with resource copying
#setup_main_executable ()

setup_executable ()
