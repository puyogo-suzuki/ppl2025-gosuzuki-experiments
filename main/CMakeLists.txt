idf_component_register(SRCS "xorshift32.c" "esp32-protection.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_tee ulp esp_timer)
set(ulp_exp_dep_srcs "esp32-protection.c")
set(ulp_app_name ulp_${COMPONENT_NAME})
set(ulp_sources "ulp/main.c")
ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}")