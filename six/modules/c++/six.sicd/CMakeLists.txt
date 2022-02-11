# Dynamically embed XML schemas into a source file with an associated header.
#
# Arguments:
#  schema_paths        - The paths to schemas to embed.
#  module_namespace    - The name of the module that's added to six::${module_namespace}::schemas
#  hdr                 - The path to the header file to create.
#  cpp                 - The path to the source file to create.
function(generate_embedded_schemas schema_paths module_namespace hdr cpp)
    set(EMBED_NAMESPACE_HEADER "namespace six\n{\nnamespace ${module_namespace}\n{\nnamespace schemas\n{\n\n")
    set(EMBED_NAMESPACE_FOOTER "}\n}\n}\n")

    # Write the SICD schema header at compile-time.
    file(WRITE ${hdr} "#pragma once\n\n#include <vector>\n\n${EMBED_NAMESPACE_HEADER}")
    file(WRITE ${cpp} "#include <vector>\n\n${EMBED_NAMESPACE_HEADER}")
    set(functions "")
    foreach(schema_file_path ${SCHEMAS_TO_EMBED})

        # Scrub the schema file so that it's converted into a safe-ish function name.
        get_filename_component(schema_file ${schema_file_path} NAME)
        string(REPLACE "." "_" schema_file_safe ${schema_file})

        # Function declaration.
        file(APPEND ${hdr} "/*! Content of ${schema_file} */\n")
        file(APPEND ${hdr} "const char* ${schema_file_safe}();\n\n")

        # Function definition.
        file(READ ${schema_file_path} schema_content)
        file(APPEND ${cpp} "const char* ${schema_file_safe}()\n{\n")
        file(APPEND ${cpp} "  return R\"(")
        file(APPEND ${cpp} ${schema_content})
        file(APPEND ${cpp} ")\";\n")
        file(APPEND ${cpp} "}\n\n")

        list(APPEND functions "${schema_file_safe}()")
    endforeach()

    # Add a utility function that'll get a vector that contains every schema.
    file(APPEND ${hdr} "/*! Get references to all the embedded schemas */\n")
    file(APPEND ${hdr} "std::vector<const char*> All();\n\n")
    file(APPEND ${cpp} "std::vector<const char*> All()\n{\n")
    list(JOIN functions ", " functions)
    file(APPEND ${cpp} "  return { ${functions} };\n")
    file(APPEND ${cpp} "}\n\n")
    file(APPEND ${hdr} "${EMBED_NAMESPACE_FOOTER}")
    file(APPEND ${cpp} "${EMBED_NAMESPACE_FOOTER}")
endfunction()

file(GLOB SCHEMAS_TO_EMBED conf/schema/*.xsd)
set(schema_header "${CMAKE_CURRENT_BINARY_DIR}/include/six/sicd/Schemas.h")
set(schema_source "${CMAKE_CURRENT_BINARY_DIR}/source/Schemas.cpp")
generate_embedded_schemas("${SCHEMAS_TO_EMBED}" sicd ${schema_header} ${schema_source})
install(FILES "${schema_header}"
        DESTINATION "${CODA_STD_PROJECT_INCLUDE_DIR}/six/sicd"
        ${CODA_INSTALL_OPTION})

coda_add_module(
    six.sicd
    DEPS six-c++ units-c++
    SOURCES
        source/Antenna.cpp
        source/AreaPlaneUtility.cpp
        source/ComplexData.cpp
        source/ComplexDataBuilder.cpp
        source/ComplexToAMP8IPHS8I.cpp
        source/ComplexXMLControl.cpp
        source/ComplexXMLParser.cpp
        source/ComplexXMLParser040.cpp
        source/ComplexXMLParser041.cpp
        source/ComplexXMLParser04x.cpp
        source/ComplexXMLParser050.cpp
        source/ComplexXMLParser100.cpp
        source/ComplexXMLParser101.cpp
        source/ComplexXMLParser10x.cpp
        source/CropUtils.cpp
        source/Functor.cpp
        source/GeoData.cpp
        source/GeoLocator.cpp
        source/Grid.cpp
        source/ImageData.cpp
        source/ImageFormation.cpp
        source/NITFReadComplexXMLControl.cpp
        source/PFA.cpp
        source/Position.cpp
        source/RMA.cpp
        source/RadarCollection.cpp
        source/RgAzComp.cpp
        source/SCPCOA.cpp
        source/SICDByteProvider.cpp
        source/SICDMesh.cpp
        source/SICDVersionUpdater.cpp
        source/SICDWriteControl.cpp
        source/SlantPlanePixelTransformer.cpp
        source/Timeline.cpp
        source/Utilities.cpp
        ${schema_source})

coda_add_tests(
    MODULE_NAME six.sicd
    DIRECTORY "tests"
    DEPS cli-c++
    SOURCES
        derive_output_plane.cpp
        test_add_additional_des.cpp
        test_clone_container.cpp
        test_compare_sicd_meshes.cpp
        test_get_complex_data.cpp
        test_load_from_input_stream.cpp
        test_mesh_polyfit.cpp
        test_mesh_roundtrip.cpp
        test_read_sicd_mesh.cpp
        test_read_sicd_with_extra_des.cpp
        test_read_sicd.cpp
        test_sicd_byte_provider.cpp
        test_sicd_schemata.cpp
        test_streaming_write.cpp
        test_vdp_polyfit.cpp)

coda_add_tests(
    MODULE_NAME six.sicd
    DIRECTORY "unittests"
    UNITTEST
    SOURCES
        test_area_plane.cpp
        test_filling_geo_data.cpp
        test_filling_grid.cpp
        test_filling_pfa.cpp
        test_filling_position.cpp
        test_filling_rgazcomp.cpp
        test_filling_rma.cpp
        test_filling_scpcoa.cpp
        test_get_segment.cpp
        test_projection_polynomial_fitter.cpp
        test_radar_collection.cpp
        test_update_sicd_version.cpp
        test_valid_six.cpp
        test_AMP8I_PHS8I.cpp
        test_utilities.cpp)

# Install the schemas
install(DIRECTORY "conf/schema/"
        DESTINATION "conf/schema/six/")

if (Python_Development_FOUND)
    # Install the Python schemas
    install(DIRECTORY "conf/schema/"
            DESTINATION "${CODA_PYTHON_SITE_PACKAGES}/pysix/schema/")
endif()
