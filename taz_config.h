#ifndef taz_config_h
#define taz_config_h

#ifndef taz_CONFIG_DISABLE_PTR_TAGGING
    #define taz_CONFIG_DISABLE_PTR_TAGGING 0
#endif

#ifndef taz_CONFIG_DISABLE_NAN_TAGGING
    #define taz_CONFIG_DISABLE_NAN_TAGGING 0
#endif

#ifndef taz_CONFIG_DISABLE_COMPUTED_GOTOS
    #define taz_CONFIG_DISABLE_COMPUTED_GOTOS 0
#endif

#ifndef taz_CONFIG_ENABLE_GC_TRACING
    #define taz_CONFIG_ENABLE_GC_TRACING 0
#endif

#ifndef taz_CONFIG_ENABLE_VARIABLE_LENGTH_INSTRUCTIONS
    #define taz_CONFIG_ENABLE_VARIABLE_LENGTH_INSTRUCTIONS 0
#endif

#ifndef taz_CONFIG_GC_FULL_CYCLE_INTERVAL
    #define taz_CONFIG_GC_FULL_CYCLE_INTERVAL 5
#endif

#ifndef taz_CONFIG_GC_STACK_SEGMENT_SIZE
    #define taz_CONFIG_GC_STACK_SEGMENT_SIZE 16
#endif

#endif