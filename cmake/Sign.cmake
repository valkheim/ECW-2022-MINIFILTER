
set(WINSDK ${WDK_ROOT}/bin/${WDK_VERSION}/${WDK_PLATFORM}/)
set(MAKECERT "${WINSDK}makecert.exe")
set(CERTUTIL "certutil.exe")
set(CERTMGR "certmgr.exe")
set(CERT2SPC "cert2spc.exe")
set(PVK2PFX "${WINSDK}pvk2pfx.exe")
set(SIGNTOOL "${WINSDK}signtool.exe")
set(OPENSSL "openssl.exe")

set(CERTIFICATE_NAME certificate)
set(COMPANY "Contoso Very Ltd")
set(CERTIFICATE_PATH ${CMAKE_CURRENT_BINARY_DIR})
set(TIMESTAMP_SERVER http://timestamp.digicert.com)
set(COMPILATION_DATE "")
TODAY(COMPILATION_DATE)
message(STATUS "Compilation date: ${COMPILATION_DATE}")

#add_custom_command(OUTPUT ${CERTIFICATE_NAME}.pfx
#    COMMAND "${CMAKE_COMMAND}" -E remove ${CERTIFICATE_NAME}.pvk ${CERTIFICATE_NAME}.cer ${CERTIFICATE_NAME}.pfx ${CERTIFICATE_NAME}.spc
#    COMMAND "${MAKECERT}" -b ${COMPILATION_DATE} -r -n \"CN=${COMPANY}\" -sv ${CERTIFICATE_NAME}.pvk ${CERTIFICATE_NAME}.cer
#    COMMAND "${CERTMGR}" -add ${CERTIFICATE_NAME}.cer -s -r currentUser ROOT
#    COMMAND "${CERTMGR}" -add ${CERTIFICATE_NAME}.cer -s -r currentUser TRUSTEDPUBLISHER
#    COMMAND "${CERT2SPC}" ${CERTIFICATE_NAME}.cer ${CERTIFICATE_NAME}.spc
#    COMMAND "${PVK2PFX}" -pvk ${CERTIFICATE_NAME}.pvk -spc ${CERTIFICATE_NAME}.spc -pfx ${CERTIFICATE_NAME}.pfx
#    WORKING_DIRECTORY "${CERTIFICATE_PATH}"
#    COMMENT "Generating SSL certificates to sign the drivers and executable ..."
#)

add_custom_command(OUTPUT ${CERTIFICATE_NAME}.pfx
    COMMAND "${OPENSSL}" req -x509 -nodes -newkey rsa:2048 -keyout ${CERTIFICATE_NAME}.key -out ${CERTIFICATE_NAME}.crt -days 90 -subj "/CN=${COMPANY}"
    COMMAND "${OPENSSL}" pkcs12 -passout pass: -export -out ${CERTIFICATE_NAME}.pfx -inkey ${CERTIFICATE_NAME}.key -in ${CERTIFICATE_NAME}.crt
    COMMENT "Generating SSL certificates to sign the drivers and executable ..."
)

add_custom_target(make_certificate DEPENDS ${CERTIFICATE_NAME}.pfx)

function(sign_with_timestamp target)
    add_dependencies(${target} make_certificate)

    add_custom_command(TARGET ${target}
        COMMAND "${SIGNTOOL}" sign /v /fd SHA256 /f "${CERTIFICATE_PATH}/${CERTIFICATE_NAME}.pfx" /t ${TIMESTAMP_SERVER} $<TARGET_FILE:${target}>
        COMMENT "Signing $<TARGET_FILE:${target}>..."
    )
endfunction()

function(sign_without_timestamp target)
    add_dependencies(${target} make_certificate)

    add_custom_command(TARGET ${target}
        COMMAND "${SIGNTOOL}" sign /debug /v /fd SHA256 /f "${CERTIFICATE_PATH}/${CERTIFICATE_NAME}.pfx" $<TARGET_FILE:${target}>
        COMMENT "Signing $<TARGET_FILE:${target}>..."
    )
endfunction()

function(verify_signature target)
    add_custom_command(TARGET ${target}
        COMMAND "${CERTUTIL}" -dump "${CERTIFICATE_PATH}/${CERTIFICATE_NAME}.pfx"
        COMMAND "${SIGNTOOL}" verify /v $<TARGET_FILE:${target}>
    )
endfunction()