find_package(protobuf REQUIRED)
find_package(gRPC REQUIRED)

set(PROTO_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/protocpp)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

set(GENERATED_SRCS "")
set(GENERATED_HDRS "")

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

    set(PB_CC ${PROTO_GEN_DIR}/${PROTO_NAME}.pb.cc)
    set(PB_H  ${PROTO_GEN_DIR}/${PROTO_NAME}.pb.h)
    set(GRPC_CC ${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.cc)
    set(GRPC_H  ${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.h)

    add_custom_command(
        OUTPUT ${PB_CC} ${PB_H} ${GRPC_CC} ${GRPC_H}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
            --cpp_out=${PROTO_GEN_DIR}
            --grpc_out=${PROTO_GEN_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating gRPC & Protobuf from ${PROTO_FILE}"
        VERBATIM
    )

    list(APPEND GENERATED_SRCS ${PB_CC} ${GRPC_CC})
    list(APPEND GENERATED_HDRS ${PB_H} ${GRPC_H})

endforeach()

add_custom_target(generate_proto ALL
    DEPENDS ${GENERATED_SRCS} ${GENERATED_HDRS}
)