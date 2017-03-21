source "$PATHROOT/autosetup/targets/spec-cpu2006/benchmarks.inc" # BENCHMARKS_SPEC

: ${INSTANCES=baseline-lto midfat dummy dummy-sfi}
: ${INSTANCESUFFIX=}
: ${JOBSMAX=16}
: ${TARGETS=spec-cpu2006}
