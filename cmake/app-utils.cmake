function(get_default_runtime_output_dir result_)
    if(CMAKE_CONFIGURATION_TYPES)
        set(result "${CMAKE_CURRENT_BINARY_DIR}/\$<CONFIG>")
    else()
        set(result "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    set(${result_} ${result} PARENT_SCOPE)
endfunction()


function(get_base_dir file result_)
    if(gen_dir AND file MATCHES "^${gen_dir}")
        set(result ${gen_dir})
    elseif(src_dir AND file MATCHES "^${src_dir}")
        set(result ${src_dir})
    else()
        message(FATAL_ERROR "Unexpected file location: ${file}")
    endif()
    set(${result_} ${result} PARENT_SCOPE)
endfunction()


function(convert_shaders_gen_spirv files_out_)
    file(MAKE_DIRECTORY "${gen_dir}/assets/shaders")
    set(
        glsl_include_dirs
        "${src_dir}/assets/shaders"
        # ...
    )

    find_program(glslang_validator glslangValidator REQUIRED)

    set(args "-G;--aml;-l")
    foreach(dir ${glsl_include_dirs})
        list(APPEND args "-I${dir}")
    endforeach()

    set(files_out ${${files_out_}})
    foreach(glsl_file ${glsl_files})
        get_base_dir(${glsl_file} base_dir)
        file(RELATIVE_PATH rel_path ${base_dir} ${glsl_file})
        cmake_path(REMOVE_EXTENSION rel_path LAST_ONLY)
        set(spv_file "${gen_dir}/${rel_path}.spv")
        add_custom_command(
            OUTPUT
                ${spv_file}
            DEPENDS 
                ${glsl_file}
            COMMAND 
                ${glslang_validator}
                ${args}
                -o ${spv_file}
                ${glsl_file}
            COMMENT 
                "Generating SPIR-V binary"
        )
        list(APPEND files_out ${spv_file})
    endforeach()

    set(${files_out_} ${files_out} PARENT_SCOPE)
endfunction()


function(convert_shaders files_out_)
    convert_shaders_gen_spirv(spv_files)

    find_program(spirv_cross spirv-cross REQUIRED)

    if(EMSCRIPTEN)
        set(args "--es;--version;300")
    else()
        set(args "--version;330;--no-420pack-extension")
    endif()

    set(files_out ${${files_out_}})
    foreach(spv_file ${spv_files})
        cmake_path(REPLACE_EXTENSION spv_file LAST_ONLY ".glsl" OUTPUT_VARIABLE glsl_file)
        add_custom_command(
            OUTPUT
                ${glsl_file}
            DEPENDS 
                ${spv_file}
            COMMAND 
                ${spirv_cross}
                ${args}
                --output ${glsl_file}
                ${spv_file}
            COMMENT 
                "Converting SPIR-V binary"
        )
        list(APPEND files_out ${glsl_file})
    endforeach()

    add_custom_target(${app_name}-shaders DEPENDS ${files_out})
    add_dependencies(${app_name} ${app_name}-shaders)

    set(${files_out_} ${files_out} PARENT_SCOPE)
endfunction()


function(copy_assets)
    set(files_out)
    foreach(file_in ${asset_files})
        get_base_dir(${file_in} base_dir)
        file(RELATIVE_PATH rel_path ${base_dir} ${file_in})
        set(file_out "${runtime_output_dir}/${rel_path}")
        add_custom_command(
            OUTPUT
                ${file_out}
            DEPENDS 
                ${file_in}
            COMMAND 
                ${CMAKE_COMMAND} -E copy 
                ${file_in}
                ${file_out}
            COMMENT 
                "Copying ${file_in}"
        )
        list(APPEND files_out ${file_out})
    endforeach()
    
    add_custom_target(${app_name}-assets DEPENDS ${files_out})
    add_dependencies(${app_name} ${app_name}-assets)
endfunction()


function(package_assets)
    if(NOT asset_files)
        return()
    endif()

    # Create Emscripten file mappings
    set(file_mappings)
    foreach(file ${asset_files})
        get_base_dir(${file} base_dir)
        file(RELATIVE_PATH rel_path ${base_dir} ${file})
        list(APPEND file_mappings "${file}@/${rel_path}")
    endforeach()

    # Create asset package via Emscripten's file packager utility
    set(emsc_file_packager "${EMSCRIPTEN_ROOT_PATH}/tools/file_packager")
    set(output "${runtime_output_dir}/${app-name}-assets.data")
    add_custom_command(
        OUTPUT
            ${output}
        DEPENDS
            ${asset_files}
        COMMAND
            ${emsc_file_packager}
            ${app_name}-assets.data 
            --js-output=${app_name}-assets.js 
            --preload
            ${file_mappings}
        WORKING_DIRECTORY
            "${runtime_output_dir}"
        COMMENT
            "Packaging asset data"
    )

    add_custom_target(${app_name}-assets-web DEPENDS ${output})
    add_dependencies(${app_name} ${app_name}-assets-web)
endfunction()


function(copy_web_files)
    set(files_out)
    foreach(file_in ${web_files})
        get_base_dir(${file_in} base_dir)
        file(RELATIVE_PATH rel_path "${base_dir}/web" ${file_in})
        set(file_out "${runtime_output_dir}/${rel_path}")
        add_custom_command(
            OUTPUT
                ${file_out}
            DEPENDS 
                ${file_in}
            COMMAND 
                ${CMAKE_COMMAND} -E copy 
                ${file_in}
                ${file_out}
            COMMENT 
                "Copying ${file_in}"
        )
        list(APPEND files_out ${file_out})
    endforeach()

    add_custom_target(${app_name}-web DEPENDS ${files_out})
    add_dependencies(${app_name} ${app_name}-web)
endfunction()
