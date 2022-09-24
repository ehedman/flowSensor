execute_process(COMMAND date +%s
    OUTPUT_VARIABLE _output OUTPUT_STRIP_TRAILING_WHITESPACE)
file(WRITE rtc.def "#define COMPILE_TIME_EPOCH ${_output}")

