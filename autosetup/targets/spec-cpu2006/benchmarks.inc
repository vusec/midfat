BENCHMARKS_SPEC_C="
	401.bzip2
	429.mcf
	433.milc
	445.gobmk
	456.hmmer
	458.sjeng
	462.libquantum
	464.h264ref
	470.lbm
	482.sphinx3
	"
BENCHMARKS_SPEC_CPP="
	444.namd
	453.povray
	471.omnetpp
	473.astar
	"

# not currently working:
#	400.perlbench
#	403.gcc
#	447.dealII
#	450.soplex
#	483.xalancbmk

: ${BENCHMARKS_SPEC:="$BENCHMARKS_SPEC_C $BENCHMARKS_SPEC_CPP"}
