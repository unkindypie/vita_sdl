if(NOT DEFINED TEST_OUTPUT)
  message(FATAL_ERROR "TEST_OUTPUT required")
endif()
file(MAKE_DIRECTORY "${TEST_OUTPUT}")
set(stage "${TEST_OUTPUT}/stages.log")
set(fake "${TEST_OUTPUT}/fake-emulator.sh")
set(runner "${CMAKE_CURRENT_LIST_DIR}/../tools/run_vita3k_finite.sh")
# Old successful sessions must not satisfy a newly failed guest run.
file(WRITE "${stage}" "SMOKE_OK old\nSHUTDOWN_OK old\n")
execute_process(COMMAND sh "${runner}" "${stage}" sh -c "exit 0"
  RESULT_VARIABLE failed TIMEOUT 10 OUTPUT_QUIET ERROR_QUIET)
if(failed EQUAL 0)
  message(FATAL_ERROR "Accepted stale guest completion")
endif()
file(WRITE "${fake}" "#!/bin/sh\nprintf 'PROFILE frames=180\nSMOKE_OK new\nSHUTDOWN_OK new\n' >> \"$1\"\nexec sleep 30\n")
execute_process(COMMAND sh "${runner}" "${stage}" sh "${fake}" "${stage}"
  RESULT_VARIABLE success TIMEOUT 10 OUTPUT_QUIET ERROR_QUIET)
if(NOT success EQUAL 0)
  message(FATAL_ERROR "Failed to finish successful finite guest: ${success}")
endif()
# SDL exits instead of returning to a library; its final markers still count.
file(WRITE "${fake}" "#!/bin/sh\nprintf 'SMOKE_OK immediate\nSHUTDOWN_OK immediate\n' >> \"$1\"\n")
execute_process(COMMAND sh "${runner}" "${stage}" sh "${fake}" "${stage}"
  RESULT_VARIABLE immediate TIMEOUT 10 OUTPUT_QUIET ERROR_QUIET)
if(NOT immediate EQUAL 0)
  message(FATAL_ERROR "Rejected clean immediate emulator exit: ${immediate}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env VITA3K_BENCH_TIMEOUT=0
  sh "${runner}" "${stage}" sleep 30 RESULT_VARIABLE timed_out
  TIMEOUT 10 OUTPUT_QUIET ERROR_QUIET)
if(timed_out EQUAL 0)
  message(FATAL_ERROR "Accepted benchmark timeout")
endif()
