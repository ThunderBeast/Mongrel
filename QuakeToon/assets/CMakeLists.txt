
# Define target name
set ( TARGET_NAME Q2Convert )

add_definitions( -DC_ONLY=1 )

# Define source files
file ( GLOB ASSET_FILES *.c *.cpp)

set (SOURCE_FILES ${ASSET_FILES} )

setup_executable ()

