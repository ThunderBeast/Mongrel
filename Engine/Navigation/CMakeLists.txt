# Define target name
set (TARGET_NAME Navigation)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

# Define dependency libs
set (LIBS ../Container ../Core ../Graphics ../IO ../Math ../Physics ../Resource ../Scene ../../ThirdParty/Recast/Include ../../ThirdParty/Detour/Include)

# Setup target
enable_pch ()
setup_library ()
