# Every function that returns a value does so into the given `return_var` at
# the end of the arguments.

# Names for MSVC warning codes
set(MSVC_SIGN_MISMATCH_WARNING "C4018")
set(MSVC_FLOAT_TO_INT_WARNING "C4224")

# Given `the_list`, prepends `to_prepend` to each item in this list.
function(ex_prepend_to_each the_list to_prepend return_var)
	set(prepended_items "")
	foreach(item ${the_list})
		set(prepended_items "${to_prepend}${item};${prepended_items}")
	endforeach()
	set(${return_var} "${prepended_items}" PARENT_SCOPE)
endfunction()

# Compiles the given file (should be C++) with the include directions and
# preprocessor definitions of the given target. Usually for seeing the
# assembly output, or something from clang.
function(ex_compile_one_file file_name target extra_options)
	get_target_property(include_dirs ${target} INCLUDE_DIRECTORIES)
	ex_prepend_to_each("${include_dirs}" "-I" include_dirs_flag)
	# get_target_property(compile_defs ${target} INTERFACE_COMPILE_DEFINITIONS)
	message("compiling single file ${file_name} with compile defs ${compile_defs} and include dirs ${include_dirs_flag}")
	execute_process(COMMAND g++ ${extra_options} ${include_dirs_flag} ${compile_defs} ${file_name})
endfunction()

function(place_in_folder target folder_name)
	set_target_properties(${target} PROPERTIES FOLDER ${folder_name})
endfunction()

function(add_macro_definition macro_name)
	set(_tmp "-D${macro_name}=\"${${macro_name}}\"")
	message(STATUS "LOGL Adding macro definition - ${_tmp}")
	add_compile_options(${_tmp})
endfunction()

function(one_error_max)
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		add_compile_options(-fmax-errors=1)
	endif()
endfunction()

function(march_native)
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		add_compile_options(-march=native)
	endif()
endfunction()

function(less_msvc_warnings level)
	if(MSVC)
  		if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    		string(REGEX REPLACE "/W[0-4]" "/W${level}" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  		else()
    		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  		endif()
  		message("Setting MSVC warning level to ${level}")
	elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  		# Update if necessary
  	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic")
	endif()
endfunction()

# function(suppress_all_msvc_warnings)
# 	if(MSVC)
# 		if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
#     		string(REGEX REPLACE "/W[0-4]" "/W0" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#   		else()
#     		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0")
#   		endif()
# 		string(REGEX REPLACE "/W[0-4]" "/W0" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#   		message("SUPPRESSING ALL MSVC WARNINGS. MIGHT WANNA CHANGE THAT. New flags = ${CMAKE_CXX_FLAGS}")
#   	endif()
# endfunction()

function(suppress_all_msvc_warnings)
	if (MSVC)
		add_compile_options("/w")
	endif()
endfunction()

function(disable_unwanted_warnings)
	if(MSVC)
		add_compile_options(/W1 /wd4100 /wd4201 /wd4189 /wd4514 /wd4820 /wd4710 /wd4571 /wd5027 /wd5045 /wd4625 /wd4626 /wd5026 /wd4623)
		# ^ A plethora of 'ctor/move ctor/assignment etc. was implicitly deleted' warnings
		add_compile_options(/wd4365)
		# ^ signed-unsigned mismatch. Not a fan of omitting this warning, but third-party libs are violating.
		add_compile_options(/wd4324)
		# ^ struct getting padded extra due to alignas specifier
		add_compile_options(/wd4774)
		# ^ weird one in the <string> file of msvc lib
		add_compile_options(/wd4996)
		# ^ 'Unsafe' C function related crapola
		add_compile_options(/wd5039)
		# ^ Weird C function related exception throwing undefined behaivor extravaganza
		add_compile_options(/wd4668)
		# ^ Allow undefined macros that get #if'ed assume the value of 0 without bitching about it.
		add_compile_options(/wd4127)
		# ^ Conditional expression is constant, because I FREAKING use it for constexpr. DAMMIT.
		add_compile_options(/wd4061)
		# ^ Explicitly not handling enum value in switch case.
		add_compile_options(/wd4200)
		# ^ Non-standard extensions, but usually supported by the three.
		add_compile_options(/wd4711)
		# ^ Function selected for automatic inline expansion
	endif()

endfunction()

function(ex_compile_with_mt)
	if (MSVC)
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	endif()
endfunction()

function(ex_set_gcc_or_clang)
	set(gcc_or_clang FALSE PARENT_SCOPE)
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  		set(gcc_or_clang TRUE PARENT_SCOPE)
  	elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  		set(msvc TRUE PARENT_SCOPE)
	elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  		set(gcc_or_clang TRUE PARENT_SCOPE)
	endif()
endfunction()
