set (D3D11_FOUND "NO")

set (WIN10_SDK_VERSION "10.0.17763.0")

if (WIN32)
	set (WIN10_SDK_DIR "C:/Program Files (x86)/Windows Kits/10")
	set (LEGACY_SDK_DIR "$ENV{DXSDK_DIR}")

	set (ARCH "x64")
	
	# Find include files
	find_path (D3D11_INCLUDE_PATH 
		NAMES d3d11.h
		PATHS "${WIN10_SDK_DIR}/Include/${WIN10_SDK_VERSION}/um"
		NO_DEFAULT_PATH
		DOC "Path to the windows 10 d3d11.h file"
	)

	if (D3D11_INCLUDE_PATH)
		find_library (D3D11_LIB
			NAMES d3d11
			PATHS "${WIN10_SDK_DIR}/Lib/${WIN10_SDK_VERSION}/um/${ARCH}"
			NO_DEFAULT_PATH
			DOC "Path to the windows 10 d3d11.lib file"
		)
		
		if (D3D11_LIB)
			set (D3D11_LIBRARIES "${D3D11_LIB}")

			find_library (D3D_COMPILER_LIB 
				NAMES d3dcompiler
				PATHS "${WIN10_SDK_DIR}/Lib/${WIN10_SDK_VERSION}/um/${ARCH}"
				NO_DEFAULT_PATH
				DOC "Path to the windows 10 d3d11.lib file"
			)
		
			if (D3D_COMPILER_LIB)
				set (D3D11_COMPILER_LIBS ${D3D_COMPILER_LIB})

				find_library (DXGUID_LIB
					NAMES dxguid
					PATHS "${WIN10_SDK_DIR}/Lib/${WIN10_SDK_VERSION}/um/${ARCH}"
					NO_DEFAULT_PATH
					DOC "Path to the windows 10 dxguid.lib file"
				)

				if (DXGUID_LIB)
					set (DXGUID_LIB "${DXGUID_LIB}") # Eh?
					set (D3D11_FOUND "YES")
				endif()

			endif (D3D_COMPILER_LIB)
		endif (D3D11_LIB)

	endif (D3D11_INCLUDE_PATH)
endif (WIN32)

if (D3D11_FOUND)
	if (NOT D3D11_FIND_QUIETLY)
		message (STATUS "D3D11 headers found at ${D3D11_INCLUDE_PATH}")
	endif (NOT D3D11_FIND_QUIETLY)
else (D3D11_FOUND)
	if (D3D11_FIND_REQUIRED)
		message (FATAL_ERROR "Could NOT find Direct3D11")
	endif (D3D11_FIND_REQUIRED)
	if (NOT D3D11_FIND_QUIETLY)
		message (STATUS "Could NOT find Direct3D11")
	endif (NOT D3D11_FIND_QUIETLY)
endif (D3D11_FOUND)